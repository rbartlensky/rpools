#ifndef __OBJ2_H__
#define __OBJ2_H__

#include <cstdlib>
#include <vector>

class Obj2 {
public:

    static size_t overhead, POOL_SIZE, METADATA_SIZE;

    static size_t calculateOverhead();

    static std::pair<void*, size_t> getPool(void* ptr);

    static std::vector<void*> pools;

    static size_t getFreeCount(void* ptr);

    static void* nextFree(void* ptr);

    Obj2() = default;

    ~Obj2() = default;

    static void* operator new(size_t size);

    static void operator delete(void* ptr);

private:
    // 12 bytes
    int _x;
    int _y;
    int _z;
};

#endif // __OBJ2_H__