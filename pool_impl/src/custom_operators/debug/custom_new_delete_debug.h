#ifndef __CUSTOM_NEW_DELETE_DEBUG_H__
#define __CUSTOM_NEW_DELETE_DEBUG_H__

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>

#include "tools/mallocator.h"
#include "tools/FreeDeleter.h"
#include "tools/AllocCollector.h"
#include "pool_allocators/GlobalLinkedPool.h"

namespace {
    using efficient_pools::GlobalLinkedPool;

    const size_t __threshold = 128;

    const size_t __usablePoolSize = GlobalLinkedPool::PAGE_SIZE -
                                    sizeof(PoolHeaderG);

    const size_t __mallocOverhead = 8;
    const bool __useOnlyMalloc = false;

    const size_t __mod = sizeof(void*) - 1;
    const size_t __logOfVoid = std::log2(sizeof(void*));

    std::vector<std::unique_ptr<GlobalLinkedPool, FreeDeleter<GlobalLinkedPool>>,
                mallocator<std::unique_ptr<GlobalLinkedPool>>>
        __allocators(__threshold >> __logOfVoid);
    // the pointers that have been allocated with malloc and
    // their respective sizes
    std::set<std::pair<void*, size_t>,
             std::less<std::pair<void*, size_t>>,
             mallocator<std::pair<void*, size_t>>> __mallocs;
    AllocCollector ac;
}

inline size_t getAllocatorsIndex(size_t size) {
    return (size >> __logOfVoid) - 1;
}

inline void* custom_new_no_throw(size_t size) {
    ac.addObject(size);
    // use malloc for large sizes
    if (__useOnlyMalloc || size > __threshold) {
        void* toRet = std::malloc(size == 0 ? 1 : size);
        __mallocs.insert(std::make_pair(toRet, size));
        ac.addAllocation(size);
        ac.addOverhead(__mallocOverhead);
        return toRet;
    } else {
        size_t remainder = size & __mod; // size % sizeof(void*)
        // get the next multiple of size(void*)
        size = remainder == 0 ? size : (size + __mod) & ~__mod;
        auto& poolAlloc = __allocators[getAllocatorsIndex(size)];
        if (poolAlloc) {
            size_t poolSize = poolAlloc->getNumberOfPools();
            // our pool was already created, just use it
            void* toRet = poolAlloc->allocate();
            if (poolAlloc->getNumberOfPools() > poolSize) {
                ac.addAllocation(__usablePoolSize);
                ac.addOverhead(sizeof(PoolHeaderG));
            }
            return toRet;
        } else {
            // create the pool which can hold objects of size
            // <size>
            poolAlloc.reset(
                new (malloc(sizeof(GlobalLinkedPool))) GlobalLinkedPool(size)
            );
            ac.addAllocation(__usablePoolSize);
            ac.addOverhead(sizeof(PoolHeaderG));
            return poolAlloc->allocate();
        }
    }
}

inline void* custom_new(size_t size) {
    void* toRet = custom_new_no_throw(size);
    if (toRet == nullptr) {
        throw std::bad_alloc();
    }
    return toRet;
}

inline void custom_delete(void* ptr) throw() {
    const PoolHeaderG& ph = GlobalLinkedPool::getPoolHeader(ptr);

    // find out if the pointer was allocated with malloc
    auto bigAlloc = std::find_if(__mallocs.begin(), __mallocs.end(),
                                 [&ptr](const std::pair<void*, size_t>& p){
                                     return (size_t*)p.first == (size_t*)ptr;
                                 });
    if (bigAlloc != __mallocs.end()) {
        __mallocs.erase(bigAlloc);
        std::free(ptr);
        ac.removeObject(bigAlloc->second);
        ac.removeAllocation(bigAlloc->second);
        ac.removeOverhead(__mallocOverhead);
    }
    else {
        // convert the size to an index of the allocators vector
        // by dividing it to sizeof(void*)
        auto pool =__allocators[getAllocatorsIndex(ph.sizeOfSlot)].get();
        size_t numOfPools = pool->getNumberOfPools();
        pool->deallocate(ptr);
        if (numOfPools > pool->getNumberOfPools()) {
            ac.removeObject(ph.sizeOfSlot);
            ac.removeAllocation(__usablePoolSize);
            ac.removeOverhead(sizeof(PoolHeaderG));
        }
    }
}

#endif // __CUSTOM_NEW_DELETE_DEBUG_H__
