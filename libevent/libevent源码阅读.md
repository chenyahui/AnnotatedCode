
# 约定
|缩写|描述|
|---|---|
|tv|timeval|

private函数以下划线结尾

event_internal.h中，充斥着
#define ev_flags ev_evcallback.evcb_flags
写法




# 创建event
```c
struct event *event_new(struct event_base *base, evutil_socket_t fd,
    short what, event_callback_fn cb,
    void *arg);
```

```c
#define evsignal_new(base, signum, cb, arg) \
    event_new(base, signum, EV_SIGNAL|EV_PERSIST, cb, arg)
```

```c
#define evtimer_new(base, callback, arg) \
    event_new((base), -1, 0, (callback), (arg))
```

所以singal只需要EV_SIGNAL就可以了，
evtimer不需要事件


# 怎么根据平台选择相关的

全局变量  用宏平台生成相关
```c
static const struct eventop *eventops[] = {
#ifdef EVENT__HAVE_EVENT_PORTS
	&evportops,
#endif
#ifdef EVENT__HAVE_WORKING_KQUEUE
	&kqops,
#endif
#ifdef EVENT__HAVE_EPOLL
	&epollops,
#endif
#ifdef EVENT__HAVE_DEVPOLL
	&devpollops,
#endif
#ifdef EVENT__HAVE_POLL
	&pollops,
#endif
#ifdef EVENT__HAVE_SELECT
	&selectops,
#endif
#ifdef _WIN32
	&win32ops,
#endif
	NULL
};
```

然后选择base->sel
```c
	for (i = 0; eventops[i] && !base->evbase; i++) {
		if (cfg != NULL) {
			/* determine if this backend should be avoided */
			if (event_config_is_avoided_method(cfg,
				eventops[i]->name))
				continue;
			if ((eventops[i]->features & cfg->require_features)
			    != cfg->require_features)
				continue;
		}

		/* also obey the environment variables */
		if (should_check_environment &&
		    event_is_method_disabled(eventops[i]->name))
			continue;

		base->evsel = eventops[i];

		base->evbase = base->evsel->init(base);
	}
```

# 基础设施

```c
#define LIST_HEAD(name, type)						\
struct name {								\
	struct type *lh_first;  /* first element */			\
	}
```
LIST_HEAD相当于定一个list结构体, 其中每个元素的类型是struct type
有点模拟泛型的意思

已定义的list类型
```c
LIST_HEAD (event_dlist, event); 
```

# 当前进度
看到了evmap_io_add_


# 
libevent为了兼容各个平台以及兼容旧的API，使得代码中出现了大量的宏定义，给阅读代码带来了一些困难。
但libevent的质量还是很高的


# timerfd
https://blog.csdn.net/yusiguyuan/article/details/22936707

# changelist是什么

libevent内存管理