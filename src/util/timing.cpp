
#include "../common_setup.hpp"

#if defined(_WIN32) || defined(_WIN64)
// #include <windows.h>
#else
// sched_setscheduler():
#include <sched.h>
#endif

namespace bhc {

void SetupThread()
{
#ifdef BHC_USE_HIGH_PRIORITY_THREADS
#if defined(_WIN32) || defined(_WIN64)
    // std::cout << "Warning, not changing thread priority because on Windows\n";
#else
    struct sched_param sp = {.sched_priority = sched_get_priority_max(SCHED_RR)};
    if(sched_setscheduler(0, SCHED_RR, &sp) < 0) {
        std::cout << "Could not change thread priority (probably not root)\n";
    }
#endif
#endif
}

} // namespace bhc
