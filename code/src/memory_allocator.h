#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>


class MemoryAllocator {
public:
    MemoryAllocator();

    MemoryAllocator(const MemoryAllocator&) = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;

     ~MemoryAllocator();

    char* allocate(size_t size);
    void deallocate(char* ptr);

 private:
    size_t free_memory_;
    size_t use_memory_;
    std::vector<char*> memory_list_;

    std::mutex mu_;
    std::condition_variable cv_;
};
