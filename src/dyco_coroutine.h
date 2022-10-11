#ifndef __COROUTINE_H__
#define __COROUTINE_H__

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
#include <fcntl.h>
#include <sys/time.h>
#include <netinet/tcp.h>

#include <ucontext.h>

#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <errno.h>

#include "sys_queue.h"
#include "sys_tree.h"

// #define COROUTINE_HOOK

#define DYCO_MAX_EVENTS		(1024 * 1024)
#define DYCO_MAX_STACKSIZE		(128 * 1024) // {http: 16*1024, tcp: 4*1024}

#define BIT(x)				(1 << (x))
#define SETBIT(d, x)			do {(d) |= BIT(x);} while(0)
#define CLRBIT(d, x)			do {(d) &= (~BIT(x));} while(0)
#define TESTBIT(d, x)			((d) & BIT(x))

TAILQ_HEAD(_dyco_coroutine_queue, _dyco_coroutine);
RB_HEAD(_dyco_coroutine_rbtree_sleep, _dyco_coroutine);
RB_HEAD(_dyco_coroutine_rbtree_wait, _dyco_coroutine);

typedef struct _dyco_coroutine_queue dyco_coroutine_queue;
typedef struct _dyco_coroutine_rbtree_sleep dyco_coroutine_rbtree_sleep;
typedef struct _dyco_coroutine_rbtree_wait dyco_coroutine_rbtree_wait;

extern pthread_key_t global_sched_key;
typedef struct _dyco_schedule dyco_schedule;
typedef struct _dyco_coroutine dyco_coroutine;

typedef void (*proc_coroutine)(void *);
typedef enum
{
	// COROUTINE_STATUS_WAIT_READ,
	// COROUTINE_STATUS_WAIT_WRITE,
	COROUTINE_STATUS_NEW, // *
	COROUTINE_STATUS_READY, // *
	COROUTINE_STATUS_EXITED, // *
	COROUTINE_STATUS_RUNNING, // *
	COROUTINE_STATUS_WAITING, // *
	COROUTINE_STATUS_SLEEPING, // *
	COROUTINE_STATUS_EXPIRED
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
	uint64_t		default_timeout;
	uint64_t		birth;

	// dynamic info
	int			coro_count;
	unsigned int		_id_gen;
	int 			num_new_events;
	dyco_coroutine 		*curr_thread;

	// coroutine containers
	dyco_coroutine_queue 		ready;
	dyco_coroutine_rbtree_sleep 	sleeping;
	dyco_coroutine_rbtree_wait 	waiting;

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
	dyco_coroutine_status 	status;
	uint32_t 		sched_count;
	uint64_t 		sleep_usecs;

	// static info
	// ownstack: set 1 to use co->stack when running, set 0 to use sched-stack.
	unsigned int 		id;
	unsigned int		ownstack; 

	// user customized data
	void 			*data;

	// events
	unsigned int 		events; 	// for single event
	int 			fd;		// for single event
	int			epollfd;	// for IO multiplexing 
	
	// container node
	TAILQ_ENTRY(_dyco_coroutine) 	ready_next;
	RB_ENTRY(_dyco_coroutine) 	sleep_node;
	RB_ENTRY(_dyco_coroutine) 	wait_node;
};


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

// coroutine api
int _resume(dyco_coroutine *co);
void _yield(dyco_coroutine *co);

static void dyco_coroutine_init(dyco_coroutine *co);
int dyco_coroutine_create(dyco_coroutine **new_co, proc_coroutine func, void *arg);
void dyco_coroutine_free(dyco_coroutine *co);
void dyco_coroutine_sleep(uint32_t msecs);

// schedular api
void _schedule_sched_sleep(dyco_coroutine *co, int msecs);
void _schedule_cancel_sleep(dyco_coroutine *co);
void _schedule_sched_wait(dyco_coroutine *co, int fd, unsigned int events);
dyco_coroutine *_schedule_cancel_wait(dyco_coroutine *co, int fd);

void dyco_schedule_run(void);


// epoll api
int dyco_epoll_create(void);
int dyco_epoll_ev_register_trigger(void);
int dyco_epoll_wait(struct timespec t);


// socket api wrapper

#ifndef COROUTINE_HOOK

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

#else

typedef int (*socket_t)(int domain, int type, int protocol);
extern socket_t socket_f;

typedef int (*close_t)(int);
extern close_t close_f;

typedef int (*accept_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern accept_t accept_f;

typedef int (*connect_t)(int, const struct sockaddr *, socklen_t);
extern connect_t connect_f;

typedef ssize_t (*send_t)(int sockfd, const void *buf, size_t len, int flags);
extern send_t send_f;

typedef ssize_t (*recv_t)(int sockfd, void *buf, size_t len, int flags);
extern recv_t recv_f;

typedef ssize_t (*sendto_t)(int sockfd, const void *buf, size_t len, int flags,
			    const struct sockaddr *dest_addr, socklen_t addrlen);
extern sendto_t sendto_f;

typedef ssize_t (*recvfrom_t)(int sockfd, void *buf, size_t len, int flags,
			      struct sockaddr *src_addr, socklen_t *addrlen);
extern recvfrom_t recvfrom_f;

int init_hook(void);

#endif

#endif
