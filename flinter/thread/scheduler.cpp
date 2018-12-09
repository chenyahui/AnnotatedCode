#include "flinter/thread/scheduler.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>

#include "config.h"
#if HAVE_SCHED_H
#include <sched.h>
#endif

namespace flinter {

bool Scheduler::SetScheduler(const Policy &policy, int priority)
{
    (void)policy;
    (void)priority;

#if HAVE_SCHED_SETSCHEDULER
    struct sched_param p;
    int s;

    switch (policy) {
    case FIFO:  s = SCHED_FIFO;  break;
    case BATCH: s = SCHED_BATCH; break;
    case IDLE:  s = SCHED_IDLE;  break;
    case RR:    s = SCHED_RR;    break;
    case OTHER: s = SCHED_OTHER; break;
    default:              return false;
    }

    memset(&p, 0, sizeof(p));
    p.sched_priority = priority;
    if (sched_setscheduler(0, s, &p)) {
        return false;
    }
#endif // HAVE_SCHED_SETSCHEDULER

    return true;
}

bool Scheduler::SetScheduler(const std::string &policy, int priority)
{
    std::string low(policy);
    std::transform(low.begin(), low.end(), low.begin(), tolower);

    if (low.compare("fifo") == 0) {
        return SetScheduler(FIFO, priority);
    } else if (low.compare("rr") == 0) {
        return SetScheduler(RR, priority);
    } else if (low.compare("idle") == 0) {
        return SetScheduler(IDLE, priority);
    } else if (low.compare("batch") == 0) {
        return SetScheduler(BATCH, priority);
    } else if (low.compare("other") == 0) {
        return SetScheduler(OTHER, priority);
    } else {
        return false;
    }
}

bool Scheduler::SetAffinity(int affinity)
{
    if (affinity < 0) {
        return false;
    }

    std::vector<int> a(1);
    a[0] = affinity;
    return SetAffinity(a);
}

bool Scheduler::SetAffinity(const std::vector<int> &affinity)
{
    if (affinity.empty()) {
        return true;
    }

#if HAVE_SCHED_SETAFFINITY
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (std::vector<int>::const_iterator p = affinity.begin(); p != affinity.end(); ++p) {
        if (*p < 0) {
            return false;
        }

        CPU_SET(static_cast<size_t>(*p), &mask);
    }

    if (sched_setaffinity(0, sizeof(mask), &mask)) {
        return false;
    }
#endif // HAVE_SCHED_SETAFFINITY

    return true;
}

} // namespace flinter
