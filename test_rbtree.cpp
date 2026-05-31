// test_rbtree.cpp — Unit test for MASM x64 Red-Black Tree
// Compile: cl.exe /EHsc /Zi /I. test_rbtree.cpp build-ninja\Debug\RawrXD_RBTree.obj

#include "src/win32app/RawrXD_RBTree_Bridge.h"
#include <stdio.h>
#include <assert.h>

int main() {
    printf("[RawrXD] RB-Tree Unit Test\n");
    printf("==========================\n\n");

    // Test 1: Basic insert and get
    printf("Test 1: Insert/Get...\n");
    {
        RawrXD_RBTree tree;
        assert(tree.Insert(42, 100));
        assert(tree.Insert(10, 200));
        assert(tree.Insert(99, 300));

        assert(tree.Get(42) == 100);
        assert(tree.Get(10) == 200);
        assert(tree.Get(99) == 300);
        assert(tree.Get(999) == 0); // Not found
        printf("  PASS: Basic insert/get\n");
    }

    // Test 2: Contains
    printf("Test 2: Contains...\n");
    {
        RawrXD_RBTree tree;
        tree.Clear(); // Clear global state from previous test
        tree.Insert(5, 50);
        assert(tree.Contains(5));
        assert(!tree.Contains(99));
        printf("  PASS: Contains check\n");
    }

    // Test 3: Size and empty
    printf("Test 3: Size/Empty...\n");
    {
        RawrXD_RBTree tree;
        tree.Clear();
        assert(tree.Empty());
        assert(tree.Size() == 0);
        tree.Insert(1, 10);
        assert(!tree.Empty());
        assert(tree.Size() == 1);
        tree.Insert(2, 20);
        assert(tree.Size() == 2);
        printf("  PASS: Size tracking\n");
    }

    // Test 4: Clear
    printf("Test 4: Clear...\n");
    {
        RawrXD_RBTree tree;
        tree.Clear();
        tree.Insert(1, 10);
        tree.Insert(2, 20);
        tree.Clear();
        assert(tree.Empty());
        assert(tree.Size() == 0);
        assert(!tree.Contains(1));
        printf("  PASS: Clear\n");
    }

    // Test 5: Stress test (1000 nodes)
    printf("Test 5: Stress (1000 nodes)...\n");
    {
        RawrXD_RBTree tree;
        tree.Clear();
        for (uint64_t i = 0; i < 1000; ++i) {
            assert(tree.Insert(i, i * 10));
        }
        assert(tree.Size() == 1000);
        for (uint64_t i = 0; i < 1000; ++i) {
            assert(tree.Get(i) == i * 10);
        }
        printf("  PASS: 1000 nodes\n");
    }

    // Test 6: ForEach iteration
    printf("Test 6: ForEach iteration...\n");
    {
        RawrXD_RBTree tree;
        tree.Clear();
        tree.Insert(3, 30);
        tree.Insert(1, 10);
        tree.Insert(2, 20);

        uint64_t count = 0;
        uint64_t lastKey = 0;
        tree.ForEach([&](uint64_t key, uint64_t value) {
            assert(key > lastKey || lastKey == 0); // In-order
            assert(value == key * 10);
            lastKey = key;
            ++count;
        });
        assert(count == 3);
        printf("  PASS: In-order iteration\n");
    }

    // Test 7: FindIf
    printf("Test 7: FindIf...\n");
    {
        RawrXD_RBTree tree;
        tree.Insert(5, 50);
        tree.Insert(10, 100);
        tree.Insert(15, 150);

        uint64_t key = 0, value = 0;
        bool found = tree.FindIf(
            [](uint64_t k, uint64_t v) { return v == 100; },
            key, value);
        assert(found);
        assert(key == 10);
        assert(value == 100);
        printf("  PASS: FindIf predicate\n");
    }

    printf("\n==========================\n");
    printf("[RawrXD] ALL TESTS PASSED\n");
    return 0;
}
