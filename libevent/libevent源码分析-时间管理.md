# gettime 
绝对时间和相对时间的概念
wall time和mm time的概念


# 小根堆管理超时事件

# common-timeout 优化超时事件


为什么会有这个优化？这个优化比着普通方法好在哪里? 



#define evtimer_new(base, callback, arg) \
    event_new((base), -1, 0, (callback), (arg))
#define evtimer_add(ev, tv) \
    event_add((ev),(tv))
#define evtimer_del(ev) \
    event_del(ev)
#define evtimer_pending(ev, tv_out) \
    event_pending((ev), EV_TIMEOUT, (tv_out))
# 参考
https://blog.csdn.net/luotuo44/article/details/38661787
https://blog.csdn.net/luotuo44/article/details/38678333