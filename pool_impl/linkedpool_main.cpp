#include "src/linked_pool/LinkedPool.h"
#include <vector>
using std::vector;

#include <iostream>
using std::cout;
using std::endl;
using efficient_pools::LinkedPool;


struct Test {
    Test() = default;
    ~Test() = default;
    int x, y, z;

    void setX(int x2) {
        x = x2;
    }

    int getX() {
        return x;
    }

    virtual void print() {
        cout << "Hello Test" << endl;
    }
};

int main(int agrc, char* argv[]) {
    size_t BOUND = 100;
    LinkedPool<Test> pool = LinkedPool<Test>();
    vector<Test*> objs;
    objs.reserve(BOUND);
    for (int i = 0; i < BOUND; ++i) {
        objs.push_back((Test*) pool.allocate());
    }
    for (int i = 0; i < BOUND; ++i) {
        pool.deallocate(objs.back());
        objs.pop_back();
    }
    return 0;
}
