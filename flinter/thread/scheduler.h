#ifndef FLINTER_THREAD_SCHEDULER_H
#define FLINTER_THREAD_SCHEDULER_H

#include <string>
#include <vector>

namespace flinter {

class Scheduler {
public:
    enum Policy {
        FIFO,
        BATCH,
        OTHER,
        IDLE,
        RR,
    };

    static bool SetAffinity(int affinity);
    static bool SetAffinity(const std::vector<int> &affinity);
    static bool SetScheduler(const Policy &policy, int priority = 0);
    static bool SetScheduler(const std::string &policy, int priority = 0);

}; // class Scheduler

} // namespace flinter

#endif // FLINTER_THREAD_SCHEDULER_H
