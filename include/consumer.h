#ifndef __MARIO_CONSUMER_H_
#define __MARIO_CONSUMER_H_

#include "env.h"
#include "status.h"

namespace mario {

class Version;


class Consumer
{
public:
    class Handler
    {
    public:
        virtual ~Handler();
        virtual void processMsg(const std::string &item) = 0;
    };

#if defined(MARIO_MEMORY)
    Consumer(uint64_t initial_offset, Handler *h, char* pool, uint64_t pool_len);
#endif

#if defined(MARIO_MMAP)
    Consumer(SequentialFile *queue, uint64_t initial_offset, Handler *h, Version *version);
    uint64_t last_record_offset () const { return last_record_offset_; }
#endif

    ~Consumer();
    Status Consume();

private:
    Handler *h_;
    uint64_t initial_offset_;
    uint64_t last_record_offset_;
    uint64_t end_of_buffer_offset_;

#if defined(MARIO_MEMORY)
    char* pool_;
    char* origin_;
    uint64_t pool_len_;

    inline void CheckEnd(const uint64_t distance);
    inline void ConsumeData(const int distance);
    unsigned int ReadMemoryRecord(Slice *fragment);

#endif

#if defined(MARIO_MMAP)
    SequentialFile* const queue_;
    char* const backing_store_;
    Slice buffer_;
    bool eof_;
    Version* version_;

    unsigned int ReadPhysicalRecord(Slice *fragment);
#endif

    // No copying allowed
    Consumer(const Consumer&);
    void operator=(const Consumer&);
};

}
#endif
