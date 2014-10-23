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
        virtual void processMsg(const std::string &item) {
            log_info("consume data %s", item.data());
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
     *
     * @return 
     */
    mario::Mario *m = new mario::Mario(1, fh);

    std::string item = "Put data in mario";
    s = m->Put(item);
    if (!s.ok()) {
        log_err("Put error");
        exit(-1);
    }

    delete m;
    delete fh;
    return 0;
}
