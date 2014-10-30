#include <iostream>
#include <string>
#include <queue>
#include <sys/time.h>

#include "mario.h"
#include "consumer.h"
#include "mutexlock.h"
#include "port.h"


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
            log_info("consume data %d %d", item.c_str()[0], item.c_str()[1]);
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
    mario::Mario *m = new mario::Mario(1, fh, 2);

    std::string item = "\x41\x00\x42";
    const char *item1 = "\x41\x03\x42";
    s = m->Put(item1, 3);
    // s = m->Put(item1, 2);
    // s = m->Put(item1, 1);
    if (!s.ok()) {
        log_err("Put error");
        exit(-1);
    }

    delete m;
    delete fh;
    return 0;
}
