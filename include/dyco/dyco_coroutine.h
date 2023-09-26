#ifndef __COROUTINE_H__
#define __COROUTINE_H__
#define DYCO_VERSION		"v2.1.1"

// ------ 1. Included Headers
#define _GNU_SOURCE
#include <dlfcn.h>

#define NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#ifndef __cplusplus
# include <stdatomic.h>
#else
# include <atomic>
# define _Atomic(X) std::atomic< X >
#endif

#include <time.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <sys/socket.h>

#include "dyco/dyco_ucontext.h"
#include "sys_queue.h"
#include "sys_tree.h"
#include "dyco_htable.h"

#ifdef __cplusplus 
extern "C"{
#endif

// ------ 2. User Configurations
#define		COROUTINE_HOOK
#define		DYCO_SSL_ENABLE
// #define 	DYCO_RANDOM_WAITFD
#define		DYCO_MAX_EVENTS			1024
#define		DYCO_MAX_STACKSIZE		(64 * 1024)
#define		DYCO_DEFAULT_STACKSIZE		(16 * 1024)
#define		DYCO_DEFAULT_TIMEOUT		10000000
#define		DYCO_DEFAULT_CHANNELSIZE	256
#define		DYCO_URGENT_MAXEXEC		128
#define		DYCO_URGENT_MAXWAIT		128

// ------ 3. Data Structure Defination
// 3.0 sublist
typedef struct notifylist dyco_sublist;
struct notifylist {
	int	notifyfd;
	dyco_sublist	*next;
};

// 3.1 scheduler
typedef struct dyco_coroutine dyco_coroutine;
typedef struct dyco_coroutine_queue dyco_coroutine_queue;
typedef struct dyco_coroutine_rbtree_sleep dyco_coroutine_rbtree_sleep;
typedef struct dyco_coropool_list dyco_coropool_list;
TAILQ_HEAD(dyco_coroutine_queue, dyco_coroutine);
RB_HEAD(dyco_coroutine_rbtree_sleep, dyco_coroutine);
SLIST_HEAD(dyco_coropool_list, dyco_coroutine);

typedef struct schedcall dyco_schedcall;
typedef enum {
	CALLNUM_SIGPROCMASK,
	CALLNUM_SCHED_STOP,
	CALLNUM_SCHED_ABORT
} dyco_schedcall_num;

struct schedcall {
	dyco_schedcall_num	callnum;
	int	ret;
	void	*arg;
};

typedef struct dyco_schedule dyco_schedule;
typedef enum
{
	SCHEDULE_STATUS_READY,
	SCHEDULE_STATUS_RUNNING,
	SCHEDULE_STATUS_STOPPED,
	SCHEDULE_STATUS_ABORTED,
	SCHEDULE_STATUS_DONE
} dyco_schedule_status;

struct dyco_schedule
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
	unsigned int		sched_id;
	uint64_t		loopwait_timeout;
	uint64_t		birth;

	// dynamic info
	unsigned int		coro_count;
	unsigned int		_cid_gen;
	dyco_coroutine 		*curr_thread;
	dyco_schedcall		schedcall;
	dyco_schedule_status	status;

	// user customized data
	void 			*udata;

	// coroutine containers
	dyco_coroutine_queue 		urgent_ready;
	dyco_coroutine_queue 		ready;
	dyco_coroutine_rbtree_sleep 	sleeping;

	dyco_htable		fd_co_map;
	dyco_htable		cid_co_map;
};

// 3.2 coroutine
typedef struct coroutinepool dyco_coropool;
struct coroutinepool {
	int			totalsize;
	int			activenum;
	int			stacksize;
	dyco_sublist		*sublist;
	dyco_coropool_list	freelist;
};

typedef void (*proc_coroutine)(void *);
typedef enum
{
	COROUTINE_STATUS_NEW,
	COROUTINE_STATUS_READY,
	COROUTINE_STATUS_EXITED,
	COROUTINE_STATUS_RUNNING,
	COROUTINE_STATUS_SLEEPING,
	COROUTINE_STATUS_SCHEDCALL,
	COROUTINE_STATUS_KILLED,

	COROUTINE_FLAGS_URGENT,
	COROUTINE_FLAGS_OWNSTACK,
	COROUTINE_FLAGS_ALLOCSTACKMEM,
	COROUTINE_FLAGS_WAITING,
	COROUTINE_FLAGS_EXPIRED,
	COROUTINE_FLAGS_IOMULTIPLEXING,
	COROUTINE_FLAGS_WAITSIGNAL,
	COROUTINE_FLAGS_INCOROPOOL,
	COROUTINE_FLAGS_ASYMMETRIC
} dyco_coroutine_status;

struct dyco_coroutine
{
	// function and arg
	proc_coroutine		func;
	void 			*arg;

	// scheduler and context
	dyco_schedule 		*sched;
	ucontext_t		ctx;
	ucontext_t		ret;

	// coroutine stack
	void 			*stack;
	size_t 			stack_size;

	// dynamic info
	uint32_t 		status;
	uint32_t 		sched_count;
	uint64_t 		sleep_usecs;
	sigset_t		old_sigmask;

	// static info
	int 			cid;

	// coroutine pool
	dyco_coropool		*cpool;

	// user customized data
	void 			*udata;

	// events
	int			epollfd;	// for IO multiplexing 
	int			sigfd;		// for wait signals
	
	// container node
	TAILQ_ENTRY(dyco_coroutine) 	ready_next;
	SLIST_ENTRY(dyco_coroutine) 	cpool_next;
	RB_ENTRY(dyco_coroutine) 	sleep_node;
};

// 3.3 channel
typedef struct half_duplex_channel dyco_channel;
typedef enum {
	HDC_STATUS_NOP,
	HDC_STATUS_EMPTY,
	HDC_STATUS_FULL,
	HDC_STATUS_WANTREAD,
	HDC_STATUS_WANTWRITE,
	HDC_STATUS_WANTCLOSE,
	HDC_STATUS_CANCLOSE
} half_duplex_channel_status;

struct half_duplex_channel {
	size_t maxsize;
	size_t msglen;
	void *msg;

	int r_notifyfd;
	int w_notifyfd;
	half_duplex_channel_status status;
};

// 3.4 pubsub
typedef struct waitgroup dyco_waitgroup;
struct waitgroup {
	int	tot_size;
	int	finished;
	dyco_htable	cid_set;
	dyco_htable	target_sublist_map;
	dyco_sublist	*final_sublist;
};

// 3.5 waitgroup
typedef struct pubsub_channel dyco_pubsubchannel;
typedef enum {
	PSC_STATUS_NOP,
	PSC_STATUS_EMPTY,
	PSC_STATUS_TRANS,
	PSC_STATUS_CLOSE
} dyco_pubsub_channel_status;

struct pubsub_channel {
	size_t	maxsize;
	size_t	msglen;
	void	*msg;
	int	pub_notifyfd;

	size_t		sub_num;
	size_t		ack_rem;
	dyco_sublist	*sublist;
	dyco_pubsub_channel_status	status;
};

// 3.6 semaphore
typedef struct semaphore dyco_semaphore;
typedef struct notifylist dyco_semwait_queue;
typedef struct notifylist dyco_semwait_node;

struct semaphore {
	int	semval;
	dyco_semwait_queue	*wqueue;
	dyco_semwait_node	*wtail;
};

// ------ 4. Utils
#define BIT(x)				(1 << (x))
#define SETBIT(d, x)			do {(d) |= BIT(x);} while(0)
#define CLRBIT(d, x)			do {(d) &= (~BIT(x));} while(0)
#define TESTBIT(d, x)			(((d) & BIT(x)) != 0)

#define DYCO_ABORT()			do { \
						printf("[x] error occurs in %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__); \
						exit(1); \
					} while(0)
#define DYCO_MUST(logic) 		if (!(logic)) { \
						printf("[?] condition is not met in %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__); \
						exit(1); \
					}
#define DYCO_MUSTNOT(logic) 		if ((logic)) { \
						printf("[?] condition is not met in %s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__); \
						exit(1); \
					}
#define DYCO_WARNIF(logic, msg) 	if ((logic)) { \
						printf("[!] warning in %s:%s:%d: %s\n", __FILE__, __FUNCTION__, __LINE__, msg); \
					}

#define DYCO_PRINTSTACK(co)		do { \
						char dummy; \
						size_t size = (char*)(co->stack + co->stack_size) - &dummy; \
						printf("[i] stack size is [%lu] in %s:%s:%d\n", size, __FILE__, __FUNCTION__, __LINE__); \
					} while (0)
extern pthread_key_t global_sched_key;

static inline dyco_schedule *get_sched()
{
	return (dyco_schedule*)pthread_getspecific(global_sched_key);
}

static inline uint64_t diff_usecs(uint64_t t1, uint64_t t2)
{
	return t2 - t1;
}

static inline int tv_ms(struct timeval *tv)
{
	return tv->tv_sec * 1000 + tv->tv_usec/1000;
}

static inline uint64_t usec_now()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000 + ts.tv_nsec/1000;
}

static inline int coroutine_sleep_cmp(dyco_coroutine *co1, dyco_coroutine *co2)
{
	if (co1->sleep_usecs < co2->sleep_usecs)
		return -1;
	if (co1->sleep_usecs == co2->sleep_usecs)
		return 0;
	else
		return 1;
}

// ------ 5. Inner Primes
// User DO NOT use them. Use User APIs is enough in most case.
// 5.1 coroutine
dyco_coroutine* newcoro();
void freecoro(dyco_coroutine *co);
void savestk(dyco_coroutine *co);
void loadstk(dyco_coroutine *co);
int checkstk(dyco_coroutine *co);
void coro_abort(void *arg);
int resume(dyco_coroutine *co);
void yield(dyco_coroutine *co);
int waitev(int fd, unsigned int events, int timeout);

// 5.2 scheduler
void schedule_sched_sleep(dyco_coroutine *co, int msecs);
void schedule_cancel_sleep(dyco_coroutine *co);
void schedule_sched_wait(dyco_coroutine *co, int fd, unsigned int events);
void schedule_sched_waitR(dyco_coroutine *co, int fd);
void schedule_sched_waitW(dyco_coroutine *co, int fd);
void schedule_sched_waitRW(dyco_coroutine *co, int fd);
int schedule_cancel_wait(dyco_coroutine *co, int fd);
int schedule_callexec(dyco_schedule *sched);
void schedule_stop(dyco_schedule *sched);
void schedule_abort(dyco_schedule *sched);

// ------ 6. User APIs
// 6.1 coroutine & coroutines pool
int dyco_coroutine_create(proc_coroutine func, void *arg);
int dyco_coroutine_create_urgent(proc_coroutine func, void *arg);
void dyco_coroutine_sleep(uint32_t msecs);
void dyco_coroutine_abort();
int dyco_coroutine_waitRead(int fd, int timeout);
int dyco_coroutine_waitWrite(int fd, int timeout);
int dyco_coroutine_waitRW(int fd, int timeout);

int dyco_coroutine_coroID();
int dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize);
int dyco_coroutine_getStack(int cid, void **stackptr, size_t *stacksize);
int dyco_coroutine_checkStack();

int dyco_coroutine_setUdata(int cid, void *udata);
int dyco_coroutine_getUdata(int cid, void **udata);
int dyco_coroutine_getSchedCount(int cid);
int dyco_coroutine_setUrgent(int cid);
int dyco_coroutine_unsetUrgent(int cid);

dyco_coropool* dyco_coropool_create(int totalsize, size_t stacksize);
dyco_coropool* dyco_coropool_resize(dyco_coropool* cp, int newsize);
int dyco_coropool_destroy(dyco_coropool** cp);
int dyco_coropool_available(dyco_coropool* cp);
int dyco_coropool_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout);
int dyco_coropool_obtain_urgent(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout);

// 6.2 scheduler
int dyco_schedule_run();
int  dyco_schedule_create(size_t stack_size, uint64_t loopwait_timeout);
void  dyco_schedule_free(dyco_schedule *sched);

int dyco_schedule_schedID();
int dyco_schedule_setUdata(void *udata);
int dyco_schedule_getUdata(void **udata);
int dyco_schedule_getCoroCount();

// 6.3 scheduler call
int dyco_schedcall_sigprocmask(int how, sigset_t *set, sigset_t *oset);
void dyco_schedcall_stop();
void dyco_schedcall_abort();

// 6.4 poll/epoll
int dyco_epoll_init();
void dyco_epoll_destroy();
int dyco_epoll_add(int fd, struct epoll_event *ev);
int dyco_epoll_del(int fd, struct epoll_event *ev);
int dyco_epoll_wait(struct epoll_event *events, int maxevents, int timeout);
// int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
// int poll(struct pollfd *fds, nfds_t nfds, int timeout);

// 6.5 signal
int dyco_signal_waitchild(const pid_t child, int *status, int timeout);
int dyco_signal_init(sigset_t *mask);
void dyco_signal_destroy();
int dyco_signal_wait(struct signalfd_siginfo *sinfo, int timeout);

// 6.6 half duplex channel
dyco_channel* dyco_channel_create(size_t size);
void dyco_channel_destroy(dyco_channel **chan);
ssize_t dyco_channel_send(dyco_channel *chan, void *buf, size_t size, int timeout);
ssize_t dyco_channel_recv(dyco_channel *chan, void *buf, size_t maxsize, int timeout);

// 6.7 publish-subscribe channel
dyco_pubsubchannel* dyco_pubsub_create(size_t size);
void dyco_pubsub_destroy(dyco_pubsubchannel **pschan);
ssize_t dyco_pubsub_publish(dyco_pubsubchannel *pschan, void *buf, size_t size);
ssize_t dyco_pubsub_subscribe(dyco_pubsubchannel *pschan, void *buf, size_t maxsize, int timeout);

// 6.8 wait group
dyco_waitgroup* dyco_waitgroup_create(int suggest_size);
void dyco_waitgroup_destroy(dyco_waitgroup **group);
int dyco_waitgroup_add(dyco_waitgroup* group, int cid);
int dyco_waitgroup_done(dyco_waitgroup* group);
int dyco_waitgroup_wait(dyco_waitgroup* group, int target, int timeout);

// 6.9 semaphore
dyco_semaphore* dyco_semaphore_create(size_t value);
void dyco_semaphore_destroy(dyco_semaphore **sem);
int dyco_semaphore_wait(dyco_semaphore *sem, int timeout);
int dyco_semaphore_signal(dyco_semaphore *sem);

// 6.10 socket
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

// 6.11 ssl
#ifdef DYCO_SSL_OK
#ifdef DYCO_SSL_ENABLE
#include <openssl/ssl.h>
#include <openssl/err.h>
int dyco_SSL_accept(SSL *ssl);
int dyco_SSL_connect(SSL *ssl);
int dyco_SSL_read(SSL *ssl, void *buf, int num);
int dyco_SSL_write(SSL *ssl, const void *buf, int num);
#endif
#endif

// 6.12 asymmetric coroutine & asymmetric coroutines pool
int dyco_coroutine_isasymmetric(int cid);
int dyco_asymcoro_create(proc_coroutine func, void *arg);
int dyco_asymcororesume(int cid);
void dyco_asymcoroyield();
void dyco_asymcoro_free(int cid);
int dyco_asymcoro_coroID();
int dyco_asymcoro_setStack(int cid, void *stackptr, size_t stacksize);
int dyco_asymcoro_getStack(int cid, void **stackptr, size_t *stacksize);
int dyco_asymcoro_setUdata(int cid, void *udata);
int dyco_asymcoro_getUdata(int cid, void **udata);

dyco_coropool* dyco_asymcpool_create(int totalsize, size_t stacksize);
dyco_coropool* dyco_asymcpool_resize(dyco_coropool* cp, int newsize);
int dyco_asymcpool_destroy(dyco_coropool** cp);
int dyco_asymcpool_available(dyco_coropool* cp);
int dyco_asymcpool_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout);
void dyco_asymcpool_return(int cid);

// ------ 7. Hook Functions
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
typedef int (*epoll_wait_t)(int epfd, struct epoll_event *events, 
				int maxevents, int timeout);
typedef int (*poll_t)(struct pollfd *fds, nfds_t nfds, int timeout);

extern socket_t socket_f;
extern close_t close_f;
extern accept_t accept_f;
extern connect_t connect_f;
extern send_t send_f;
extern sendto_t sendto_f;
extern recv_t recv_f;
extern recvfrom_t recvfrom_f;
extern epoll_wait_t epoll_wait_f;
extern poll_t poll_f;

// ------ 8. Version
static char* dyco_version()
{
	return DYCO_VERSION;
}

#ifdef __cplusplus 
	}
#endif

#endif
