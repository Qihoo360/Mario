#include "version.h"
#include "xdebug.h"

namespace mario {

Version::Version(RWFile *save) :
    save_(save)
{

    save_->GetData();
    assert(save_ != NULL);
}

Version::~Version()
{
    StableSave();
}

Status Version::StableSave()
{

    char *p = save_->GetData();
    memcpy(p, &offset_, sizeof(uint64_t));
    p += 8;
    memcpy(p, &item_num_, sizeof(uint32_t));
    p += 4;
    memcpy(p, &pronum_, sizeof(uint32_t));
    p += 4;
    memcpy(p, &connum_, sizeof(uint32_t));
    p += 4;
    return Status::OK();
}

Status Version::InitSelf()
{
    Status s;
    if (save_->GetData() != NULL) {
        memcpy((char*)(&offset_), save_->GetData(), sizeof(uint64_t));
        memcpy((char*)(&item_num_), save_->GetData() + 8, sizeof(uint32_t));
        memcpy((char*)(&pronum_), save_->GetData() + 12, sizeof(uint32_t));
        memcpy((char*)(&connum_), save_->GetData() + 16, sizeof(uint32_t));
        // log_info("InitSelf offset %llu itemnum %u pronum %u connum %u", offset_, item_num_, pronum_, connum_);
        return Status::OK();
    } else {
        return Status::Corruption("version init error");
    }
}

} // namespace mario
