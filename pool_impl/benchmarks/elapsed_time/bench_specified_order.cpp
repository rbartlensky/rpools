/**
 *  @file bench_specified_order.cpp
 *  Allocates and deallocates a number of `TestObject`s on the heap in a certain
 *  order.
 *  Allocation and deallocation is done with `new/delete`, `LinkedPools`,
 *  `MemoryPool`, and `boost::object_pool`.
 *  @par
 *  A command line argument can be passed to set the number of `TestObject`s
 *  that will be created and destroyed.
 *  @par
 *  The results will be written to a file called **specified_time_taken.output** and
 *  it will be of the form: <br>
 *    ```
 *    Allocating <ARG> objects.
 *    Allocate TestObject normally: X ms
 *    Deallocate TestObject normally: Y ms
 *    Allocate TestObject with LinkedPool: X1 ms
 *    Deallocate TestObject with LinkedPool: Y1 ms
 *    ...
 *    ```
 */

#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

#include "Utility.h"
#include "unit_test/TestObject.h"
#ifdef INCLUDE_BOOST
#include <boost/pool/object_pool.hpp>
#endif
#include "pool_allocators/MemoryPool.h"
#include "pool_allocators/LinkedPool.h"
#include "pool_allocators/LinkedPool3.h"

using efficient_pools::LinkedPool;
using efficient_pools3::LinkedPool3;
using std::vector;

/**
 *  Allocates `num` `TestObject`s using `new` which are stored in `vec`.
 *  @param num the number of objects that are allocated
 *  @param vec the vector in which the allocations are pushed_back
 *  @return The number of ms it took to allocate `num` `TestObject`s.
 */
float allocateN(size_t num, vector<TestObject*>& vec) {
    std::clock_t start = std::clock();
    for (size_t i = 0; i < num; ++i) {
        vec.push_back(new TestObject());
    }
    return (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
}

/**
 *  Deallocates `num` `TestObject`s using `delete` which are stored in `vec`.
 *  @param num the number of objects that are deallocated
 *  @param vec the vector from which the deallocated objects are poped_back
 *  @return The number of ms it took to deallocate `num` `TestObject`s.
 */
float deallocateN(size_t num, vector<TestObject*>& vec) {
    std::clock_t start = std::clock();
    for (size_t i = 0; i < num; ++i) {
        delete vec.back();
        vec.pop_back();
    }
    return (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
}

/**
 *  Allocates `num` `TestObject`s using a pool allocator which
 *  are stored in `vec`.
 *  @tparam T the type of the pool allocator
 *  @param num the number of objects that are allocated
 *  @param vec the vector in which the allocations are pushed_back
 *  @param lp the pool allocator
 *  @return The number of ms it took to allocate `num` `TestObject`s.
 */
template<template <typename> class T>
float allocateN(size_t num, vector<TestObject*>& vec, T<TestObject>& lp) {
    std::clock_t start = std::clock();
    for (size_t i = 0; i < num; ++i) {
        vec.push_back((TestObject*) lp.allocate());
    }
    return (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
}

/**
 *  Deallocates `num` `TestObject`s using a pool allocator which are stored in
 *  `vec`.
 *  @tparam T the type of the pool allocator
 *  @param num the number of objects that are allocated
 *  @param vec the vector from which the deallocated objects are poped_back
 *  @param lp the pool allocator
 *  @return The number of ms it took to deallocate `num` `TestObject`s.
 */
template<template <typename> class T>
float deallocateN(size_t num, vector<TestObject*>& vec, T<TestObject>& lp) {
    std::clock_t start = std::clock();
    for (size_t i = 0; i < num; ++i) {
        lp.deallocate(vec.back());
        vec.pop_back();
    }
    return (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
}

/**
 *  Allocate and deallocate a number of `TestObject`s by using a pool allocator
 *  in a certain order.
 *  @note
 *  The allocation and deallocation times are recorded and written to a file.
 *  @tparam T the type of the pool allocator
 *  @param bound the number of (de)allocations
 *  @param f the file in which the output is written
 *  @param five 5% of `bound`
 *  @param ten 10% of `bound`
 *  @param name the name of the pool allocator which (de)allocates `TestObject`s
 */
template<template <typename> class T>
void benchPool(size_t bound, std::ofstream& f,
               size_t five, size_t ten,
               const std::string& name) {
    T<TestObject> lp;
    vector<TestObject*> objs;
    objs.reserve(bound);
    float alloc = 0.0f;
    float dealloc = 0.0f;

    alloc += allocateN(ten, objs, lp);
    dealloc += deallocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);

    alloc += allocateN(ten, objs, lp);
    dealloc += deallocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);

    dealloc += deallocateN(ten, objs, lp);
    alloc += allocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);

    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(ten, objs, lp);

    printToFile2(f, "TestObject", alloc, false, name);
    printToFile2(f, "TestObject", dealloc, true, name);
}

#ifdef INCLUDE_BOOST
template<template <typename, typename> class T>
float allocateN(size_t num, vector<TestObject*>& vec,
                T<TestObject, boost::default_user_allocator_malloc_free>& lp) {
    std::clock_t start = std::clock();
    for (size_t i = 0; i < num; ++i) {
        vec.push_back((TestObject*) lp.malloc());
    }
    return (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
}

template<template <typename, typename> class T>
float deallocateN(size_t num, vector<TestObject*>& vec,
                  T<TestObject, boost::default_user_allocator_malloc_free>& lp) {
    std::clock_t start = std::clock();
    for (size_t i = 0; i < num; ++i) {
        lp.free(vec.back());
        vec.pop_back();
    }
    return (std::clock() - start) / (double)(CLOCKS_PER_SEC / 1000);
}

template<template <typename, typename> class T>
void benchPool(size_t BOUND, std::ofstream& f,
               size_t five, size_t ten,
               const std::string& name) {
    T<TestObject, boost::default_user_allocator_malloc_free> lp;
    vector<TestObject*> objs;

    float alloc = 0.0f;
    float dealloc = 0.0f;

    alloc += allocateN(ten, objs, lp);
    dealloc += deallocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);

    alloc += allocateN(ten, objs, lp);
    dealloc += deallocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);

    dealloc += deallocateN(ten, objs, lp);
    alloc += allocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);

    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(five, objs, lp);
    alloc += allocateN(five, objs, lp);
    dealloc += deallocateN(ten, objs, lp);

    printToFile2(f, "TestObject", alloc, false, name);
    printToFile2(f, "TestObject", dealloc, true, name);
}
#endif

int main(int argc, char *argv[]) {
    size_t BOUND = argc > 1 ? std::stoul(argv[1]) : 10000;

    std::ofstream f("specified_time_taken.output");

    size_t five = BOUND * 5 / 100; // 5%
    size_t ten = BOUND / 10; // 10%
    f << "Allocating " << BOUND << " objects." << std::endl;
    {
        vector<TestObject*> objs;
        objs.reserve(BOUND);
        float alloc = 0.0f;
        float dealloc = 0.0f;

        alloc += allocateN(ten, objs);
        dealloc += deallocateN(five, objs);
        alloc += allocateN(five, objs);
        dealloc += deallocateN(five, objs);

        alloc += allocateN(ten, objs);
        dealloc += deallocateN(five, objs);
        alloc += allocateN(five, objs);
        dealloc += deallocateN(five, objs);

        dealloc += deallocateN(ten, objs);
        alloc += allocateN(five, objs);
        alloc += allocateN(five, objs);
        dealloc += deallocateN(five, objs);

        alloc += allocateN(five, objs);
        dealloc += deallocateN(five, objs);
        alloc += allocateN(five, objs);
        dealloc += deallocateN(ten, objs);

        printToFile2(f, "TestObject", alloc, false, "Regular");
        printToFile2(f, "TestObject", dealloc, true, "Regular");
    }
    {
        benchPool<LinkedPool>(BOUND, f, five, ten, "LinkedPool");
    }
    {
        benchPool<LinkedPool3>(BOUND, f, five, ten, "LinkedPool3");
    }
    {
        benchPool<MemoryPool>(BOUND, f, five, ten, "MemoryPool");
    }
#ifdef INCLUDE_BOOST
    {
        benchPool<boost::object_pool>(BOUND, f, five, ten, "boost::object_pool");
    }
#endif
    return 0;
}