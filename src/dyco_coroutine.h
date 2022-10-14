#ifndef __COROUTINE_H__
#define __COROUTINE_H__

// ------ 1. Included Headers
#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <netinet/tcp.h>

#include <ucontext.h>

#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <errno.h>

#include "sys_queue.h"
#include "sys_tree.h"
#include "dyco_htable.h"

// ------ 2. User Configurations
#define COROUTINE_HOOK
#define DYCO_MAX_EVENTS			256
#define DYCO_MAX_STACKSIZE		(64 * 1024)
#define DYCO_DEFAULT_TIMEOUT		3000000

// ------ 3. Macro Utils
#define BIT(x)				(1 << (x))
#define SETBIT(d, x)			do {(d) |= BIT(x);} while(0)
#define CLRBIT(d, x)			do {(d) &= (~BIT(x));} while(0)
#define TESTBIT(d, x)			(((d) & BIT(x)) != 0)

// ------ 4. Data Structure Defination
TAILQ_HEAD(_dyco_coroutine_queue, _dyco_coroutine);
RB_HEAD(_dyco_coroutine_rbtree_sleep, _dyco_coroutine);
// RB_HEAD(_dyco_coroutine_rbtree_wait, _dyco_coroutine);

typedef struct _dyco_coroutine_queue dyco_coroutine_queue;
typedef struct _dyco_coroutine_rbtree_sleep dyco_coroutine_rbtree_sleep;
// typedef struct _dyco_coroutine_rbtree_wait dyco_coroutine_rbtree_wait;

extern pthread_key_t global_sched_key;
typedef struct _dyco_schedule dyco_schedule;
typedef struct _dyco_coroutine dyco_coroutine;

typedef void (*proc_coroutine)(void *);
typedef enum
{
	COROUTINE_STATUS_NEW,
	COROUTINE_STATUS_READY,
	COROUTINE_STATUS_EXITED,
	COROUTINE_STATUS_RUNNING,
	COROUTINE_STATUS_SLEEPING,

	COROUTINE_FLAGS_OWNSTACK,
	COROUTINE_FLAGS_WAITING,
	COROUTINE_FLAGS_EXPIRED,
	COROUTINE_FLAGS_IOMULTIPLEXING,
	COROUTINE_FLAGS_WAITSIGNAL
} dyco_coroutine_status;

struct _dyco_schedule
{
	// context
	ucontext_t		ctx;
	
	// events
	int			epollfd;
	struct epoll_event	eventlist[DYCO_MAX_EVENTS];

	// stack
	void			*stack;
	size_t			stack_size;

	// static info
	int			sched_id;
	uint64_t		loopwait_timeout;
	uint64_t		birth;

	// dynamic info
	unsigned int		coro_count;
	unsigned int		_cid_gen;
	unsigned int 		num_new_events;
	dyco_coroutine 		*curr_thread;

	// coroutine containers
	dyco_coroutine_queue 		ready;
	dyco_coroutine_rbtree_sleep 	sleeping;
	// dyco_coroutine_rbtree_wait 	waiting;

	dyco_htable		fd_co_map;
	dyco_htable		cid_co_map;
};

struct _dyco_coroutine
{
	// function and arg
	proc_coroutine		func;
	void 			*arg;

	// scheduler and context
	dyco_schedule 		*sched;
	ucontext_t		ctx;

	// coroutine stack
	void 			*stack;
	size_t 			stack_size;

	// dynamic info
	uint32_t 		status;
	uint32_t 		sched_count;
	uint64_t 		sleep_usecs;

	// static info
	// ownstack: set 1 to use co->stack when running, set 0 to use sched-stack.
	int 			cid;
	// int			ownstack; 

	// user customized data
	void 			*data;

	// events
	int 			fd;		// for single event
	int			epollfd;	// for IO multiplexing 
	int			sigfd;		// for wait signals
	
	// container node
	TAILQ_ENTRY(_dyco_coroutine) 	ready_next;
	RB_ENTRY(_dyco_coroutine) 	sleep_node;
	// RB_ENTRY(_dyco_coroutine) 	wait_node;
};

// ------ 5. Function Utils
static inline dyco_schedule *_get_sched(void)
{
	return pthread_getspecific(global_sched_key);
}

static inline uint64_t _diff_usecs(uint64_t t1, uint64_t t2)
{
	return t2 - t1;
}

static inline uint64_t _usec_now(void)
{
	struct timeval t1 = {0, 0};
	gettimeofday(&t1, NULL);
	return t1.tv_sec * 1000000 + t1.tv_usec;
}

static inline int _coroutine_sleep_cmp(dyco_coroutine *co1, dyco_coroutine *co2)
{
	if (co1->sleep_usecs < co2->sleep_usecs)
		return -1;
	if (co1->sleep_usecs == co2->sleep_usecs)
		return 0;
	else
		return 1;
}

static inline int _coroutine_wait_cmp(dyco_coroutine *co1, dyco_coroutine *co2)
{
	if (co1->fd < co2->fd)
		return -1;
	else if (co1->fd == co2->fd)
		return 0;
	else
		return 1;
}

// ------ 6. Inner Primes
// coroutine
static void _init_coro(dyco_coroutine *co);
int _resume(dyco_coroutine *co);
void _yield(dyco_coroutine *co);

// scheduler
void _schedule_sched_sleep(dyco_coroutine *co, int msecs);
void _schedule_cancel_sleep(dyco_coroutine *co);
void _schedule_sched_wait(dyco_coroutine *co, int fd);
dyco_coroutine *_schedule_cancel_wait(dyco_coroutine *co, int fd);


// ------ 7. User APIs
// coroutine
int dyco_coroutine_create(proc_coroutine func, void *arg);
void dyco_coroutine_free(dyco_coroutine *co);
void dyco_coroutine_sleep(uint32_t msecs);

int dyco_coroutine_coroID();
int dyco_coroutine_schedID();
int dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize);
int dyco_coroutine_getStack(int cid, void **stackptr, size_t *stacksize);
int dyco_coroutine_setUdata(int cid, void *udata);
int dyco_coroutine_getUdata(int cid, void **udata);
int dyco_coroutine_getSchedCount(int cid);

// scheduler
void dyco_schedule_run(void);
int  dyco_schedule_create(size_t stack_size, uint64_t loopwait_timeout);
void  dyco_schedule_free(dyco_schedule *sched);

// epoll
int dyco_epoll_init();
void dyco_epoll_destroy();
int dyco_epoll_add(int __fd, struct epoll_event *__ev);
int dyco_epoll_del(int __fd, struct epoll_event *__ev);
int dyco_epoll_wait(struct epoll_event *__events, int __maxevents, int __timeout);

// child signal
int dyco_signal_waitchild(const pid_t __child, int *__status, int __timeout);
int dyco_signal_init(const sigset_t *__mask);
void dyco_signal_destroy();
int dyco_signal_wait(struct signalfd_siginfo *__sinfo, int __timeout);

// socket
int dyco_socket(int domain, int type, int protocol);
int dyco_close(int fd);
int dyco_accept(int fd, struct sockaddr *addr, socklen_t *len);
int dyco_connect(int fd, struct sockaddr *name, socklen_t namelen);
ssize_t dyco_send(int fd, const void *buf, size_t len, int flags);
ssize_t dyco_recv(int fd, void *buf, size_t len, int flags);
ssize_t dyco_sendto(int fd, const void *buf, size_t len, int flags,
		    const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t dyco_recvfrom(int fd, void *buf, size_t len, int flags,
		      struct sockaddr *src_addr, socklen_t *addrlen);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);


// ------ 8. Hook Function Type
typedef int (*socket_t)(int domain, int type, int protocol);
typedef int (*close_t)(int);
typedef int (*accept_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
typedef int (*connect_t)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
typedef ssize_t (*send_t)(int sockfd, const void *buf, size_t len, int flags);
typedef ssize_t (*recv_t)(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t (*sendto_t)(int sockfd, const void *buf, size_t len, int flags,
			    const struct sockaddr *dest_addr, socklen_t addrlen);
typedef ssize_t (*recvfrom_t)(int sockfd, void *buf, size_t len, int flags,
			      struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t (*epoll_wait_t)(int epfd, struct epoll_event *events, 
				int maxevents, int timeout);

// ------ 9. Hook Function Handler
extern socket_t socket_f;
extern close_t close_f;
extern accept_t accept_f;
extern connect_t connect_f;
extern send_t send_f;
extern sendto_t sendto_f;
extern recv_t recv_f;
extern recvfrom_t recvfrom_f;
extern epoll_wait_t epoll_wait_f;

#endif
