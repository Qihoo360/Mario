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

}

Status Version::StableSave()
{
    memcpy(save_->GetData(), this, sizeof(*this));
    return Status::OK();
}

Status Version::InitSelf()
{
    Status s;
    Version *v1 = reinterpret_cast<Version *>(save_->GetData());

    set_item_num(v1->item_num());
    set_offset(v1->offset());
    return Status::OK();
}

} // namespace mario
