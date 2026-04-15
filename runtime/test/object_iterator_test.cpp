#include <gtest/gtest.h>
#include <alaska/handles/ObjectHeader.hpp>
#include <vector>

using namespace alaska;

class ObjectIteratorTest : public ::testing::Test {
protected:
    char memory[1024];

    void SetUp() override {
        memset(memory, 0, sizeof(memory));
    }
};

TEST_F(ObjectIteratorTest, BasicIteration) {
    // Layout:
    // [Header 1 (size 16)] [Data 16]
    // [Header 2 (size 32)] [Data 32]
    // [Header 3 (size 8)]  [Data 8]
    
    ObjectHeader *h1 = (ObjectHeader *)memory;
    h1->size = 16;
    h1->handle_id = 1;

    ObjectHeader *h2 = (ObjectHeader *)((char *)h1 + h1->real_object_size());
    h2->size = 32;
    h2->handle_id = 2;

    ObjectHeader *h3 = (ObjectHeader *)((char *)h2 + h2->real_object_size());
    h3->size = 8;
    h3->handle_id = 3;

    void *end = (char *)h3 + h3->real_object_size();

    ObjectRange range(h1, end);
    auto it = range.begin();

    ASSERT_NE(it, range.end());
    EXPECT_EQ((*it)->handle_id, 1);
    EXPECT_EQ((*it)->size, 16);

    ++it;
    ASSERT_NE(it, range.end());
    EXPECT_EQ((*it)->handle_id, 2);
    EXPECT_EQ((*it)->size, 32);

    ++it;
    ASSERT_NE(it, range.end());
    EXPECT_EQ((*it)->handle_id, 3);
    EXPECT_EQ((*it)->size, 8);

    ++it;
    EXPECT_EQ(it, range.end());
}

TEST_F(ObjectIteratorTest, SingleObject) {
    ObjectHeader *h1 = (ObjectHeader *)memory;
    h1->size = 64;
    h1->handle_id = 42;

    void *end = (char *)h1 + h1->real_object_size();

    ObjectRange range(h1, end);
    auto it = range.begin();

    ASSERT_NE(it, range.end());
    EXPECT_EQ((*it)->handle_id, 42);

    ++it;
    EXPECT_EQ(it, range.end());
}

TEST_F(ObjectIteratorTest, EmptyRange) {
    void *ptr = (void *)memory;
    ObjectRange range((ObjectHeader *)ptr, ptr);
    EXPECT_EQ(range.begin(), range.end());
}

TEST_F(ObjectIteratorTest, RangeBasedForLoop) {
    ObjectHeader *h1 = (ObjectHeader *)memory;
    h1->size = 16;
    h1->handle_id = 101;

    ObjectHeader *h2 = (ObjectHeader *)((char *)h1 + h1->real_object_size());
    h2->size = 16;
    h2->handle_id = 102;

    void *end = (char *)h2 + h2->real_object_size();

    std::vector<uint32_t> ids;
    ObjectRange range(h1, end);
    for (ObjectHeader *h : range) {
        ids.push_back(h->handle_id);
    }

    ASSERT_EQ(ids.size(), 2);
    EXPECT_EQ(ids[0], 101);
    EXPECT_EQ(ids[1], 102);
}

TEST_F(ObjectIteratorTest, BoundaryStop) {
    // If the end pointer is exactly at the start of an object, it shouldn't be included.
    ObjectHeader *h1 = (ObjectHeader *)memory;
    h1->size = 16;
    
    void *end = (void *)h1; // End is at the start of h1
    ObjectRange range(h1, end);
    EXPECT_EQ(range.begin(), range.end());
}
