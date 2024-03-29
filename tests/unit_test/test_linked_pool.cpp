#include "catch.hpp"

#include "TestObject.h"
#include "TestObject2.h"
#include "rpools/allocators/LinkedPool.hpp"
using namespace rpools;

template<typename T>
void test_pool_size() {
    LinkedPool<T> lp;
    size_t expectedSize = (getPageSize() - sizeof(PoolHeader)) / sizeof(T);
    REQUIRE(lp.getPoolSize() == expectedSize);
}

TEST_CASE("Pool size of LinkedPool is correct for different objects",
          "[LinkedPool]") {
    SECTION("Pool size for TestObject") {
        test_pool_size<TestObject>();
    }
    SECTION("Pool size for TestObject2") {
        test_pool_size<TestObject2>();
    }
}

#include <vector>
using std::vector;

template<typename T>
void test_allocation_1() {
    LinkedPool<T> lp;
    size_t size = lp.getPoolSize();
    vector<T*> objs(size);
    objs[0] = new (lp.allocate()) T();
    for (size_t i = 1; i < size; ++i) {
        objs[i] = new (lp.allocate()) T();
        // all objects are part of the same pool
        // so the 3rd object will be 2 slots away from
        // the first one and so on for all objects
        REQUIRE(objs[i] == objs[0] + i);
    }
    // clean up
    for (auto obj : objs) {
        lp.deallocate(obj);
    }
}

// where P is LinkedPool.getPoolSize()
TEST_CASE("Allocating P objects returns correct pointers",
          "[LinkedPool]") {
    SECTION("Allocate TestObjects") {
        test_allocation_1<TestObject>();
    }
    SECTION("Allocate TestObject2s") {
        test_allocation_1<TestObject2>();
    }
}

template<typename T>
void test_allocation_2() {
    LinkedPool<T> lp;
    // allocate two more objects which will get
    // allocated in a second pool
    size_t size = lp.getPoolSize() + 2;
    vector<T*> objs(size);
    for (size_t i = 0; i < size; ++i) {
        objs[i] = new (lp.allocate()) T();
    }
    // the (P + 1) object is allocated in another pool
    // because P == lp.getPoolSize()
    // if we mask the address of the first object allocated
    // and the address of the (P + 1) object, we should
    // get that they are different based on the assumption that
    // only P objects fit in one pool
    REQUIRE(((size_t)objs[size - 2] & getPoolMask())
            != ((size_t)objs[0] & getPoolMask()));
    // last 2 elements are from the same pool (i.e. P+1 and P+2)
    REQUIRE(objs[size - 2] + 1 == objs[size - 1]);
    // clean up
    for (auto obj : objs) {
        lp.deallocate(obj);
    }
}

// where P is LinkedPool.getPoolSize()
TEST_CASE("Allocating more than P objects returns correct pointers",
          "[LinkedPool]") {
    SECTION("Allocate TestObjects") {
        test_allocation_2<TestObject>();
    }
    SECTION("Allocate TestObject2s") {
        test_allocation_2<TestObject2>();
    }
}

template<typename T>
void test_interleaving() {
    LinkedPool<T> lp;
    // we will allocate 5 objects
    size_t size = 5;
    vector<T*> objs(size);
    for (size_t i = 0; i < size; ++i) {
        objs[i] = new (lp.allocate()) T();
    }
    auto firstDeallocated = reinterpret_cast<void*>(objs[1]);
    lp.deallocate(objs[1]);
    auto secondDeallocated = reinterpret_cast<void*>(objs[4]);
    lp.deallocate(objs[4]);
    // now we have that head -> 4 -> 1 -> 6
    // so if we allocate something, it will have the address of slot 4
    // which is saved in secondDeallocated
    objs[1] = new (lp.allocate()) T();
    REQUIRE(objs[1] == secondDeallocated);
    // now we have head -> 4 -> 6
    // so if we allocate something, it will have the address of slot 1
    // which is saved in firstDeallocated
    objs[4] = new (lp.allocate()) T();
    REQUIRE(objs[4] == firstDeallocated);
    // now head -> 6 -> ...
    objs.push_back(new (lp.allocate()) T());
    REQUIRE(objs[5] == objs[0] + 5);

    for (size_t i = 0; i < objs.size(); ++i) {
        lp.deallocate(objs[i]);
    }
}

TEST_CASE("(De)allocation sequence produces correct result", "[LinkedPool]") {
    SECTION("TestObject") {
        test_interleaving<TestObject>();
    }
    SECTION("TestObject2") {
        test_interleaving<TestObject2>();
    }
}

template<typename T>
void test_pools_fill_up() {
    LinkedPool<T> lp;
    size_t size = lp.getPoolSize() * 2;
    vector<T*> objs(size);
    for (size_t i = 0; i < size; ++i) {
        objs[i] = new (lp.allocate()) T();
    }
    T* firstDealloc = objs[size - 1];
    lp.deallocate(objs[size - 1]);
    T* secondDealloc = objs[size / 2 - 1];
    lp.deallocate(objs[size / 2 - 1]);
    objs[size - 1] = new (lp.allocate()) T();
    // new allocation should be placed either in the first or second pool
    REQUIRE(((objs[size - 1] == firstDealloc)
             || (objs[size - 1] == secondDealloc)));
    objs[size / 2 - 1] = new (lp.allocate()) T();
    // same for the second allocation.
    // If the first one was placed in pool 1 then this will be placed in pool 2.
    REQUIRE(((objs[size / 2 - 1] == firstDealloc)
             || (objs[size / 2 - 1] == secondDealloc)));
    for (size_t i = 0; i < objs.size(); ++i) {
        lp.deallocate(objs[i]);
    }
}

TEST_CASE("A new pool allocates iff all the other pools are full", "[LinkedPool]") {
    SECTION("TestObject") {
        test_pools_fill_up<TestObject>();
    }
    SECTION("TestObject2") {
        test_pools_fill_up<TestObject2>();
    }
}
