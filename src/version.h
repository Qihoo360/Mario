#ifndef __MARIO_VERSION_H_
#define __MARIO_VERSION_H_

#include <stdio.h>
#include <stdint.h>
#include "status.h"
#include "env.h"


namespace mario {

class Version 
{
public:
    Version(RWFile *save);
    ~Version();

    // Status Recovery(WritableFile *save);

    Status StableSave();
    Status InitSelf();

    uint64_t offset() { return offset_; }
    void set_offset(uint64_t offset) { offset_ = offset; }
    void plus_offset() { offset_++; }
    void rise_offset(uint64_t offset_r) { offset_ += offset_r; }

    uint32_t item_num() { return item_num_; }
    void set_item_num(uint32_t item_num) { item_num_ = item_num; }
    void plus_item_num() { item_num_++; }
    void minus_item_num() { item_num_--; }

private:
    uint64_t offset_;
    uint32_t item_num_;
    RWFile *save_;
    // port::Mutex mutex_;


    // No copying allowed;
    Version(const Version&);
    void operator=(const Version&);
};

} // namespace mario

#endif
