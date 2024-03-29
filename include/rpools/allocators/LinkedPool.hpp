#ifndef __LINKED_POOL_H__
#define __LINKED_POOL_H__

#include <cstdlib>
#include <new>

#include "rpools/allocators/Node.hpp"
#include "rpools/tools/LMLock.hpp"
#include "rpools/tools/pool_utils.hpp"

extern "C" {
#include "rpools/avltree/avl_utils.h"
}

namespace rpools {

using Pool = void*;

/**
 *  Every pool is allocated on a page boundary.
 *  The `PoolHeader` is placed at the first byte of the page and
 *  contains certain metadata about a pool.
 */
struct PoolHeader {
    /** Denotes the number of slots that are occupied. */
    size_t occupiedSlots;
    /** A `Node` which points to the next free slot of the pool, or
     *  to nullptr if there are no slots left. */
    Node head;
};

/**
 *  Represents a pool allocator which tries to minimise the amount of memory
 *  overheads of small objects.
 *  This is done by allocating a number of pages of memory in which objects
 *  will be allocated.
 *  @note When all objects of a page are deallocated, the page is freed.
 *  @tparam T the type of object to store in the pool
 */
template<typename T>
class LinkedPool {
public:

    /**
     *  Creates a `LinkedPool` allocator that will allocate objects of type T
     *  in pools and return pointers to them.
     */
    LinkedPool();

    /**
     *  Allocates space for an object of type T in one of the free slots
     *  and returns a pointer the mmeory location of where the object will be
     *  stored.
     *  @return A pointer to the newly allocated space for T.
     */
    void* allocate();

    /**
     *  Deallocates the memory that is used by the object of type T whose
     *  pointer is supplied.
     *  @param t_ptr a pointer to an object that will be deallocated
     */
    void deallocate(void* t_ptr);

    /**
     *  @return the number of T objects that fit in a page of memory.
     */
    size_t getPoolSize() { return m_poolSize; }

    /**
     *  @warning this is not a constant time operation so use it wisely!
     *  @return the number of pages that are currently allocated
     */
    size_t getNumberOfPools() { return pool_count(&m_freePools); }

private:
    avl_tree m_freePools;
    LMLock m_poolLock;
    size_t m_headerPadding = 0;
    size_t m_slotSize;
    size_t m_poolSize = 0;
    Pool m_freePool = nullptr;

    /**
     *  Creates a `PoolHeader` at **t_ptr**
     *  @param t_ptr the address where the `PoolHeader` is created
     */
    void constructPoolHeader(Pool t_ptr);

    /**
     *  @return a pointer to the next free slot of memory from the given `Pool`.
     */
    void* nextFree(Pool t_ptr);
};

template<typename T>
LinkedPool<T>::LinkedPool()
    : m_freePools(),
      m_poolLock(),
      m_slotSize(sizeof(T) < sizeof(Node) ? sizeof(Node) : sizeof(T)) {
    // make sure the first slot starts at a proper alignment
    size_t diff = mod(sizeof(PoolHeader), alignof(T));
    if (diff != 0) {
        m_headerPadding += alignof(T) - diff;
    }
    // make sure that slots are properly aligned
    diff = mod(m_slotSize, alignof(T));
    if (diff != 0) {
        m_slotSize += alignof(T) - diff;
    }
    m_poolSize = (getPageSize() - sizeof(PoolHeader)) / m_slotSize;
}

template<typename T>
void* LinkedPool<T>::allocate() {
    m_poolLock.lock();
    // use the cached pool to get the next slot
    if (m_freePool) {
        return nextFree(m_freePool);
    } else {
        // look for a page that has a free slot
        Pool pool = pool_first(&m_freePools);
        if (pool) {
            // a page has a free slot
            return nextFree(pool);
        } else {
            // allocate a new page of memory because there are no free pool
            // slots left
            Pool pool = aligned_alloc(getPageSize(), getPageSize());
            constructPoolHeader(pool);
            pool_insert(&m_freePools, pool);
            m_freePool = pool;
            return nextFree(pool);
        }
    }
}

template<typename T>
void LinkedPool<T>::deallocate(void* t_ptr) {
    // get the pool of t_ptr
    auto pool = reinterpret_cast<PoolHeader*>(
        reinterpret_cast<size_t>(t_ptr) & getPoolMask()
    );
    m_poolLock.lock();
    // the last slot was deallocated => free the page
    if (pool->occupiedSlots == 1) {
        pool_remove(&m_freePools, pool);
        free(pool);
        m_freePool = pool_first(&m_freePools);
    } else {
        auto newNodeG = new (t_ptr) Node();
        // update nodes to point to the newly create Node
        Node& head = pool->head;
        newNodeG->next = head.next;
        head.next = newNodeG;
        m_freePool = pool;
        // the pool is not full, therefore add it to the list of pools
        // that have free slots
        if (--pool->occupiedSlots == m_poolSize - 1) {
            pool_insert(&m_freePools, pool);
        }
    }
    m_poolLock.unlock();
}

template<typename T>
void LinkedPool<T>::constructPoolHeader(Pool t_ptr) {
    auto header = new (t_ptr) PoolHeader();
    auto first = reinterpret_cast<char*>(header + 1);
    first += m_headerPadding;
    header->head.next = reinterpret_cast<Node*>(first);
    for (size_t i = 0; i < m_poolSize - 1; ++i) {
        auto node = new (first) Node();
        first += m_slotSize;
        node->next = reinterpret_cast<Node*>(first);
    }
    new (first) Node();
}

template<typename T>
void* LinkedPool<T>::nextFree(Pool t_ptr) {
    auto header = reinterpret_cast<PoolHeader*>(t_ptr);
    Node& head = header->head;
    void* toReturn = head.next;
    if (head.next) {
        head.next = head.next->next;
        // if the pool becomes full, don't consider it in the list
        // of pools that have some free slots
        if (++(header->occupiedSlots) == m_poolSize) {
            pool_remove(&m_freePools, t_ptr);
            m_freePool = pool_first(&m_freePools);
        }
    }
    m_poolLock.unlock();
    return toReturn;
}

}
#endif // __LINKED_POOL_H__
