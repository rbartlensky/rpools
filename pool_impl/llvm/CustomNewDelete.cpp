#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <algorithm>
#include <vector>
#include <map>

#include "common.h"

using namespace llvm;
using namespace legacy;
using namespace custom_pass;
using std::vector;
using std::map;
using std::find;

namespace {
/**
 *  Change all occurences of `operator new` with `custom_new` and all occurences
 *  of `operator delete` with `custom_delete`
 *  @note All versions of operator new and delete are considered.
 */
struct CustomNewDelete : public BasicBlockPass {
  static char ID;
  /** Mangled custom_new function name. */
  static const StringRef CUSTOM_NEW_NAME;
  /** Mangled custom_new_no_throw function name. */
  static const StringRef CUSTOM_NEW_NO_THROW_NAME;
  /** Mangled custom_delete function name. */
  static const StringRef CUSTOM_DELETE_NAME;
  /** The declaration of custom_new inside of the module. */
  static Function* CUSTOM_NEW_FUNC;
  /** The declaration of custom_new_no_throw inside of the module. */
  static Function* CUSTOM_NEW_NO_THROW_FUNC;
  /** The declaration of custom_delete inside of the module. */
  static Function* CUSTOM_DELETE_FUNC;
  /** A mapping from operator news to their `custom_new` correspondent. */
  static const map<StringRef, Function**> OP_TO_CUSTOM;

  CustomNewDelete() : BasicBlockPass(ID) {}

  /**
   *  Insert a call to `custom_new` after each `operator new` call and
   *  change all `operator delete` calls to `custom_delete`.
   *  @param bb the BasicBlock in which the call to custom new and
   *            delete are inserted
   *  @param insts the vector in which all calls to operator new are stored
   *  @note InvokeInst are considered as well.
   */
  void addCustomNewAndDeleteCalls(BasicBlock& bb,
                                  std::vector<Instruction*>& insts) {
    for (auto& inst : bb) {
      if (isa<CallInst>(inst)) {
        CallInst& ci = cast<CallInst>(inst);
        auto func = ci.getCalledFunction();
        if (func) {
          StringRef name = getDemangledName(func->getName());
          IRBuilder<> builder(&ci);
          if (isNew(name)) {
            // replace the call to operator new with custom_new
            // but make sure the first argument of operator new
            // is copied into custom new as well!
            ci.replaceAllUsesWith(
              builder.CreateCall(*OP_TO_CUSTOM.at(name),
                                 { ci.getOperand(0), builder.getInt64(0) })
            );
            // save call instruction to remove it later
            insts.push_back(&ci);
          } else if (isDelete(name)) {
            // we are processing an operator delete call
            // because custom_delete and operator delete take the same
            // type and number of arguments, we can just change the
            // function called in this case call custom_delete
            ci.setCalledFunction(CUSTOM_DELETE_FUNC);
          }
        }
      } else if (isa<InvokeInst>(inst)) {
          InvokeInst& ii = cast<InvokeInst>(inst);
          auto func = ii.getCalledFunction();
          if (func) {
            StringRef name = getDemangledName(func->getName());
            IRBuilder<> builder(&ii);
            if (isNew(name)) {
              InvokeInst* customNewInvoke =
                // InvokeInsts seem to not hold the type, therefore we will
                // assume the largest alignment possible
                builder.CreateInvoke(*OP_TO_CUSTOM.at(name),
                                     ii.getNormalDest(),
                                     ii.getUnwindDest(),
                                     { ii.getOperand(0),
                                       builder.getInt64(alignof(max_align_t)) });
              AttributeSet attrs;
              attrs.addAttribute(ii.getContext(), 0, Attribute::NoAlias);
              customNewInvoke->setAttributes(attrs);
              ii.replaceAllUsesWith(customNewInvoke);
              insts.push_back(&ii);
            } else if (isDelete(name)) {
              ii.setCalledFunction(CUSTOM_DELETE_FUNC);
          }
        }
      }
    }
  }

  using BasicBlockPass::doInitialization;
  bool doInitialization(Module& mod) override {
    LLVMContext& context = mod.getContext();

    // custom_new type definition: void* custom_new(size_t, size_t)
    FunctionType* customNewType = FunctionType::get(
      Type::getInt8PtrTy(context),
      { Type::getInt64Ty(context), Type::getInt64Ty(context) },
      false
    );
    // add the declaration of custom_new into the module
    // because it will be linked later
    mod.getOrInsertFunction(CUSTOM_NEW_NAME, customNewType);
    CUSTOM_NEW_FUNC = mod.getFunction(CUSTOM_NEW_NAME);

    // reuse customNewType because custom_new_no_throw has the same signature
    mod.getOrInsertFunction(CUSTOM_NEW_NO_THROW_NAME, customNewType);
    CUSTOM_NEW_NO_THROW_FUNC = mod.getFunction(CUSTOM_NEW_NO_THROW_NAME);

    // custom_delete type definition: void custom_delete(void*)
    FunctionType* customDeleteType = FunctionType::get(
      Type::getInt8PtrTy(context),
      { Type::getVoidTy(context) },
      false
    );
    // same for custom_delete
    mod.getOrInsertFunction(CUSTOM_DELETE_NAME, customDeleteType);
    CUSTOM_DELETE_FUNC = mod.getFunction(CUSTOM_DELETE_NAME);
    return true;
  }

  bool runOnBasicBlock(BasicBlock& bb) override {
    Module* mod = bb.getParent()->getParent();
    const DataLayout& dataLayout = mod->getDataLayout();
    // all calls to operator new will be saved in this vector
    std::vector<Instruction*> insts;
    addCustomNewAndDeleteCalls(bb, insts);
    // remove all calls to operator new/delete
    while (insts.size() > 0) {
      insts.back()->removeFromParent();
      insts.pop_back();
    }

    IRBuilder<> builder(mod->getContext());
    for (auto& inst : bb) {
      if (isa<CallInst>(inst)) {
        CallInst& ci = cast<CallInst>(inst);
        auto func = ci.getCalledFunction();
        if (func) {
          StringRef name = func->getName();
          if (name == CUSTOM_NEW_NAME) {
            Instruction* nextInst = inst.getNextNode();
            // processing a custom_new means that the next instruction
            // is a bitcast which will give us the alignment
            if (nextInst && isa<BitCastInst>(*nextInst)) {
              BitCastInst& bci = cast<BitCastInst>(*nextInst);
              PointerType& pt = cast<PointerType>(*bci.getDestTy());
              Type* type = pt.getPointerElementType();
              size_t alignment = dataLayout.getPrefTypeAlignment(type);
              ci.setOperand(1, builder.getInt64(alignment));
            } else {
              // if the BitCastInst was not present, we will choose
              // the largest alignment possible
              ci.setOperand(1, builder.getInt64(alignof(max_align_t)));
            }
          }
        }
      }
    }
    return true;
  }
}; // end of struct CustomNewDelete
}  // end of anonymous namespace

char CustomNewDelete::ID = 0;
const StringRef CustomNewDelete::CUSTOM_NEW_NAME = "_Z10custom_newmm";
const StringRef CustomNewDelete::CUSTOM_NEW_NO_THROW_NAME =
  "_Z19custom_new_no_throwmm";
const StringRef CustomNewDelete::CUSTOM_DELETE_NAME = "_Z13custom_deletePv";

Function* CustomNewDelete::CUSTOM_NEW_FUNC = nullptr;
Function* CustomNewDelete::CUSTOM_NEW_NO_THROW_FUNC = nullptr;
Function* CustomNewDelete::CUSTOM_DELETE_FUNC = nullptr;

const map<StringRef, Function**> CustomNewDelete::OP_TO_CUSTOM = {
  { NEW_OPS[0], &CUSTOM_NEW_FUNC },
  { NEW_OPS[1], &CUSTOM_NEW_FUNC },
  { NEW_NO_THROW_OPS[0], &CUSTOM_NEW_NO_THROW_FUNC },
  { NEW_NO_THROW_OPS[1], &CUSTOM_NEW_NO_THROW_FUNC }
};

static RegisterPass<CustomNewDelete> X("custom new delete",
                                       "CustomNewDelete Pass",
                                       false /* Only looks at CFG */,
                                       false /* Analysis Pass */);

static void registerMyPass(const PassManagerBuilder &,
                           PassManagerBase &PM) {
  PM.add(new CustomNewDelete());
}
static RegisterStandardPasses
RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
               registerMyPass);
