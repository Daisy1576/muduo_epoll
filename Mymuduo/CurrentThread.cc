#include "CurrentThread.h"



namespace CurrentThread
{
    __thread int t_cacheTid = 0;

    void cacheTid()
    {
        if(t_cachedTid == 0)
        {
            //通过linux获取当前线程tid值
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid))
        }
    }
}