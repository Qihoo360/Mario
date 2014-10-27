#include "consumer.h"
#include "xdebug.h"
#include "mario_item.h"
#include "version.h"
#include <iostream>


namespace mario {


#if defined(MARIO_MEMORY)
Consumer::Consumer(uint64_t initial_offset, Handler *h, char* pool,
        uint64_t pool_len)
    : h_(h),
    initial_offset_(initial_offset),
    last_record_offset_(0),
    end_of_buffer_offset_(kBlockSize),
    pool_(pool),
    origin_(pool),
    pool_len_(pool_len)
{

}

Consumer::~Consumer()
{

}

inline void Consumer::CheckEnd(const uint64_t distance)
{
    if (pool_len_ < distance) {
        pool_len_ = kPoolSize;
        pool_ = origin_;
    }

}

inline void Consumer::ConsumeData(const int distance)
{
    pool_ += distance;
    pool_len_ -= distance;
}

unsigned int Consumer::ReadMemoryRecord(Slice *result)
{
    Status s;
    if (end_of_buffer_offset_ - last_record_offset_ <= kHeaderSize) {
        ConsumeData(end_of_buffer_offset_ - last_record_offset_);
        last_record_offset_ = 0;
    }

    CheckEnd(static_cast<uint64_t>(kHeaderSize));
    const char* header = pool_;
    const uint32_t a = static_cast<uint32_t>(header[0]) & 0xff;
    const uint32_t b = static_cast<uint32_t>(header[1]) & 0xff;
    const uint32_t c = static_cast<uint32_t>(header[2]) & 0xff;
    const unsigned int type = header[3];
    const uint32_t length = a | (b << 8) | (c << 16);

    if (type == kZeroType || length == 0) {
        return kOldRecord;
    }
    ConsumeData(kHeaderSize);
    CheckEnd(static_cast<uint64_t>(length));
    *result = Slice(pool_, length);
    ConsumeData(length);
    last_record_offset_ += kHeaderSize + length;
    return type;
}

#endif

#if defined(MARIO_MMAP)
Consumer::Consumer(SequentialFile* const queue, uint64_t initial_offset,
        Handler *h, Version* version)
    : h_(h),
    initial_offset_(0),
    last_record_offset_(initial_offset % kBlockSize),
    end_of_buffer_offset_(kBlockSize),
    queue_(queue),
    backing_store_(new char[kBlockSize]),
    buffer_(),
    eof_(false),
    version_(version)
{
    queue_->Skip(initial_offset);
}

Consumer::~Consumer()
{
    delete [] backing_store_;
}

unsigned int Consumer::ReadPhysicalRecord(Slice *result)
{
    Status s;
    if (end_of_buffer_offset_ - last_record_offset_ <= kHeaderSize) {
        queue_->Skip(end_of_buffer_offset_ - last_record_offset_);
        version_->rise_offset(end_of_buffer_offset_ - last_record_offset_);
        version_->StableSave();
        last_record_offset_ = 0;
    }
    buffer_.clear();
    s = queue_->Read(kHeaderSize, &buffer_, backing_store_);
    // log_info("buffer_ %s", buffer_.data());
    if (!s.ok()) {
        return kBadRecord;
    }
    const char* header = buffer_.data();
    const uint32_t a = static_cast<uint32_t>(header[0]) & 0xff;
    const uint32_t b = static_cast<uint32_t>(header[1]) & 0xff;
    const uint32_t c = static_cast<uint32_t>(header[2]) & 0xff;
    const unsigned int type = header[3];
    const uint32_t length = a | (b << 8) | (c << 16);
    if (type == kZeroType || length == 0) {
        buffer_.clear();
        return kOldRecord;
    }
    buffer_.clear();
    s = queue_->Read(length, &buffer_, backing_store_);
    // log_info("buffer_ %s", buffer_.data());
    *result = Slice(buffer_.data(), buffer_.size());
    last_record_offset_ += kHeaderSize + length;
    if (s.ok()) {
        version_->rise_offset(kHeaderSize + length);
        version_->StableSave();
    }
    return type;
}

#endif

Consumer::Handler::~Handler() {};

Status Consumer::Consume(std::string &scratch)
{
    Status s;
    if (last_record_offset_ < initial_offset_) {
        return Status::IOError("last_record_offset exceed");
    }

    scratch = "";

    Slice fragment;
    while (true) {
#if defined(MARIO_MEMORY)
        const unsigned int record_type = ReadMemoryRecord(&fragment);
#endif

#if defined(MARIO_MMAP)
        const unsigned int record_type = ReadPhysicalRecord(&fragment);
#endif
        switch (record_type) {
        case kFullType:
            scratch = std::string(fragment.data(), fragment.size());
            s = Status::OK();
            break;
        case kFirstType:
            scratch.assign(fragment.data(), fragment.size());
            s = Status::NotFound("Middle Status");
            break;
        case kMiddleType:
            scratch.append(fragment.data(), fragment.size());
            s = Status::NotFound("Middle Status");
            break;
        case kLastType:
            scratch.append(fragment.data(), fragment.size());
            s = Status::OK();
            break;
        case kEof:
            return Status::EndFile("Eof");
        case kBadRecord:
            return Status::Corruption("Data Corruption");
        case kOldRecord:
            return Status::EndFile("Eof");
        default:
            s = Status::Corruption("Unknow reason");
            break;
        }
        // TODO:do handler here
        if (s.ok()) {
            break;
        }
    }
    return Status::OK();
}

}
