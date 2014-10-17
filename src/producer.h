#ifndef __MARIO_PRODUCER_H_
#define __MARIO_PRODUCER_H_

#include "env.h"
#include "mario_item.h"
#include "status.h"

namespace mario {

class Producer 
{
public:
#if defined(MARIO_MEMORY)
    Producer(char* pool, uint64_t pool_len);
#endif

#if defined(MARIO_MMAP)
    explicit Producer(WritableFile *queue, uint64_t offset);
#endif
    ~Producer();
    Status Produce(const Slice &item);

private:
#if defined(MARIO_MEMORY)
    Status EmitMemoryRecord(RecordType t, const char *ptr, size_t n);
    inline void MovePoint(const char *src, const uint64_t distance);
    char* pool_;
    char* origin_;
    uint64_t pool_len_;
#endif

#if defined(MARIO_MMAP)
    WritableFile *queue_;
    Status EmitPhysicalRecord(RecordType t, const char *ptr, size_t n);
#endif
    int block_offset_;

    // No copying allowed
    Producer(const Producer&);
    void operator=(const Producer&);
};

}

#endif
