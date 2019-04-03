#include "coroutine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#if __APPLE__ && __MACH__
	#include <sys/ucontext.h>
#else 
	#include <ucontext.h>
#endif 

#define STACK_SIZE (1024*1024)
#define DEFAULT_COROUTINE 16

struct coroutine;

/**
* 协程调度器
*/
struct schedule {
	char stack[STACK_SIZE];
	ucontext_t main; // 主协程的上下文
	int nco; // 未运行结束的协程个数
	int cap; // 协程的最大容量
	int running; // 正在运行的协程ID
	struct coroutine **co; // 用于存放协程 
};

/*
* 协程
*/
struct coroutine {
	coroutine_func func; // 协程所用的函数
	void *ud;  // 协程参数
	ucontext_t ctx; // 协程上下文
	struct schedule * sch; // 该协程所属的调度器
	ptrdiff_t cap; // 
	ptrdiff_t size;
	int status;
	char *stack;
};

/*
* 新建一个协程
*/
struct coroutine * 
_co_new(struct schedule *S , coroutine_func func, void *ud) {
	struct coroutine * co = malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->status = COROUTINE_READY; // 默认的最初状态都是COROUTINE_READY
	co->stack = NULL;
	return co;
}

/**
* 删除一个协程
*/
void
_co_delete(struct coroutine *co) {
	free(co->stack);
	free(co);
}

/**
* 创建一个协程调度器 
*/
struct schedule * 
coroutine_open(void) {
	struct schedule *S = malloc(sizeof(*S));
	S->nco = 0;
	S->cap = DEFAULT_COROUTINE;
	S->running = -1;
	S->co = malloc(sizeof(struct coroutine *) * S->cap);
	memset(S->co, 0, sizeof(struct coroutine *) * S->cap);
	return S;
}

/**
* 关闭一个协程调度器
* @param S 将此调度器关闭
*/
void 
coroutine_close(struct schedule *S) {
	int i;
	// 关闭掉每一个协程
	for (i=0;i<S->cap;i++) {
		struct coroutine * co = S->co[i];
		if (co) {
			_co_delete(co);
		}
	}

	// 释放掉
	free(S->co);
	S->co = NULL;
	free(S);
}

/**
* 创建一个协程
* @param S 该协程所属的调度器
* @param func 该协程运行的函数
* @param ud func的参数
* @return 新建的协程的ID
*/
int 
coroutine_new(struct schedule *S, coroutine_func func, void *ud) {
	struct coroutine *co = _co_new(S, func , ud);
	if (S->nco >= S->cap) {
		// 如果目前协程的数量已经大于调度器的容量，那么进行扩容
		int id = S->cap;
		// 扩容的方式为，扩大为当前容量的2倍，这种方式和Hashmap的扩容略像
		S->co = realloc(S->co, S->cap * 2 * sizeof(struct coroutine *));
		// 初始化内存
		memset(S->co + S->cap , 0 , sizeof(struct coroutine *) * S->cap);
		//将协程放入调度器
		S->co[S->cap] = co;
		// 将容量扩大为两倍
		S->cap *= 2;
		// 尚未结束运行的协程的个数 
		++S->nco; 
		return id;
	} else {
		// 如果目前协程的数量小于调度器的容量
		int i;
		for (i=0;i<S->cap;i++) {
			/* 为什么不 i%S->cap,而是要从nco+i开始呢 
			* 这其实也算是一种优化策略吧，因为前nco有很大概率都非NULL的，直接跳过去更好
			*/
			int id = (i+S->nco) % S->cap;
			if (S->co[id] == NULL) {
				S->co[id] = co;
				++S->nco;
				return id;
			}
		}
	}
	assert(0);
	return -1;
}

static void
mainfunc(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	struct schedule *S = (struct schedule *)ptr;
	int id = S->running;
	struct coroutine *C = S->co[id];
	C->func(S,C->ud);
	_co_delete(C);
	S->co[id] = NULL;
	--S->nco;
	S->running = -1;
}

/**
* 继续运行一个协程
* 
* @param S 协程调度器
* @param id 协程ID
*/
void 
coroutine_resume(struct schedule * S, int id) {
	assert(S->running == -1);
	assert(id >=0 && id < S->cap);
    // 取出协程
	struct coroutine *C = S->co[id];
	if (C == NULL)
		return;
	int status = C->status;
	switch(status) {
	case COROUTINE_READY:
	    //初始化ucontext_t结构体
		getcontext(&C->ctx);
		C->ctx.uc_stack.ss_sp = S->stack; // 每个协程的运行时的栈地址都是，S->stack
		C->ctx.uc_stack.ss_size = STACK_SIZE;
		C->ctx.uc_link = &S->main;
		S->running = id;
		C->status = COROUTINE_RUNNING;
		uintptr_t ptr = (uintptr_t)S;
		makecontext(&C->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));
		// 将当前的上下文放入S->main中，并将C->ctx的上下文替换到当前上下文
		swapcontext(&S->main, &C->ctx);
		break;
	case COROUTINE_SUSPEND:
	    // 将协程所保存的栈的内容，拷贝到当前运行时栈中
		memcpy(S->stack + STACK_SIZE - C->size, C->stack, C->size);
		S->running = id;
		C->status = COROUTINE_RUNNING;
		swapcontext(&S->main, &C->ctx);
		break;
	default:
		assert(0);
	}
}

/*
* 将本协程的栈内容保存起来
* 
*/
static void
_save_stack(struct coroutine *C, char *top) {
	// 这个dummy很关键，是求取整个栈的关键
	char dummy = 0;
	assert(top - &dummy <= STACK_SIZE);
	if (C->cap < top - &dummy) {
		free(C->stack);
		C->cap = top-&dummy;
		C->stack = malloc(C->cap);
	}
	C->size = top - &dummy;
	memcpy(C->stack, &dummy, C->size);
}

/**
* 主动让出协程
* @param S 协程调度器
*/
void
coroutine_yield(struct schedule * S) {
	int id = S->running;
	assert(id >= 0);
	struct coroutine * C = S->co[id];
	assert((char *)&C > S->stack);
	// 保存当前栈
	_save_stack(C,S->stack + STACK_SIZE);
	// 将当前栈的状态改为 挂起
	C->status = COROUTINE_SUSPEND;
	S->running = -1;
	// 交换上下文
	swapcontext(&C->ctx , &S->main);
}

int 
coroutine_status(struct schedule * S, int id) {
	assert(id>=0 && id < S->cap);
	if (S->co[id] == NULL) {
		return COROUTINE_DEAD;
	}
	return S->co[id]->status;
}

/**
* 获取正在运行的协程的ID
* 
* @param S 协程调度器
* @return 协程ID 
*/
int 
coroutine_running(struct schedule * S) {
	return S->running;
}

