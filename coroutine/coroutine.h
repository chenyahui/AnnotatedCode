#ifndef C_COROUTINE_H
#define C_COROUTINE_H

#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_RUNNING 2
#define COROUTINE_SUSPEND 3

// 协程调度器
// 为了ABI兼容，这里故意没有提供具体实现
struct schedule;

typedef void (*coroutine_func)(struct schedule *, void *ud);

// 开启一个协程调度器
struct schedule * coroutine_open(void);

// 关闭一个协程调度器
void coroutine_close(struct schedule *);

// 创建一个协程
int coroutine_new(struct schedule *, coroutine_func, void *ud);

// 切换到对应协程中执行
void coroutine_resume(struct schedule *, int id);

// 返回协程状态
int coroutine_status(struct schedule *, int id);

// 协程是否在正常运行
int coroutine_running(struct schedule *);

// 切出协程
void coroutine_yield(struct schedule *);

#endif
