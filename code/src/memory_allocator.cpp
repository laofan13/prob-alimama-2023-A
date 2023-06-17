
#include "memory_allocator.h"

static const uint64_t max_limit = 1 << 30; 
static const int kBlockSize = 1 << 16;

MemoryAllocator::MemoryAllocator(){
    use_memory_ = 0;
}

MemoryAllocator::~MemoryAllocator() {
    for(char *data: memory_list_) {
        delete[] data;
    }
}

char* MemoryAllocator::allocate(size_t size) {
    std::unique_lock<std::mutex> lock(mu_);
    if(memory_list_.empty()) {
        if(use_memory_ +  kBlockSize > max_limit) {
            while(memory_list_.empty()) {
                cv_.wait(lock, [&] {
                    return !memory_list_.empty() ;
                });
            }
        }else{
            lock.unlock();
            char* ret = new char[kBlockSize];
            lock.lock();
            use_memory_ += kBlockSize;
            return ret;
        }
    }
    char* result = memory_list_.back();
    memory_list_.pop_back();
    // No suitable block found.
    return result;
}

void MemoryAllocator::deallocate(char* ptr) {
    {
        std::unique_lock<std::mutex> lock(mu_);
        memory_list_.push_back(ptr);
    }
    cv_.notify_all();
}
