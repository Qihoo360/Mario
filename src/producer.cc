#include "producer.h"
#include "xdebug.h"
#include "mario_item.h"

namespace mario {

#if defined(MARIO_MEMORY)
Producer::Producer(char* pool, uint64_t pool_len) :
    pool_(pool),
    origin_(pool),
    pool_len_(pool_len),
    block_offset_(0)
{
}

inline void Producer::MovePoint(const char *src, const uint64_t distance)
{
    if (pool_len_ < distance) {
        // log_info("produce access one loop");
        pool_len_ = kPoolSize;
        pool_ = origin_;
    }
    memcpy(pool_, src, distance);
    pool_ += distance;
    pool_len_ -= distance;

}

Status Producer::EmitMemoryRecord(RecordType t, const char* ptr, size_t n)
{
    Status s;
    assert(n <= 0xffff);
    assert(block_offset_ + kHeaderSize + n <= kBlockSize);

    char buf[kHeaderSize];

    buf[0] = static_cast<char>(n & 0xff);
    buf[1] = static_cast<char>(n >> 8);
    buf[2] = static_cast<char>(t);

    MovePoint(buf, static_cast<uint64_t>(kHeaderSize));
    MovePoint(ptr, static_cast<uint64_t>(n));

    // log_info("pool_len = %d\n", pool_len_);
    block_offset_ += kHeaderSize + n;
    return s;
}
#endif

#if defined(MARIO_MMAP)
Producer::Producer(WritableFile *queue, uint64_t offset) :
    queue_(queue),
    block_offset_(offset % kBlockSize)
{
    log_info("offset %d kBlockSize %d block_offset_ %d", offset, kBlockSize,
            block_offset_);
    assert(queue_ != NULL);
}

Status Producer::EmitPhysicalRecord(RecordType t, const char *ptr, size_t n)
{
    Status s;
    assert(n <= 0xffff);
    assert(block_offset_ + kHeaderSize + n <= kBlockSize);

    char buf[kHeaderSize];

    buf[0] = static_cast<char>(n & 0xff);
    buf[1] = static_cast<char>(n >> 8);
    buf[2] = static_cast<char>(t);

    s = queue_->Append(Slice(buf, kHeaderSize));
    if (s.ok()) {
        s = queue_->Append(Slice(ptr, n));
        if (s.ok()) {
            s = queue_->Flush();
        }
    }
    block_offset_ += kHeaderSize + n;
    return s;
}

#endif

Producer::~Producer()
{

}

Status Producer::Produce(const Slice &item)
{
    Status s;
    const char *ptr = item.data();
    size_t left = item.size();
    bool begin = true;
    do {
        const int leftover = kBlockSize - block_offset_;
        assert(leftover >= 0);
        if (static_cast<size_t>(leftover) < kHeaderSize) {
            if (leftover > 0) {
#if defined(MARIO_MEMORY)
                MovePoint("\x00\x00\x00\x00\x00\x00", leftover);
#endif
#if defined(MARIO_MMAP)
                queue_->Append(Slice("\x00\x00\x00\x00\x00\x00", leftover));
#endif
            }
            block_offset_ = 0;
        }

        const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
        const size_t fragment_length = (left < avail) ? left : avail;
        RecordType type;
        const bool end = (left == fragment_length);
        if (begin && end) {
            type = kFullType;
        } else if (begin) {
            type = kFirstType;
        } else if (end) {
            type = kLastType;
        } else {
            type = kMiddleType;
        }
#if defined(MARIO_MEMORY)
        s = EmitMemoryRecord(type, ptr, fragment_length);
#endif

#if defined(MARIO_MMAP)
        s = EmitPhysicalRecord(type, ptr, fragment_length);
#endif
        ptr += fragment_length;
        left -= fragment_length;
        begin = false;
    } while (s.ok() && left > 0);
    return s;
}

} // namespace mario
