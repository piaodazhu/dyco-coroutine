![DYCOIMG](./img/dyco.png)

# dyco-coroutine

☄️ **dyco-coroutine** is a dynamic coroutine framework for C. I hope this framework to be TRULY **practical** and **user-friendly**, rather than just a coroutine demo. This framework was first inspired by the `wangbojing/NtyCo` project. Some other projects are also referred, such as `cloudwu/coroutine`, `jamwt/libtask` and `stevedekorte/coroutine`.

With this framework, programers can achieve asynchronous I/O performance by programming in a synchronous manner. And I want this framework to work out-of-the-box: you just create a coroutine to run your functions, and I'll provide all tools that you need (such as scheduler, socket and synchronization), then all the functions run as coroutines do. Besides, I provide detailed examples that covers almost all supported features of dyco. Anyone can get started within 5 minite by reviewing and running this examples.

Features of dyco-coroutine:
1. Fully automated coroutine scheduling.
2. Either shared or separate stacks can be set for coroutines.
3. Socket/epoll hooks to automatically change the behavior of the socket/epoll API.
4. Wait signal events. Especially waitchild, which is works well with `fork()+exec()`.
5. Allow epoll inside each coroutine without blocking the scheduler.
6. Half duplex channel and Publish-Subcribe channel for coroutine communication.
7. Semaphore and Waitgroup for coroutine synchronization.
8. TLS/SSL non-block concurrent server support.
9. Scheduler and be stopped by any coroutine, and continue running in main process.
10. Multi-thread supported.

![DYCOARCH](./img/arch.png)

There are still some future works:
1. Support different platforms. This part can be referred to `jamwt/libtask`.
2. Make dyco-coroutine a shared library: **libdyco**. Then programers can use it by simply link this lib when compiling.
3. Discover more feature requests and bugs by getting more people to use them.
4. Performance optimization. Using ucontext predestines the framework to not be the best at switching performance. But there is still room for optimization.

You can give me a 🌟, or recommend it to others if you found dyco-coroutine helpful. And feel free to open issues or pull requests to make this project better. 🫶

# Build

```bash
# optional
$ sudo apt install libssl-dev
$ sudo apt install libhiredis-dev

# build
$ cd dyco-coroutine
$ make

# run
$ ./bin/xxx_example
```

# Get Started

```c
#include "dyco_coroutine.h"
#include <arpa/inet.h>

// Pass the arguments by pointer.
struct foobarargs {
	int	arg1;
	char	*arg2;
};

void foobar(void *arg)
{
	// Get the arguments by pointer.
	struct foobarargs *args = arg;

	int cid, fd, client, ret, status;

	// coroID can be obtained
	cid = dyco_coroutine_coroID();

	// Get Udata if necessary
	dyco_semaphore *sem;
	dyco_coroutine_getUdata(cid, &sem);

	// Create other coroutines if necessary
	dyco_coroutine_create(foobar, NULL);

	// Use dyco_coroutine_sleep() instead of sleep()
	dyco_coroutine_sleep(1000);

	// Use dyco_xx socket API if COROUTINE_HOOK is undefined
	fd = dyco_socket(AF_INET, SOCK_STREAM, 0);
	client = dyco_accept(fd, xxx, yyy);
	ret = dyco_recv(client, xxx, yyy, 0);
	ret = dyco_recv(client, xxx, yyy, 0);
	dyco_close(client);

	// Use normal socket API if COROUTINE_HOOK is defined
	fd = socket(AF_INET, SOCK_STREAM, 0);
	client = accept(fd, xxx, yyy);
	ret = recv(client, xxx, yyy, 0);
	ret = recv(client, xxx, yyy, 0);
	close(client);

	ret = fork();
	if (ret == 0) {
		exec(...)
	}
	else if (ret < 0) {
		return;
	}

	// Wait child for 3000 ms. Set timeout to -1 to wait until child process is finished
	ret = dyco_signal_waitchild(ret, &status, 3000);
	
	// Use dyco_coroutine_sleep() instead of epoll_wait()
	dyco_epoll_wait(...)
	
	return;
}

int main()
{
	// Optional: Pass the arguments by pointer.
	struct foobarargs *args = calloc(1, sizeof(struct foobarargs));

	// Create the corotine
	int cid = dyco_coroutine_create(foobar, args);
	
	// Optional: Set stack if necessary
	char st[4096];
	dyco_coroutine_setStack(cid, st, 4096);

	// Optional: Create semaphore, channel, waitgroup or pubsubchannel
	dyco_semaphore *sem = dyco_semaphore_create(3);

	// Optional: Set Udata if necessary
	dyco_coroutine_setUdata(cid, sem);

	// Run 
	dyco_schedule_run();

	return 0;
}
```

# User APIs

## Coroutine

Some basic coroutine method is defined. 
```c
int dyco_coroutine_create(proc_coroutine func, void *arg);
void dyco_coroutine_free(dyco_coroutine *co);
void dyco_coroutine_sleep(uint32_t msecs);
int dyco_coroutine_waitRead(int fd, int timeout);
int dyco_coroutine_waitWrite(int fd, int timeout);
int dyco_coroutine_waitRW(int fd, int timeout);

int dyco_coroutine_coroID();
int dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize);
int dyco_coroutine_getStack(int cid, void **stackptr, size_t *stacksize);
int dyco_coroutine_setUdata(int cid, void *udata);
int dyco_coroutine_getUdata(int cid, void **udata);
int dyco_coroutine_getSchedCount(int cid);
```

## Scheduler

```c
int dyco_schedule_run();
int  dyco_schedule_create(size_t stack_size, uint64_t loopwait_timeout);
void  dyco_schedule_free(dyco_schedule *sched);

int dyco_schedule_schedID();
int dyco_schedule_setUdata(void *udata);
int dyco_schedule_getUdata(void **udata);
int dyco_schedule_getCoroCount();
```

## Scheduler Call

```c
int dyco_schedcall_sigprocmask(int __how, sigset_t *__set, sigset_t *__oset);
void dyco_schedcall_stop();
void dyco_schedcall_abort();
```

## epoll
```c
int dyco_epoll_init();
void dyco_epoll_destroy();
int dyco_epoll_add(int fd, struct epoll_event *ev);
int dyco_epoll_del(int fd, struct epoll_event *ev);
int dyco_epoll_wait(struct epoll_event *events, int maxevents, int timeout);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```


## Signal

```c
int dyco_signal_waitchild(const pid_t child, int *status, int timeout);
int dyco_signal_init(sigset_t *mask);
void dyco_signal_destroy();
int dyco_signal_wait(struct signalfd_siginfo *sinfo, int timeout);
```


## Half Duplex Channel

```c
dyco_channel* dyco_channel_create(size_t size);
void dyco_channel_destroy(dyco_channel **chan);
ssize_t dyco_channel_send(dyco_channel *chan, void *buf, size_t size, int timeout);
ssize_t dyco_channel_recv(dyco_channel *chan, void *buf, size_t maxsize, int timeout);
```


## Publish-subscribe Channel
```c
dyco_pubsubchannel* dyco_pubsub_create(size_t size);
void dyco_pubsub_destroy(dyco_pubsubchannel **pschan);
ssize_t dyco_pubsub_publish(dyco_pubsubchannel *pschan, void *buf, size_t size);
ssize_t dyco_pubsub_subscribe(dyco_pubsubchannel *pschan, void *buf, size_t maxsize, int timeout);
```

## Waitgroup
```c
dyco_waitgroup* dyco_waitgroup_create(int suggest_size);
void dyco_waitgroup_destroy(dyco_waitgroup **group);
int dyco_waitgroup_add(dyco_waitgroup* group, int cid);
int dyco_waitgroup_done(dyco_waitgroup* group);
int dyco_waitgroup_wait(dyco_waitgroup* group, int target, int timeout);
```

## Semaphore
```c
dyco_semaphore* dyco_semaphore_create(size_t value);
void dyco_semaphore_destroy(dyco_semaphore **sem);
int dyco_semaphore_wait(dyco_semaphore *sem, int timeout);
int dyco_semaphore_signal(dyco_semaphore *sem);
```

## Socket
```c
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
```

## SSL
```c
int dyco_SSL_accept(SSL *ssl);
int dyco_SSL_connect(SSL *ssl);
int dyco_SSL_read(SSL *ssl, void *buf, int num);
int dyco_SSL_write(SSL *ssl, const void *buf, int num);
```

# About Coroutine

TBD
