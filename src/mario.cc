#include "mario.h"
#include "producer.h"
#include "consumer.h"
#include "version.h"
#include "mutexlock.h"
#include "port.h"
#include "filename.h"

#include <iostream>
#include <string>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace mario {

struct Mario::Writer {
    Status status;
    bool done;
    port::CondVar cv;

    explicit Writer(port::Mutex* mu) : cv(mu) { }
};


Mario::Mario(uint32_t consumer_num, Consumer::Handler *h, int32_t retry)
    : consumer_num_(consumer_num),
    item_num_(0),
    env_(Env::Default()),
    readfile_(NULL),
    writefile_(NULL),
    versionfile_(NULL),
    version_(NULL),
    info_log_(NULL),
    bg_cv_(&mutex_),
    pronum_(0),
    connum_(0),
    retry_(retry),
    pool_(NULL),
    exit_all_consume_(false),
    mario_path_("./log")
{

    env_->set_thread_num(consumer_num_);

#if defined(MARIO_MEMORY)
    pool_ = (char *)malloc(sizeof(char) * kPoolSize);
    if (pool_ == NULL) {
        log_warn("malloc error");
        exit(-1);
    }
    producer_ = new Producer(pool_, kPoolSize);
    consumer_ = new Consumer(0, h, pool_, kPoolSize);
#endif

#if defined(MARIO_MMAP)

    filename_ = mario_path_ + kWrite2file;
    const std::string manifest = mario_path_ + kManifest;
    std::string profile;
    std::string confile;
    env_->CreateDir(mario_path_);
    Status s;
    if (!env_->FileExists(manifest)) {
        log_info("Manifest file not exist");
        profile = NewFileName(filename_, pronum_);
        confile = NewFileName(filename_, connum_);
        env_->NewWritableFile(profile, &writefile_);
        env_->NewSequentialFile(confile, &readfile_);
        env_->NewRWFile(manifest, &versionfile_);
        if (versionfile_ == NULL) {
            log_warn("new versionfile error");
        }
        version_ = new Version(versionfile_);
        version_->set_item_num(0);
        version_->set_offset(0);
        version_->set_pronum(0);
        version_->set_connum(0);
        version_->StableSave();
    } else {
        log_info("Find the exist file ");
        s = env_->NewRWFile(manifest, &versionfile_);
        if (s.ok()) {
            version_ = new Version(versionfile_);
            version_->InitSelf();
            pronum_ = version_->pronum();
            connum_ = version_->connum();
            log_info("Current offset %" PRIu64 "itemnum %u pronum %u connum %u", version_->offset(), version_->item_num(), version_->pronum(), version_->connum());
        } else {
            log_warn("new REFile error");
        }
        profile = NewFileName(filename_, pronum_);
        confile = NewFileName(filename_, connum_);
        env_->AppendWritableFile(profile, &writefile_);
        env_->AppendSequentialFile(confile, &readfile_);
    }

    uint64_t filesize;
    env_->GetFileSize(profile, &filesize);

    producer_ = new Producer(writefile_, filesize);
    // log_info("offset %llu", version_->offset());
    consumer_ = new Consumer(readfile_, version_->offset(), h, version_, connum_);
    env_->StartThread(&Mario::SplitLogWork, this);

#endif
    for (uint32_t i = 0; i < consumer_num_; i++) {
        env_->StartThread(&Mario::BGWork, this);
    }
    // env_->Schedule(&Mario::BGWork, this);
}

Mario::~Mario()
{
    mutex_.Lock();
    exit_all_consume_ = true;
    bg_cv_.SignalAll();
    while (consumer_num_ > 0) {
        bg_cv_.Wait();
    }
    mutex_.Unlock();

    delete consumer_;
    delete producer_;
    // delete info_log_;

#if defined(MARIO_MMAP)
    // log_info("offset %llu itemnum %u ", version_->offset(), version_->item_num());
    delete version_;
    delete versionfile_;
#endif

    delete writefile_;
    delete readfile_;
#if defined(MARIO_MEMORY)
    free(pool_);
#endif
    // delete env_;
}


#if defined(MARIO_MMAP)

void Mario::SplitLogWork(void *m)
{
    reinterpret_cast<Mario*>(m)->SplitLogCall();
}

void Mario::SplitLogCall()
{
    Status s;
    while (1) {
        uint64_t filesize = writefile_->Filesize();
        // log_info("filesize %llu kMmapSize %llu", filesize, kMmapSize);
        if (filesize > kMmapSize) {
            {

            MutexLock l(&mutex_);
            delete producer_;
            delete writefile_;
            pronum_++;
            version_->set_pronum(pronum_);
            version_->StableSave();
            env_->NewWritableFile(NewFileName(filename_, pronum_), &writefile_);
            producer_ = new Producer(writefile_, filesize);

            }
        }
        sleep(1);
    }
}

#endif

void Mario::BGWork(void *m)
{
    reinterpret_cast<Mario*>(m)->BackgroundCall();
}

void Mario::BackgroundCall()
{
    std::string scratch("");
    Status s;
    while (1) {
        {
        mutex_.Lock();
#if defined(MARIO_MMAP)
        while (version_->item_num() <= 0) {
#endif

#if defined(MARIO_MEMORY)
        while (item_num_ <= 0) {
#endif
            if (exit_all_consume_) {
                consumer_num_--;
                mutex_.Unlock();
                bg_cv_.Signal();
                pthread_exit(NULL);
            }
            bg_cv_.Wait();
        }
        scratch = "";
        s = consumer_->Consume(scratch);
#if defined(MARIO_MMAP)
        std::string profile = NewFileName(filename_, consumer_->filenum() + 1);
        if (s.IsEndFile() && env_->FileExists(profile)) {
            delete readfile_;
            env_->AppendSequentialFile(profile, &readfile_);
            Consumer::Handler *ho = consumer_->h();
            uint32_t tmp = consumer_->filenum() + 1;
            delete consumer_;
            version_->set_offset(0);
            version_->set_connum(tmp);
            consumer_ = new Consumer(readfile_, 0, ho, version_, tmp);
        }
        version_->minus_item_num();
        version_->StableSave();
#endif

#if defined(MARIO_MEMORY)
        item_num_--;
#endif
        mutex_.Unlock();
        if (retry_ == -1) {
            while (!consumer_->h()->processMsg(scratch)) {
            }
        } else {
            int retry = retry_ - 1;
            while (!consumer_->h()->processMsg(scratch) && retry--) {
            }
            if (retry <= 0) {
                log_warn("message retry %d time still error %s", retry_, scratch.c_str());
            }
        }

        }
    }
    return ;
}

Status Mario::Put(const std::string &item)
{
    Status s;

    {
    MutexLock l(&mutex_);
    s = producer_->Produce(Slice(item.data(), item.size()));
    if (s.ok()) {
#if defined(MARIO_MMAP)
        version_->plus_item_num();
        version_->StableSave();
#endif

#if defined(MARIO_MEMORY)
        item_num_++;
#endif
    }

    }
    bg_cv_.Signal();
    return s;
}

Status Mario::Get()
{
    Status s;

    std::string scratch;
    MutexLock l(&mutex_);
    s = consumer_->Consume(scratch);

    return s;
}


} // namespace mario
