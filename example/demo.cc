#include "mario.h"
#include "consumer.h"
#include "mutexlock.h"
#include "port.h"
#include <iostream>
#include <string>
#include <queue>
#include <sys/time.h>
// #include <atomic>


inline int64_t NowMicros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @name The consumer handler
 * @{ */
/**  @} */
static int count = 0;
static int pfunc = 0;
// std::atomic<int> refs = {0};
class FileHandler : public mario::Consumer::Handler
{
    public:
        FileHandler() {};
        virtual void processMsg(const std::string &item) {
            // refs.fetch_add(1, std::memory_order_relaxed);
            pthread_mutex_lock(&mutex);
            count++;
            pthread_mutex_unlock(&mutex);
            // Log(info_log_, "come into log one time");
            // log_info("consume data %s", item.data());
        }
};

void *func1(void *arg)
{
    // log_info("come thread");
    mario::Mario* m = reinterpret_cast<mario::Mario*>(arg);
    std::string item = "chenzongzhi\n";
    for (int i = 0; i < 1000000; i++) {
        pfunc++;
        m->Put(item);
    }
    return NULL;
}

static
std::string itos(unsigned long num)
{
    std::string buf = "";
    unsigned long mod;
    while (num > 0) {
        mod = num % 10;
        buf = char(mod + '0') + buf;
        num /= 10;
    }
    return buf;
}

int main()
{
    using namespace std;
    mario::Status s;
    FileHandler *fh = new FileHandler();
    mario::Mario *m = new mario::Mario(0, fh);
    mario::Slice result;


    int i = 0;
    int64_t st = NowMicros();
    int64_t ed = NowMicros();

    // Multi-thread test: put 1000000 items
    // pthread_t t[10];
    // for (int i = 0; i < 10; i++) {
    // pthread_create(&(t[i]), NULL, func1, (void *)m);
    // }

    // void* tret;
    // for (int i = 0; i < 10; i++) {
    // pthread_join(t[i], &tret);
    // }


    // One-thread test: put 10000000 items
    // std::string item = "chenzongzhi";
    // std::string t1;
    // while (i < 1000000) {
    // s = m->Put(item);
    // // log_info("Status ", s.ToString().c_str());
    // if (!s.ok()) {
    // log_err("Put error");
    // }
    // i++;
    // }

    // Add small items test
    std::string item = "sty";
    std::string item1;
    std::string t1;
    while (i < 200000000) {
        // item1 = item + char((i%32) + 'a');
        s = m->Put(item);
        // log_info("Status %s", s.ToString().c_str());
        if (!s.ok()) {
            log_err("Put error");
        }
        i++;
    }


    // The Origin std::queue test: put 1000000 items

    // std::queue<std::string> q;
    // std::string t2 = "chenzongzhi\n";
    // while (i < 1000000) {
    // q.push(t2);
    // i++;
    // }

    ed = NowMicros();
    log_info("total time %ld\n", ed - st);
    pint(i);
    delete m;
    delete fh;
    pint(count);
    // std::cout << "Final counter value is " << refs << '\n';
    return 0;
}
