/**
 *  @file bench_worst_order.cpp
 *  Allocates a number of `TestObject`s on the heap and deallocates
 *  them in an order which causes `LinkedPool` to work extra.
 *  Allocation and deallocation is done with both `new/delete`, `LinkedPools`,
 *  `MemoryPool` and `boost::object_pool`.
 *  @par
 *  A command line argument can be passed to set the number of `TestObject`s
 *  that will be created and destroyed.
 *  @par
 *  The results will be written to a file called **worst_time_taken.output** and
 *  it will be of the form: <br>
 *    ```
 *    Allocating <ARG * 170> objects.
 *    Allocate TestObject normally: X ms
 *    Deallocate TestObject normally: Y ms
 *    Allocate TestObject with LinkedPool: X1 ms
 *    Deallocate TestObject with LinkedPool: Y1 ms
 *    ...
 *    ```
 */

#include <vector>

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
 *  Allocate and deallocate **poolSize * mult** of `TestObject`s
 *  by using a pool allocator.
 *  The deallocation sequence is chosen as follows: deallocate the i-th slot
 *  in every subpool of size `poolSize` for all i = [0, n].
 *  @par
 *  The reason why this is slow for `LinkedPool` is because it will potentially
 *  generate lots of page faults.
 *  @note
 *  The allocation and deallocation times are recorded and written to a file.
 *  @tparam T the type of the pool allocator
 *  @param bound the number of (de)allocations
 *  @param f the file in which the output is written
 *  @param poolSize the size of a subpool of `LinkedPool`
 *  @param mult a value which is multiplied with `poolSize`
 *  @param name the name of the pool allocator which (de)allocates `TestObject`s
 *  @see LinkedPool
 *  @see TestObject
 */
template<template <typename> class T>
void benchPool(size_t bound, std::ofstream& f, size_t poolSize,
               size_t mult, const std::string& name) {
    T<TestObject> lp;
    vector<TestObject*> objs(bound);
    std::clock_t start = std::clock();
    for (size_t i = 0; i < bound; ++i) {
        objs[i] = reinterpret_cast<TestObject*>(lp.allocate());
    }
    printToFile(f, "TestObject", start, false,  name);

    start = std::clock();
    for (size_t i = 0; i < poolSize; ++i) {
        for (size_t offset = 0; offset < mult; ++offset) {
            lp.deallocate(objs[i + offset * poolSize]);
        }
    }
    printToFile(f, "TestObject", start, true, name);
}

#ifdef INCLUDE_BOOST
template<template <typename, typename> class T>
void benchPool(size_t bound, std::ofstream& f, size_t poolSize,
               size_t mult, const std::string& name) {
    T<TestObject, boost::default_user_allocator_malloc_free> lp;
    vector<TestObject*> objs(bound);
    std::clock_t start = std::clock();
    for (size_t i = 0; i < bound; ++i) {
        objs[i] = reinterpret_cast<TestObject*>(lp.malloc());
    }
    printToFile(f, "TestObject", start, false,  name);

    start = std::clock();
    for (size_t i = 0; i < poolSize; ++i) {
        for (size_t offset = 0; offset < mult; ++offset) {
            lp.free(objs[i + offset * poolSize]);
        }
    }
    printToFile(f, "TestObject", start, true, name);
}
#endif

int main(int argc, char* argv[]) {
    size_t MULT = argc > 1 ? std::stoul(argv[1]) : 10000;
    // every type of linked pool will provide the same pool size
    size_t POOL_SIZE = LinkedPool<TestObject>().getPoolSize();
    size_t BOUND = POOL_SIZE * MULT;

    std::ofstream f("worst_time_taken.output");
    f << "Allocating " << BOUND << " objects." << std::endl;
    {
        vector<TestObject*> objs(BOUND);
        std::clock_t start = std::clock();
        for (size_t i = 0; i < BOUND; ++i) {
            objs[i] = new TestObject();
        }
        printToFile(f, "TestObject", start, false, "Regular");

        start = std::clock();
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            for (size_t offset = 0; offset < MULT; ++offset) {
                delete (objs[i + offset * POOL_SIZE]);
            }
        }
        printToFile(f, "TestObject", start, true, "Regular");
    }
    {
        benchPool<LinkedPool>(BOUND, f, POOL_SIZE, MULT, "LinkedPool");
    }
    {
        benchPool<LinkedPool3>(BOUND, f, POOL_SIZE, MULT, "LinkedPool3");
    }
    {
        benchPool<MemoryPool>(BOUND, f, POOL_SIZE, MULT, "MemoryPool");
    }
#ifdef INCLUDE_BOOST
    {
        benchPool<boost::object_pool>(BOUND, f, POOL_SIZE, MULT, "boost::object_pool");
    }
#endif
}