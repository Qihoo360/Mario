#include "mario.h"
#include "producer.h"
#include "consumer.h"
#include "version.h"
#include "mutexlock.h"
#include "port.h"
#include "filename.h"
#include "signal.h"

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
    h_(h),
    item_num_(0),
    env_(Env::Default()),
    readfile_(NULL),
    writefile_(NULL),
    versionfile_(NULL),
    version_(NULL),
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
        log_err("malloc error");
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
        version_->StableSave();
    } else {
        log_info("Find the exist file ");
        s = env_->NewRWFile(manifest, &versionfile_);
        if (s.ok()) {
            version_ = new Version(versionfile_);
            version_->InitSelf();
            pronum_ = version_->pronum();
            connum_ = version_->connum();
            version_->debug();
        } else {
            log_warn("new REFile error");
        }
        profile = NewFileName(filename_, pronum_);
        confile = NewFileName(filename_, connum_);
        log_info("profile %s confile %s", profile.c_str(), confile.c_str());
        env_->AppendWritableFile(profile, &writefile_, version_->pro_offset());
        uint64_t filesize = writefile_->Filesize();
        log_info("filesize %" PRIu64 "", filesize);
        env_->AppendSequentialFile(confile, &readfile_);
    }

    producer_ = new Producer(writefile_, version_);
    consumer_ = new Consumer(readfile_, h_, version_, connum_);
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

#if defined(MARIO_MMAP)
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
    std::string profile;
    while (1) {
        uint64_t filesize = writefile_->Filesize();
        // log_info("filesize %llu kMmapSize %llu", filesize, kMmapSize);
        if (filesize > kMmapSize) {
            {

            MutexLock l(&mutex_);
            delete producer_;
            delete writefile_;
            pronum_++;
            profile = NewFileName(filename_, pronum_);
            env_->NewWritableFile(profile, &writefile_);
            version_->set_pro_offset(0);
            version_->set_pronum(pronum_);
            version_->StableSave();
            version_->debug();
            producer_ = new Producer(writefile_, version_);

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
#if defined(MARIO_MMAP)
        s = consumer_->Consume(scratch);
        while (!s.ok()) {
            // log_info("consumer_ consume %s", s.ToString().c_str());
            // log_info("connum_ %d", connum_);
            std::string confile = NewFileName(filename_, connum_ + 1);
            // log_info("confile %s connum_ %d", confile.c_str(), connum_);
            // log_info("isendfile %d fileexist %d item_num %d", s.IsEndFile(), env_->FileExists(confile), version_->item_num());
            if (s.IsEndFile() && env_->FileExists(confile)) {
                // log_info("Rotate file ");
                delete readfile_;
                env_->AppendSequentialFile(confile, &readfile_);
                connum_++;
                delete consumer_;
                version_->set_con_offset(0);
                version_->set_connum(connum_);
                version_->StableSave();
                consumer_ = new Consumer(readfile_, h_, version_, connum_);
                s = consumer_->Consume(scratch);
                // log_info("consumer_ consume %s", s.ToString().c_str());
                break;
            } else {
                mutex_.Unlock();
                sleep(1);
                mutex_.Lock();
            }
            s = consumer_->Consume(scratch);
        }
        version_->minus_item_num();
        version_->StableSave();
#endif

#if defined(MARIO_MEMORY)
        s = consumer_->Consume(scratch);
        item_num_--;
#endif
        mutex_.Unlock();
        if (retry_ == -1) {
            while (h_->processMsg(scratch)) {
            }
        } else {
            int retry = retry_ - 1;
            while (!h_->processMsg(scratch) && retry--) {
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

Status Mario::Put(const char* item, int len)
{
    Status s;

    {
    MutexLock l(&mutex_);
    s = producer_->Produce(Slice(item, len));
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
