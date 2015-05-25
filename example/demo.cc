#include <iostream>
#include <string>
#include <queue>
#include <sys/time.h>

#include "mario.h"
#include "consumer.h"
#include "mutexlock.h"
#include "port.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int count = 0;

/**
 * @brief The handler inherit from the Consumer::Handler
 * The processMsg is pure virtual function, so you need implementation your own
 * version
 */
class FileHandler : public mario::Consumer::Handler
{
public:
    FileHandler() {};
    virtual bool processMsg(const std::string &item) {
        pthread_mutex_lock(&mutex);
        count++;
        pthread_mutex_unlock(&mutex);
        log_info("consume data %s", item.data());
        return true;
    }
};

int main()
{
    mario::Status s;
    FileHandler *fh = new FileHandler();
    /**
     * @brief 
     *
     * @param 1 is the thread number
     * @param fh is the handler that you implement. It tell the mario how
     * to consume the data
     * @param 2 is the retry times. The default value is 10 if you don't set it.
     *
     * @return 
     */
    mario::Mario *m = new mario::Mario(10, fh, 2);

    std::string item2 = "heiheiadfasdf";
    std::string item = "a";
    std::string item1 = "abcde";
    std::string item3 = "abcdeasdfwupeiq";
    int i = 20000;
    while (i--) {
        s = m->Put(item);
        s = m->Put(item1);
        s = m->Put(item2);
        s = m->Put(item3);
        if (!s.ok()) {
            log_err("Put error");
            exit(-1);
        }
    }

    sleep(10);
    log_info("count %d", count);
    delete m;
    delete fh;
    return 0;
}
