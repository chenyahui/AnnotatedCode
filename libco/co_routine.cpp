/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/

#include "co_routine.h"
#include "co_routine_inner.h"
#include "co_epoll.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>

#include <poll.h>
#include <sys/time.h>
#include <errno.h>

#include <assert.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C"
{
	// 保存当前上下文到第一个参数，并激活第二个参数的上下文
	extern void coctx_swap( coctx_t *,coctx_t* ) asm("coctx_swap");
};
using namespace std;
stCoRoutine_t *GetCurrCo( stCoRoutineEnv_t *env );
struct stCoEpoll_t;

/*
* 线程所管理的协程的运行环境
* 一个线程只有一个这个属性
*/
struct stCoRoutineEnv_t
{
	// 这里实际上维护的是个调用栈
	// 最后一位是当前运行的协程，前一位是当前协程的父协程(即，resume该协程的协程)
	// 可以看出来，libco只能支持128层协程的嵌套调用。这个绝对够了
	stCoRoutine_t *pCallStack[ 128 ]; 

	int iCallStackSize; // 当前调用栈长度

	stCoEpoll_t *pEpoll;  //主要是epoll，作为协程的调度器

	//for copy stack log lastco and nextco
	stCoRoutine_t* pending_co;  
	stCoRoutine_t* occupy_co;
};

//int socket(int domain, int type, int protocol);
void co_log_err( const char *fmt,... )
{
}


#if defined( __LIBCO_RDTSCP__) 
static unsigned long long counter(void)
{
	register uint32_t lo, hi;
	register unsigned long long o;
	__asm__ __volatile__ (
			"rdtscp" : "=a"(lo), "=d"(hi)::"%rcx"
			);
	o = hi;
	o <<= 32;
	return (o | lo);

}
static unsigned long long getCpuKhz()
{
	FILE *fp = fopen("/proc/cpuinfo","r");
	if(!fp) return 1;
	char buf[4096] = {0};
	fread(buf,1,sizeof(buf),fp);
	fclose(fp);

	char *lp = strstr(buf,"cpu MHz");
	if(!lp) return 1;
	lp += strlen("cpu MHz");
	while(*lp == ' ' || *lp == '\t' || *lp == ':')
	{
		++lp;
	}

	double mhz = atof(lp);
	unsigned long long u = (unsigned long long)(mhz * 1000);
	return u;
}
#endif

static unsigned long long GetTickMS()
{
#if defined( __LIBCO_RDTSCP__) 
	static uint32_t khz = getCpuKhz();
	return counter() / khz;
#else
	struct timeval now = { 0 };
	gettimeofday( &now,NULL );
	unsigned long long u = now.tv_sec;
	u *= 1000;
	u += now.tv_usec / 1000;
	return u;
#endif
}

static pid_t GetPid()
{
    static __thread pid_t pid = 0;
    static __thread pid_t tid = 0;
    if( !pid || !tid || pid != getpid() )
    {
        pid = getpid();
#if defined( __APPLE__ )
		tid = syscall( SYS_gettid );
		if( -1 == (long)tid )
		{
			tid = pid;
		}
#elif defined( __FreeBSD__ )
		syscall(SYS_thr_self, &tid);
		if( tid < 0 )
		{
			tid = pid;
		}
#else 
        tid = syscall( __NR_gettid );
#endif

    }
    return tid;

}
/*
static pid_t GetPid()
{
	char **p = (char**)pthread_self();
	return p ? *(pid_t*)(p + 18) : getpid();
}
*/
// 将链表项ap，从链表中删除
template <class T,class TLink>
void RemoveFromLink(T *ap)
{
	// pLink代表ap所属的链表
	TLink *lst = ap->pLink;

	if(!lst) return ;
	assert( lst->head && lst->tail );

	if( ap == lst->head )
	{
		lst->head = ap->pNext;
		if(lst->head)
		{
			lst->head->pPrev = NULL;
		}
	}
	else
	{
		if(ap->pPrev)
		{
			ap->pPrev->pNext = ap->pNext;
		}
	}

	if( ap == lst->tail )
	{
		lst->tail = ap->pPrev;
		if(lst->tail)
		{
			lst->tail->pNext = NULL;
		}
	}
	else
	{
		ap->pNext->pPrev = ap->pPrev;
	}

	ap->pPrev = ap->pNext = NULL;
	ap->pLink = NULL;
}

template <class TNode,class TLink>
void inline AddTail(TLink*apLink,TNode *ap)
{
	if( ap->pLink )
	{
		return ;
	}
	if(apLink->tail)
	{
		apLink->tail->pNext = (TNode*)ap;
		ap->pNext = NULL;
		ap->pPrev = apLink->tail;
		apLink->tail = ap;
	}
	else
	{
		apLink->head = apLink->tail = ap;
		ap->pNext = ap->pPrev = NULL;
	}
	ap->pLink = apLink;
}
template <class TNode,class TLink>
void inline PopHead( TLink*apLink )
{
	if( !apLink->head ) 
	{
		return ;
	}
	TNode *lp = apLink->head;
	if( apLink->head == apLink->tail )
	{
		apLink->head = apLink->tail = NULL;
	}
	else
	{
		apLink->head = apLink->head->pNext;
	}

	lp->pPrev = lp->pNext = NULL;
	lp->pLink = NULL;

	if( apLink->head )
	{
		apLink->head->pPrev = NULL;
	}
}


template <class TNode,class TLink>
void inline Join( TLink*apLink,TLink *apOther )
{
	//printf("apOther %p\n",apOther);
	if( !apOther->head )
	{
		return ;
	}
	TNode *lp = apOther->head;
	while( lp )
	{
		lp->pLink = apLink;
		lp = lp->pNext;
	}
	lp = apOther->head;
	if(apLink->tail)
	{
		apLink->tail->pNext = (TNode*)lp;
		lp->pPrev = apLink->tail;
		apLink->tail = apOther->tail;
	}
	else
	{
		apLink->head = apOther->head;
		apLink->tail = apOther->tail;
	}

	apOther->head = apOther->tail = NULL;
}

/////////////////for copy stack //////////////////////////
/**
* 分配一个栈内存
* @param stack_size的大小
*/
stStackMem_t* co_alloc_stackmem(unsigned int stack_size)
{
	stStackMem_t* stack_mem = (stStackMem_t*)malloc(sizeof(stStackMem_t));
	stack_mem->occupy_co= NULL;
	stack_mem->stack_size = stack_size;
	stack_mem->stack_buffer = (char*)malloc(stack_size);
	stack_mem->stack_bp = stack_mem->stack_buffer + stack_size;
	return stack_mem;
}

/**
* 创建一个共享栈

* @param count 创建共享栈的个数
* @param stack_size 每个共享栈的大小
*/
stShareStack_t* co_alloc_sharestack(int count, int stack_size)
{
	stShareStack_t* share_stack = (stShareStack_t*)malloc(sizeof(stShareStack_t));
	share_stack->alloc_idx = 0;
	share_stack->stack_size = stack_size; 

	//alloc stack array
	share_stack->count = count;
	stStackMem_t** stack_array = (stStackMem_t**)calloc(count, sizeof(stStackMem_t*));
	for (int i = 0; i < count; i++)
	{
		stack_array[i] = co_alloc_stackmem(stack_size);
	}
	share_stack->stack_array = stack_array;
	return share_stack;
}

/*
* 在共享栈中，获取协程的栈内存
*/
static stStackMem_t* co_get_stackmem(stShareStack_t* share_stack)
{
	if (!share_stack)
	{
		return NULL;
	}

	// 轮询的使用shared_stack
	int idx = share_stack->alloc_idx % share_stack->count;
	share_stack->alloc_idx++;

	return share_stack->stack_array[idx];
}


// ----------------------------------------------------------------------------
struct stTimeoutItemLink_t;
struct stTimeoutItem_t;

// 自己管理的epoll结构体
struct stCoEpoll_t
{
	int iEpollFd;	// epoll的id

	static const int _EPOLL_SIZE = 1024 * 10;

	struct stTimeout_t *pTimeout;  // 超时管理器

	struct stTimeoutItemLink_t *pstTimeoutList; // 目前已超时的事件，仅仅作为中转使用，最后会合并到active上

	struct stTimeoutItemLink_t *pstActiveList; // 正在处理的事件

	co_epoll_res *result; 
};

typedef void (*OnPreparePfn_t)( stTimeoutItem_t *,struct epoll_event &ev, stTimeoutItemLink_t *active );
typedef void (*OnProcessPfn_t)( stTimeoutItem_t *);

/*
* 超时链表中的一个项
*/
struct stTimeoutItem_t
{
	enum
	{
		eMaxTimeout = 40 * 1000 //40s
	};

	stTimeoutItem_t *pPrev;	// 前一个元素
	stTimeoutItem_t *pNext; // 后一个元素

	stTimeoutItemLink_t *pLink; // 该链表项所属的链表

	unsigned long long ullExpireTime;

	OnPreparePfn_t pfnPrepare;  // 预处理函数，在eventloop中会被调用
	OnProcessPfn_t pfnProcess;  // 处理函数 在eventloop中会被调用

	void *pArg; // self routine pArg 是pfnPrepare和pfnProcess的参数

	bool bTimeout; // 是否已经超时
};
/*
* 超时链表
*/
struct stTimeoutItemLink_t
{
	stTimeoutItem_t *head;
	stTimeoutItem_t *tail;

};
/*
* 毫秒级的超时管理器
* 使用时间轮实现
* 但是是有限制的，最长超时时间不可以超过iItemSize毫秒
*/
struct stTimeout_t
{
	/*
	   时间轮
	   超时事件数组，总长度为iItemSize,每一项代表1毫秒，为一个链表，代表这个时间所超时的事件。

	   这个数组在使用的过程中，会使用取模的方式，把它当做一个循环数组来使用，虽然并不是用循环链表来实现的
	*/
	stTimeoutItemLink_t *pItems;
	int iItemSize;   // 默认为60*1000

	unsigned long long ullStart; //目前的超时管理器最早的时间
	long long llStartIdx;  //目前最早的时间所对应的pItems上的索引
};

/*
* 分配一个超时管理器
* @param iSize  默认为60*1000
*/
stTimeout_t *AllocTimeout( int iSize )
{
	stTimeout_t *lp = (stTimeout_t*)calloc( 1,sizeof(stTimeout_t) );	

	lp->iItemSize = iSize;
	lp->pItems = (stTimeoutItemLink_t*)calloc( 1,sizeof(stTimeoutItemLink_t) * lp->iItemSize );

	lp->ullStart = GetTickMS();
	lp->llStartIdx = 0;

	return lp;
}
void FreeTimeout( stTimeout_t *apTimeout )
{
	free( apTimeout->pItems );
	free ( apTimeout );
}

/*
* 将事件添加到定时器中
* @param apTimeout - (ref) 超时管理器
* @param apItem    - (in) 即将插入的超时事件
* @param allNow    - (in) 当前时间
*/
int AddTimeout( stTimeout_t *apTimeout,stTimeoutItem_t *apItem ,unsigned long long allNow )
{

	// 当前时间管理器的最早超时时间
	if( apTimeout->ullStart == 0 )
	{ 
		// 设置时间轮的最早时间是当前时间
		apTimeout->ullStart = allNow;
		// 设置最早时间对应的index 为 0
		apTimeout->llStartIdx = 0;
	}

	if( allNow < apTimeout->ullStart )
	{
		co_log_err("CO_ERR: AddTimeout line %d allNow %llu apTimeout->ullStart %llu",
					__LINE__,allNow,apTimeout->ullStart);

		return __LINE__;
	}

	if( apItem->ullExpireTime < allNow )
	{
		co_log_err("CO_ERR: AddTimeout line %d apItem->ullExpireTime %llu allNow %llu apTimeout->ullStart %llu",
					__LINE__,apItem->ullExpireTime,allNow,apTimeout->ullStart);

		return __LINE__;
	}

	// 计算当前事件的超时时间和超时管理器的最早时间的差距
	int diff = apItem->ullExpireTime - apTimeout->ullStart;

	if( diff >= apTimeout->iItemSize )
	{
		co_log_err("CO_ERR: AddTimeout line %d diff %d",
					__LINE__,diff);

		return __LINE__;
	}
	/*
	  计算出该事件的超时事件在超时管理器所在的槽的位置
	  apTimeout->pItems + ( apTimeout->llStartIdx + diff ) % apTimeout->iItemSize , apItem );
	  
	  然后在该位置的槽对应的超时链表的尾部添加一个事件
	*/
	AddTail( apTimeout->pItems + ( apTimeout->llStartIdx + diff ) % apTimeout->iItemSize , apItem );

	return 0;
}

/*
* 以allNow参数为截止时间，取出所有的超时事件
*
* @param apTimeout (in) 超时管理器
* @param allNow (in)  截止时间 也就是当前时刻
* @param apResult (out) 最终的超时事件结果会放入此表中
*/
inline void TakeAllTimeout( stTimeout_t *apTimeout,unsigned long long allNow,stTimeoutItemLink_t *apResult )
{
	if( apTimeout->ullStart == 0 )
	{
		apTimeout->ullStart = allNow;
		apTimeout->llStartIdx = 0;
	}

	// 如果当前时间还未达到最早的超时时间，则直接返回
	if( allNow < apTimeout->ullStart )
	{
		return ;
	}

	// 用当前时间减去最早超时时间，因为每一项代表1ms
	// 所以cnt刚好就代表了，超时的个数
	int cnt = allNow - apTimeout->ullStart + 1;

	if( cnt > apTimeout->iItemSize )
	{
		cnt = apTimeout->iItemSize;
	}
	if( cnt < 0 )
	{
		return;
	}

	for( int i = 0;i<cnt;i++)
	{
		int idx = ( apTimeout->llStartIdx + i) % apTimeout->iItemSize;

		// 把该格子上的所有超时时间都放进去(同一时刻可能有多个超时时间)
		Join<stTimeoutItem_t,stTimeoutItemLink_t>( apResult,apTimeout->pItems + idx  );
	}
	apTimeout->ullStart = allNow;
	apTimeout->llStartIdx += cnt - 1;
}

/**
* 协程回调函数的wrapper
*/
static int CoRoutineFunc( stCoRoutine_t *co,void * )
{
	if( co->pfn )
	{
		co->pfn( co->arg );
	}

	// 标识该协程已经结束
	co->cEnd = 1;

	stCoRoutineEnv_t *env = co->env;
	
	// 函数结束时，将会
	co_yield_env( env );
	return 0;
}

/**
* 根据协程管理器env, 新建一个协程
* 
* @param env - (input) 协程所在线程的环境
* @param attr - (input) 协程属性，目前主要是共享栈 
* @param pfn - (input) 协程所运行的函数
* @param arg - (input) 协程运行函数的参数
*/
struct stCoRoutine_t *co_create_env( stCoRoutineEnv_t * env, const stCoRoutineAttr_t* attr,
		pfn_co_routine_t pfn,void *arg )
{

	// 初始化属性。并且给默认值
	stCoRoutineAttr_t at;
	if( attr )
	{
		memcpy( &at,attr,sizeof(at) );
	}


	if( at.stack_size <= 0 )
	{
		at.stack_size = 128 * 1024; // 默认的为128k
	}
	else if( at.stack_size > 1024 * 1024 * 8 )
	{
		at.stack_size = 1024 * 1024 * 8;
	}

	if( at.stack_size & 0xFFF ) 
	{
		at.stack_size &= ~0xFFF;
		at.stack_size += 0x1000;
	}

	stCoRoutine_t *lp = (stCoRoutine_t*)malloc( sizeof(stCoRoutine_t) );
	
	memset( lp,0,(long)(sizeof(stCoRoutine_t))); 


	lp->env = env;
	lp->pfn = pfn;
	lp->arg = arg;

	stStackMem_t* stack_mem = NULL;
	if( at.share_stack )
	{
		// 如果采用了共享栈模式，则获取到其中一个共享栈的内存
		stack_mem = co_get_stackmem( at.share_stack);
		at.stack_size = at.share_stack->stack_size;
	}
	else
	{
		// 如果没有采用共享栈，则分配内存
		stack_mem = co_alloc_stackmem(at.stack_size);
	}

	lp->stack_mem = stack_mem;

	// 设置该协程的context
	lp->ctx.ss_sp = stack_mem->stack_buffer; // 栈地址
	lp->ctx.ss_size = at.stack_size; // 栈大小

	lp->cStart = 0;
	lp->cEnd = 0;
	lp->cIsMain = 0;
	lp->cEnableSysHook = 0;	// 默认不开启hook
	lp->cIsShareStack = at.share_stack != NULL;

	// 仅在共享栈的时候有意义
	lp->save_size = 0;
	lp->save_buffer = NULL;

	return lp;
}

/**
* 创建一个协程对象
* 
* @param ppco - (output) 协程的地址，未初始化，需要在此函数中将其申请内存空间以及初始化工作
* @param attr - (input) 协程属性，目前主要是共享栈 
* @param pfn - (input) 协程所运行的函数
* @param arg - (input) 协程运行函数的参数
*/
int co_create( stCoRoutine_t **ppco,const stCoRoutineAttr_t *attr,pfn_co_routine_t pfn,void *arg )
{
	// 查找当前线程的管理环境
	if( !co_get_curr_thread_env() ) 
	{
		// 如果找不到，则初始化协程
		co_init_curr_thread_env();
	}

	// 根据协程的运行环境，来创建一个协程
	stCoRoutine_t *co = co_create_env( co_get_curr_thread_env(), attr, pfn,arg );

	*ppco = co;
	return 0;
}

void co_free( stCoRoutine_t *co )
{
    if (!co->cIsShareStack) 
    {    
        free(co->stack_mem->stack_buffer);
        free(co->stack_mem);
    }   
    free( co );
}

void co_release( stCoRoutine_t *co )
{
    co_free( co );
}

void co_swap(stCoRoutine_t* curr, stCoRoutine_t* pending_co);

/*
* 语义：继续运行协程
* 实际上：
* @param co - (input) 要切换的协程
*/
void co_resume( stCoRoutine_t *co )
{
	stCoRoutineEnv_t *env = co->env;

	// 找到当前运行的协程, 从数组最后一位拿出当前运行的协程，如果目前没有协程，那就是主线程
	stCoRoutine_t *lpCurrRoutine = env->pCallStack[ env->iCallStackSize - 1 ];

	if( !co->cStart )
	{
		// 如果当前协程还没有开始运行，为其构建上下文
		coctx_make( &co->ctx,(coctx_pfn_t)CoRoutineFunc,co, 0 );
		co->cStart = 1;
	}

	// 将指定协程放入线程的协程队列末尾
	env->pCallStack[ env->iCallStackSize++ ] = co;
	
	// 将当前运行的上下文保存到lpCurrRoutine中，同时将协程co的上下文替换进去
	// 执行完这一句，当前的运行环境就被替换为 co 了
	co_swap( lpCurrRoutine, co );
}

/*
*
* 主动将当前运行的协程挂起，并恢复到上一层的协程
*
* @param env 协程管理器 
*/
void co_yield_env( stCoRoutineEnv_t *env )
{
	// 这里直接取了iCallStackSize - 2，那么万一icallstacksize < 2呢？
	// 所以这里实际上有个约束，就是co_yield之前必须先co_resume, 这样就不会造成这个问题了

	// last就是 找到上次调用co_resume(curr)的协程
	stCoRoutine_t *last = env->pCallStack[ env->iCallStackSize - 2 ];

	// 当前栈
	stCoRoutine_t *curr = env->pCallStack[ env->iCallStackSize - 1 ];

	env->iCallStackSize--;

	// 把上下文当前的存储到curr中，并切换成last的上下文
	co_swap( curr, last);
}

void co_yield_ct()
{
	co_yield_env( co_get_curr_thread_env() );
}

void co_yield( stCoRoutine_t *co )
{
	co_yield_env( co->env );
}

/**
* 将原本占用共享栈的协程的内存保存起来。
* @param occupy_co 原本占用共享栈的协程
*/
void save_stack_buffer(stCoRoutine_t* occupy_co)
{
	///copy out
	stStackMem_t* stack_mem = occupy_co->stack_mem;
	// 计算出栈的大小
	int len = stack_mem->stack_bp - occupy_co->stack_sp;

	if (occupy_co->save_buffer)
	{
		free(occupy_co->save_buffer), occupy_co->save_buffer = NULL;
	}

	occupy_co->save_buffer = (char*)malloc(len); //malloc buf;
	occupy_co->save_size = len;

	// 将当前运行栈的内容，拷贝到save_buffer中
	memcpy(occupy_co->save_buffer, occupy_co->stack_sp, len);
}

/*
* 1. 将当前的运行上下文保存到curr中
* 2. 将当前的运行上下文替换为pending_co中的上下文

* @param curr
* @param pending_co 
*/
void co_swap(stCoRoutine_t* curr, stCoRoutine_t* pending_co)
{
 	stCoRoutineEnv_t* env = co_get_curr_thread_env();

	//get curr stack sp
	//这里非常重要!!!： 这个c变量的实现，作用是为了找到目前的栈底，因为c变量是最后一个放入栈中的内容。
	char c;
	curr->stack_sp= &c;

	if (!pending_co->cIsShareStack)
	{  
		// 如果没有采用共享栈，清空pending_co和occupy_co
		env->pending_co = NULL;
		env->occupy_co = NULL;
	}
	else 
	{   
		// 如果采用了共享栈
		env->pending_co = pending_co; 
		
		//get last occupy co on the same stack mem
		// occupy_co指的是，和pending_co共同使用一个共享栈的协程
		// 把它取出来是为了先把occupy_co的内存保存起来
		stCoRoutine_t* occupy_co = pending_co->stack_mem->occupy_co;
		
		//set pending co to occupy thest stack mem;
		// 将该共享栈的占用者改为pending_co
		pending_co->stack_mem->occupy_co = pending_co;

		env->occupy_co = occupy_co;
		
		if (occupy_co && occupy_co != pending_co)
		{  
			// 如果上一个使用协程不为空, 则需要把它的栈内容保存起来。
			save_stack_buffer(occupy_co);
		}
	}

	// swap context
	coctx_swap(&(curr->ctx),&(pending_co->ctx) );

	// 这个地方很绕，上一步coctx_swap会进入到pending_co的协程环境中运行
	// 到这一步，已经yield回此协程了，才会执行下面的语句
	// 而yield回此协程之前，env->pending_co会被上一层协程设置为此协程
	// 因此可以顺利执行: 将之前保存起来的栈内容，恢复到运行栈上

	//stack buffer may be overwrite, so get again;
	stCoRoutineEnv_t* curr_env = co_get_curr_thread_env();
	stCoRoutine_t* update_occupy_co =  curr_env->occupy_co;
	stCoRoutine_t* update_pending_co = curr_env->pending_co;
	
	// 将栈的内容恢复，如果不是共享栈的话，每个协程都有自己独立的栈空间，则不用恢复。
	if (update_occupy_co && update_pending_co && update_occupy_co != update_pending_co)
	{
		// resume stack buffer
		if (update_pending_co->save_buffer && update_pending_co->save_size > 0)
		{
			// 将之前保存起来的栈内容，恢复到运行栈上
			memcpy(update_pending_co->stack_sp, update_pending_co->save_buffer, update_pending_co->save_size);
		}
	}
}



//int poll(struct pollfd fds[], nfds_t nfds, int timeout);
// { fd,events,revents }
struct stPollItem_t ;

struct stPoll_t : public stTimeoutItem_t 
{
	struct pollfd *fds;
	nfds_t nfds; // typedef unsigned long int nfds_t;

	stPollItem_t *pPollItems;  // 其中的 pPollItems

	int iAllEventDetach; // 标识是否已经处理过了这个对象了

	int iEpollFd;

	int iRaiseCnt;  // poll的active的事件个数
};

struct stPollItem_t : public stTimeoutItem_t
{
	struct pollfd *pSelf;
	stPoll_t *pPoll;

	struct epoll_event stEvent;
};
/*
 *   EPOLLPRI 		POLLPRI    // There is urgent data to read.  
 *   EPOLLMSG 		POLLMSG
 *
 *   				POLLREMOVE
 *   				POLLRDHUP
 *   				POLLNVAL
 *
 * */
static uint32_t PollEvent2Epoll( short events )
{
	uint32_t e = 0;	
	if( events & POLLIN ) 	e |= EPOLLIN;
	if( events & POLLOUT )  e |= EPOLLOUT;
	if( events & POLLHUP ) 	e |= EPOLLHUP;
	if( events & POLLERR )	e |= EPOLLERR;
	if( events & POLLRDNORM ) e |= EPOLLRDNORM;
	if( events & POLLWRNORM ) e |= EPOLLWRNORM;
	return e;
}
static short EpollEvent2Poll( uint32_t events )
{
	short e = 0;	
	if( events & EPOLLIN ) 	e |= POLLIN;
	if( events & EPOLLOUT ) e |= POLLOUT;
	if( events & EPOLLHUP ) e |= POLLHUP;
	if( events & EPOLLERR ) e |= POLLERR;
	if( events & EPOLLRDNORM ) e |= POLLRDNORM;
	if( events & EPOLLWRNORM ) e |= POLLWRNORM;
	return e;
}


static stCoRoutineEnv_t* g_arrCoEnvPerThread[ 204800 ] = { 0 };

// 初始化当前线程的协程管理器
void co_init_curr_thread_env()
{
	//当前的线程的ID
	pid_t pid = GetPid();	
	
	g_arrCoEnvPerThread[ pid ] = (stCoRoutineEnv_t*)calloc( 1,sizeof(stCoRoutineEnv_t) );
	
	stCoRoutineEnv_t *env = g_arrCoEnvPerThread[ pid ];

	// 当前协程数为0
	env->iCallStackSize = 0;
    
	// 创建一个协程
	struct stCoRoutine_t *self = co_create_env( env, NULL, NULL,NULL );
	
	self->cIsMain = 1;	// 标识是一个主协程

	env->pending_co = NULL; // 初始化为 null
	env->occupy_co = NULL;  // 初始化为 null

	// 初始化协程上下文
	coctx_init( &self->ctx );

	// 初始化协程管理器的时候，会把主协程放在第一个
	env->pCallStack[ env->iCallStackSize++ ] = self;

	stCoEpoll_t *ev = AllocEpoll();
	SetEpoll( env,ev );
}


// 获取当前线程的协程管理器
stCoRoutineEnv_t *co_get_curr_thread_env()
{
	return g_arrCoEnvPerThread[ GetPid() ];
}

void OnPollProcessEvent( stTimeoutItem_t * ap )
{
	stCoRoutine_t *co = (stCoRoutine_t*)ap->pArg;
	co_resume( co );
}

// active事件的预处理
/*
* @param ap
* @param e
* @param active
*/
void OnPollPreparePfn( stTimeoutItem_t * ap,struct epoll_event &e,stTimeoutItemLink_t *active )
{
	stPollItem_t *lp = (stPollItem_t *)ap;
	// 事件转换
	lp->pSelf->revents = EpollEvent2Poll( e.events );


	stPoll_t *pPoll = lp->pPoll;

	// 触发的事件个数增加1，以便co_poll_inner返回结果
	pPoll->iRaiseCnt++;

	// 是否已经处理过的表示
	if( !pPoll->iAllEventDetach )
	{
		pPoll->iAllEventDetach = 1;

		// 将该事件从超时链表中移除
		// 因为事件已经触发了，肯定不能再超时了
		RemoveFromLink<stTimeoutItem_t,stTimeoutItemLink_t>( pPoll );

		// 将该事件添加到active列表中
		AddTail( active,pPoll );
	}
}

/*
* libco的核心调度
* 在此处调度三种事件：
* 1. 被hook的io事件，该io事件是通过co_poll_inner注册进来的
* 2. 超时事件
* 3. 用户主动使用poll的事件
* 所以，如果用户用到了三种事件，必须得配合使用co_eventloop
*
* @param ctx epoll管理器
* @param pfn 每轮事件循环的最后会调用该函数
* @param arg pfn的参数
*/
void co_eventloop( stCoEpoll_t *ctx,pfn_co_eventloop_t pfn,void *arg )
{
	if( !ctx->result )
	{
		ctx->result = co_epoll_res_alloc( stCoEpoll_t::_EPOLL_SIZE );
	}

	co_epoll_res *result = ctx->result;

	for(;;)
	{
		// 最大超时时间设置为 1 ms
		// 所以最长1ms，epoll_wait就会被唤醒
		int ret = co_epoll_wait( ctx->iEpollFd,result,stCoEpoll_t::_EPOLL_SIZE, 1 );

		stTimeoutItemLink_t *active = (ctx->pstActiveList);
		stTimeoutItemLink_t *timeout = (ctx->pstTimeoutList);

		memset( timeout,0,sizeof(stTimeoutItemLink_t) );

		// 处理active事件
		for(int i=0;i<ret;i++)
		{
			// 取出本次epoll响应的事件所对应的stTimeoutItem_t
			stTimeoutItem_t *item = (stTimeoutItem_t*)result->events[i].data.ptr;
			
			// 如果定义了预处理函数，则首先进行预处理。
			// 如果是co_poll_inner/co_poll或者是被hook的函数，则这个函数是OnPollPreparePfn
			if( item->pfnPrepare )
			{
				item->pfnPrepare( item,result->events[i], active );
			}
			else
			{
				// 否则将其加到active的链表中
				AddTail( active,item );
			}
		}

		// 获取当前时刻
		unsigned long long now = GetTickMS();

		// 以当前时间为超时截止点
		// 取出所有的超时事件，放入timeout 链表中
		TakeAllTimeout( ctx->pTimeout, now, timeout );

		// 遍历所有的项，将bTimeout置为true
		stTimeoutItem_t *lp = timeout->head;
		while( lp )
		{
			//printf("raise timeout %p\n",lp);
			lp->bTimeout = true;
			lp = lp->pNext;
		}

		// 将timeout合并到active上面
		Join<stTimeoutItem_t,stTimeoutItemLink_t>( active,timeout );

		lp = active->head;
		while( lp )
		{
			PopHead<stTimeoutItem_t,stTimeoutItemLink_t>( active );
			if( lp->pfnProcess )
			{
				 /*
				   处理该事件，默认为OnPollProcessEvent
				   在OnPollProcessEvent中
				   会使用co_resume恢复协程

				   协程会回到co_poll_inner里面
				 */
				lp->pfnProcess( lp );
			}

			lp = active->head;
		}

		// 每轮事件循环的最后调用该函数
		if( pfn )
		{
			if( -1 == pfn( arg ) )
			{
				break;
			}
		}

	}
}
void OnCoroutineEvent( stTimeoutItem_t * ap )
{
	stCoRoutine_t *co = (stCoRoutine_t*)ap->pArg;
	co_resume( co );
}

/**
* 为线程分配一个epoll
*/
stCoEpoll_t *AllocEpoll()
{
	stCoEpoll_t *ctx = (stCoEpoll_t*)calloc( 1,sizeof(stCoEpoll_t) );

	ctx->iEpollFd = co_epoll_create( stCoEpoll_t::_EPOLL_SIZE );
	
	// 单位是毫秒，只能一分钟
	ctx->pTimeout = AllocTimeout( 60 * 1000 );
	
	ctx->pstActiveList = (stTimeoutItemLink_t*)calloc( 1,sizeof(stTimeoutItemLink_t) );
	ctx->pstTimeoutList = (stTimeoutItemLink_t*)calloc( 1,sizeof(stTimeoutItemLink_t) );


	return ctx;
}

void FreeEpoll( stCoEpoll_t *ctx )
{
	if( ctx )
	{
		free( ctx->pstActiveList );
		free( ctx->pstTimeoutList );
		FreeTimeout( ctx->pTimeout );
		co_epoll_res_free( ctx->result );
	}
	free( ctx );
}

/*
* 获取某个调度器中正在运行的协程
* @ env -(input) 调度器
*/
stCoRoutine_t *GetCurrCo( stCoRoutineEnv_t *env )
{
	// 数组最后一个即为当前调度器
	return env->pCallStack[ env->iCallStackSize - 1 ];
}

/*
* 获取当前线程正在运行的协程
*/
stCoRoutine_t *GetCurrThreadCo( )
{
	stCoRoutineEnv_t *env = co_get_curr_thread_env();
	if( !env ) return 0;
	return GetCurrCo(env);
}



typedef int (*poll_pfn_t)(struct pollfd fds[], nfds_t nfds, int timeout);

/**
* 
* 这个函数也极其重要
* 1. 大部分的sys_hook都需要用到这个函数来把事件注册到epoll中
* 2. 这个函数会把poll事件转换为epoll事件
* 
* @param ctx epoll上下文
* @param fds[] fds 要监听的文件描述符 原始poll函数的参数，
* @param nfds  nfds fds的数组长度 原始poll函数的参数
* @param timeout timeout 等待的毫秒数 原始poll函数的参数
* @param pollfunc 原始的poll函数, g_sys_poll_func
*/
int co_poll_inner( stCoEpoll_t *ctx,struct pollfd fds[], nfds_t nfds, int timeout, poll_pfn_t pollfunc)
{
	if( timeout > stTimeoutItem_t::eMaxTimeout )
	{
		timeout = stTimeoutItem_t::eMaxTimeout;
	}

	int epfd = ctx->iEpollFd;

	// 获取当前协程
	stCoRoutine_t* self = co_self();

	//1.struct change
	stPoll_t& arg = *((stPoll_t*)malloc(sizeof(stPoll_t)));
	memset( &arg,0,sizeof(arg) );

	arg.iEpollFd = epfd;
	arg.fds = (pollfd*)calloc(nfds, sizeof(pollfd));
	arg.nfds = nfds;

	stPollItem_t arr[2];
	if( nfds < sizeof(arr) / sizeof(arr[0]) && !self->cIsShareStack)
	{
		// 如果监听的描述符只有1个或者0个， 并且目前的不是共享栈模型
		arg.pPollItems = arr;
	}	
	else
	{
		// 如果监听的描述符在2个以上，或者协程本身采用共享栈
		arg.pPollItems = (stPollItem_t*)malloc( nfds * sizeof( stPollItem_t ) );
	}
	
	memset( arg.pPollItems,0,nfds * sizeof(stPollItem_t) );

	// 当事件到来的时候，就调用这个callback。
	// 这个callback内部做了co_resume的动作
	arg.pfnProcess = OnPollProcessEvent;
	

	// 保存当前协程，便于调用OnPollProcessEvent时恢复协程
	// 这里取到的值不是和co_self一样吗？为什么不用co_self
	arg.pArg = GetCurrCo( co_get_curr_thread_env() );
	
	//2. add epoll
	for(nfds_t i=0;i<nfds;i++)
	{
		// 将事件添加到epoll中
		arg.pPollItems[i].pSelf = arg.fds + i;
		arg.pPollItems[i].pPoll = &arg;

		// 设置一个预处理的callback
		// 这个函数会在事件active的时候首先触发
		arg.pPollItems[i].pfnPrepare = OnPollPreparePfn; 

		struct epoll_event &ev = arg.pPollItems[i].stEvent;

		// 如果大于-1，说明要监听fd的相关事件了
		// 否则就是个timeout事件
		if( fds[i].fd > -1 )
		{
			// 这个相当于是个userdata, 当事件触发的时候，可以根据这个指针找到之前的数据
			ev.data.ptr = arg.pPollItems + i;

			// 将poll的事件类型转化为epoll
			ev.events = PollEvent2Epoll( fds[i].events );

			// 将fd添加入epoll中
			int ret = co_epoll_ctl( epfd,EPOLL_CTL_ADD, fds[i].fd, &ev );

			if (ret < 0 && errno == EPERM && nfds == 1 && pollfunc != NULL)
			{
				// 如果注册失败
				if( arg.pPollItems != arr )
				{
					free( arg.pPollItems );
					arg.pPollItems = NULL;
				}
				free(arg.fds);
				free(&arg);
				
				// 使用最原生的poll函数
				return pollfunc(fds, nfds, timeout);
			}
		}
		//if fail,the timeout would work
	}

	//3.add timeout
	// 获取当前时间
	unsigned long long now = GetTickMS();

	arg.ullExpireTime = now + timeout;	
	
	// 将其添加到超时链表中
	int ret = AddTimeout( ctx->pTimeout,&arg,now );

	// 如果出错了
	if( ret != 0 )
	{
		co_log_err("CO_ERR: AddTimeout ret %d now %lld timeout %d arg.ullExpireTime %lld",
				ret,now,timeout,arg.ullExpireTime);
		errno = EINVAL;

		if( arg.pPollItems != arr )
		{
			free( arg.pPollItems );
			arg.pPollItems = NULL;
		}
		free(arg.fds);
		free(&arg);

		return -__LINE__;
	}

	// 注册完事件，就yield。切换到其他协程
	// 当事件到来的时候，就会调用callback。
	co_yield_env( co_get_curr_thread_env() );

	// --------------------分割线---------------------------
	// 注意：！！这个时候，已经和上面的逻辑不在同一个时刻处理了
	// 这个时候，协程已经resume回来了！！

	// 清理数据

	// 将该项从超时链表中删除
	RemoveFromLink<stTimeoutItem_t,stTimeoutItemLink_t>( &arg );

	// 将该项涉及事件全部从epoll中删除掉
	// 事件一定要删除，不删除会出现误resume的问题
	for(nfds_t i = 0;i < nfds;i++)
	{
		int fd = fds[i].fd;
		if( fd > -1 )
		{
			co_epoll_ctl( epfd,EPOLL_CTL_DEL,fd,&arg.pPollItems[i].stEvent );
		}
		fds[i].revents = arg.fds[i].revents;
	}

	// 释放内存啦
	int iRaiseCnt = arg.iRaiseCnt;
	if( arg.pPollItems != arr )
	{
		free( arg.pPollItems );
		arg.pPollItems = NULL;
	}

	free(arg.fds);
	free(&arg);

	return iRaiseCnt;
}

int	co_poll( stCoEpoll_t *ctx,struct pollfd fds[], nfds_t nfds, int timeout_ms )
{
	return co_poll_inner(ctx, fds, nfds, timeout_ms, NULL);
}

void SetEpoll( stCoRoutineEnv_t *env,stCoEpoll_t *ev )
{
	env->pEpoll = ev;
}

/*
*  获取当前线程的epoll
*/
stCoEpoll_t *co_get_epoll_ct()
{
	if( !co_get_curr_thread_env() )
	{
		co_init_curr_thread_env();
	}
	return co_get_curr_thread_env()->pEpoll;
}

struct stHookPThreadSpec_t
{
	stCoRoutine_t *co;
	void *value;

	enum 
	{
		size = 1024
	};
};

void *co_getspecific(pthread_key_t key)
{
	stCoRoutine_t *co = GetCurrThreadCo();
	if( !co || co->cIsMain )
	{
		return pthread_getspecific( key );
	}
	return co->aSpec[ key ].value;
}


int co_setspecific(pthread_key_t key, const void *value)
{
	stCoRoutine_t *co = GetCurrThreadCo();
	if( !co || co->cIsMain )
	{
		return pthread_setspecific( key,value );
	}
	co->aSpec[ key ].value = (void*)value;
	return 0;
}

/*
* 在本协程中进行禁用hook功能
*/
void co_disable_hook_sys()
{
	stCoRoutine_t *co = GetCurrThreadCo();
	if( co )
	{
		co->cEnableSysHook = 0;
	}
}

/*
* 检测hook功能是否打开
*/
bool co_is_enable_sys_hook()
{
	stCoRoutine_t *co = GetCurrThreadCo();
	return ( co && co->cEnableSysHook );
}

/**
* 获取当前协程
*/
stCoRoutine_t *co_self()
{
	return GetCurrThreadCo();
}

/*
* 条件变量
*/
struct stCoCond_t;
struct stCoCondItem_t 
{
	stCoCondItem_t *pPrev;
	stCoCondItem_t *pNext;
	stCoCond_t *pLink;

	stTimeoutItem_t timeout;
};
struct stCoCond_t
{
	stCoCondItem_t *head;
	stCoCondItem_t *tail;
};
static void OnSignalProcessEvent( stTimeoutItem_t * ap )
{
	stCoRoutine_t *co = (stCoRoutine_t*)ap->pArg;
	co_resume( co );
}

stCoCondItem_t *co_cond_pop( stCoCond_t *link );
/*
*
* 语义上类似于pthread_cond_signal
* 唤醒等待队列中其中一个协程
* 
*/
int co_cond_signal( stCoCond_t *si )
{
	// 队列从中取出一个等待项，进行唤醒
	stCoCondItem_t * sp = co_cond_pop( si );
	if( !sp ) 
	{
		return 0;
	}
	RemoveFromLink<stTimeoutItem_t,stTimeoutItemLink_t>( &sp->timeout );

	// 将其添加到，pstActiveList中，在event_loop中处理
	AddTail( co_get_curr_thread_env()->pEpoll->pstActiveList,&sp->timeout );

	return 0;
}
/**
*  功能类似于pthread_cond_broadcast
*  唤醒等待队列中的所有的协程
*/
int co_cond_broadcast( stCoCond_t *si )
{
	for(;;)
	{
		stCoCondItem_t * sp = co_cond_pop( si );
		if( !sp ) return 0;

		RemoveFromLink<stTimeoutItem_t,stTimeoutItemLink_t>( &sp->timeout );

		AddTail( co_get_curr_thread_env()->pEpoll->pstActiveList,&sp->timeout );
	}

	return 0;
}

/**
* 语义上类似于 pthread_cond_wait
* 实际上最终执行了 co_yield
* @link 条件
* @ms 超时时间 如果为0或者-1，则永久不失效
*/
int co_cond_timedwait( stCoCond_t *link,int ms )
{
	stCoCondItem_t* psi = (stCoCondItem_t*)calloc(1, sizeof(stCoCondItem_t));
	psi->timeout.pArg = GetCurrThreadCo();
	psi->timeout.pfnProcess = OnSignalProcessEvent;

	if( ms > 0 )
	{ 
		unsigned long long now = GetTickMS();
		psi->timeout.ullExpireTime = now + ms;

		int ret = AddTimeout( co_get_curr_thread_env()->pEpoll->pTimeout,&psi->timeout,now );
		if( ret != 0 )
		{
			free(psi);
			return ret;
		}
	}
	
	AddTail( link, psi);

	co_yield_ct();


	RemoveFromLink<stCoCondItem_t,stCoCond_t>( psi );
	free(psi);

	return 0;
}
stCoCond_t *co_cond_alloc()
{
	return (stCoCond_t*)calloc( 1,sizeof(stCoCond_t) );
}
int co_cond_free( stCoCond_t * cc )
{
	free( cc );
	return 0;
}

/**
* 从队列中取出一个正在等待的协程
*/
stCoCondItem_t *co_cond_pop( stCoCond_t *link )
{
	stCoCondItem_t *p = link->head;
	if( p )
	{
		PopHead<stCoCondItem_t,stCoCond_t>( link );
	}
	return p;
}


