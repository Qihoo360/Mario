#ifndef __MARIO_H_
#define __MARIO_H_

#include <deque>
#include "consumer.h"
#include "env.h"
#include "port.h"
#include "status.h"
#include "xdebug.h"

namespace mario {

class Producer;
class Consumer;
class Version;

class Mario
{
public:
    Mario(uint32_t consumer_num, Consumer::Handler *h, int32_t retry = 10);
    ~Mario();
    Status Put(const std::string &item);
    Status Put(const char* item, int len);
    Status Get();

    Env *env() { return env_; }
    WritableFile *writefile() { return writefile_; }

    Consumer *consumer() { return consumer_; }
    Consumer::Handler *h() { return h_; }

private:

    struct Writer;
    Producer *producer_;
    Consumer *consumer_;
    uint32_t consumer_num_;
    Consumer::Handler *h_;
    uint64_t item_num_;
    Env* env_;
    SequentialFile *readfile_;
    WritableFile *writefile_;
    RWFile *versionfile_;
    Version* version_;
    port::Mutex mutex_;
    port::CondVar bg_cv_;
    uint32_t pronum_;
    uint32_t connum_;
    int32_t retry_;

    std::string filename_;

    static void SplitLogWork(void* m);
    void SplitLogCall();

    static void BGWork(void* m);
    void BackgroundCall();

    std::deque<Writer *> writers_;

    char* pool_;
    bool exit_all_consume_;
    const std::string mario_path_;

    // No copying allowed
    Mario(const Mario&);
    void operator=(const Mario&);

};

} // namespace mario

#endif
