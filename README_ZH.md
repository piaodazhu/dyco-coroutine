![DYCOIMG](./img/dyco.png)

# dyco-coroutine

![GitHub last commit](https://img.shields.io/github/last-commit/piaodazhu/dyco-coroutine)
![GitHub top language](https://img.shields.io/github/languages/top/piaodazhu/dyco-coroutine)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/piaodazhu/dyco-coroutine)

> Language: [English](./README.md) | [中文](./README_ZH.md)

☄️ **dyco-coroutine** 是一个纯C的动态协程框架。作者希望这个框架是真正**实用**且**好用**，而不仅仅是一个Demo。这个项目最早受到`wangbojing/NtyCo`启发。也参考了一些别的项目，比如`cloudwu/coroutine`, `jamwt/libtask` 和 `stevedekorte/coroutine`。

使用这个框架，开发人员可以用同步的编程方式达到接近异步程序的I/O性能。作者希望这个框架是开箱即用的：没有过多的依赖和环境限制，使用的时候直接将子程序创建为协程即可，协程相关的所有工具（如调度器，套接字接口，协程同步）都由框架提供，所有子程序就会自动按照协程的方式运行。此外，作者提供了详细的示例，覆盖了dyco目前几乎所有的特性。通过快速阅读并运行这些示例，任何人都可以快速上手这个框架。

`dyco-coroutine`的特性:
1. 全自动的协程调度。
2. 支持为协程设定独立的运行栈，或者设定采用共享栈。
3. 支持协程池以达到更高的效率。
4. 支持Socket/epoll钩子，可以改变一些上层网络API的行为。
5. 支持等待信号事件。尤其是等待子进程，适合跟`fork()+exec()`配合使用。
6. 支持在协程内部使用epoll，而不阻塞整个调度器循环。
7. 提供了半双工信道和发布订阅信道，支持协程通信。
8. 提供了信号量和等待组功能，支持实现协程同步。
9. 支持非阻塞的TLS/SSL并发服务。
10. 调度器及其管理的协程可以被暂停，然后在适当的时机恢复。
11. 支持多线程。

![DYCOARCH](./img/arch.png)

预计未来还有一些工作方向:
1. Testbench.
2. 更好的跨平台支持。目前的跨平台设计参考自`jamwt/libtask`。但是只支持Linux系统。未来的工作可以参考`heiher/hev-task-system`和`tboox/tbox`。
3. 发掘更多的特性需求，并不断完善。寻找Bug并修复。
4. 性能优化。使用ucontext作为底层切换策略，意味着这个框架在协程切换性能上很难做到顶尖的表现（相比于直接用汇编）。但是依然可能存在其他的优化空间。
5. 更丰富的构建方式。减少构建的复杂性，而不是增加。

如果这个项目对你有用，可以给个星星以表支持。也可以推荐给其他人用用看。吐槽或者有问题随时提issue。如果有想法也可以提PR，共同合作让这个项目变得更好。🌈

# 构建

```bash
# 可选
$ sudo apt install libssl-dev
$ sudo apt install libhiredis-dev

# make编译
$ cd dyco-coroutine
$ make

# 运行
$ ./bin/xxx_example

# 可选: 编译之后安装共享库
$ sudo make install
# 编译应用的时候直接链接
$ gcc -o someoutput somecode.c -ldyco
# 卸载共享库
$ sudo make uninstall

# 1. 准备工作
# 可选
$ sudo apt install libssl-dev
$ sudo apt install libhiredis-dev
# 可选，仅用于Meson + ninja构建
$ sudo apt install meson
$ sudo apt install ninja-build
$ sudo apt install pkg-config

# 2. 使用GNU make构建
# 运行make
$ cd dyco-coroutine
$ make

# 运行示例
$ ./bin/xxx_example

# 可选: 编译之后安装共享库
$ sudo make install

# 编译应用的时候直接链接
$ gcc -o someoutput somecode.c -ldyco

# 卸载共享库
$ sudo make uninstall

# 3. 使用Meson + ninja构建
# 运行meson构建工具
$ meson build_dir
$ cd build_dir
$ meson compile

# 运行示例
$ build_dir/example/xxx_example

# 可选: 编译之后安装共享库
$ cd build_dir
$ sudo ninja install

# 编译应用的时候直接链接
$ gcc -o someoutput somecode.c -ldyco

# 卸载共享库
$ cd build_dir
$ sudo ninja uninstall
```

# 快速开始

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
	
	// Use dyco_epoll api if COROUTINE_HOOK is not defined
	dyco_epoll_create(...)
	dyco_epoll_add(...)
	dyco_epoll_wait(...)
	dyco_epoll_del(...)
	dyco_epoll_destroy(...)

	// Use normal epoll api if COROUTINE_HOOK is defined
	epoll_wait(...)
	
	return;
}

int main()
{
	// Optional: Pass the arguments by pointer.
	struct foobarargs *args = calloc(1, sizeof(struct foobarargs));

	// Create the corotine
	int cid = dyco_coroutine_create(foobar, args);
	
	// Optional: Set stack if necessary
	dyco_coroutine_setStack(cid, NULL, 4096);

	// Optional: Create semaphore, channel, waitgroup or pubsubchannel
	dyco_semaphore *sem = dyco_semaphore_create(3);

	// Optional: Set Udata if necessary
	dyco_coroutine_setUdata(cid, sem);

	// Run 
	dyco_schedule_run();

	return 0;
}
```

# 用户接口

## Coroutine & Coroutines Pool

这里定义了一些基本的协程方法。注意`sleep/wait/coroID`只能在协程里调用。

`setStack` 是**可选的**。如果在协程运行之前没有设置栈，那么默认协程是在调度器提供的共享栈上运行。否则，则协程将运行在独立的预设栈上。共享栈节约内存空间，但是每次切换都会拷贝出去，造成性能开销。因此如果一个协程需要频繁地切出切入，最好为其设置独立栈。详见 `example/*`。

这里也提供了**协程池**，来防止频繁地创建释放协程和栈的内存空间。任何时候都可以创建协程池，创建完成后可以修改协程池的大小。协程池中的协程在用户函数执行完成后会自动归还到协程池。通过调用`obtain`，你可以从协程池中获得一个空闲协程。详见 `example/coropool_example.c`。

```c
// return the coroutine ID on success, < 0 on error
int dyco_coroutine_create(proc_coroutine func, void *arg);

void dyco_coroutine_sleep(uint32_t msecs);

// set timeout to -1 for persistent waiting. set timeout to 0 for no waiting
// return > 0 on success, 0 on timeout, < 0 on error
int dyco_coroutine_waitRead(int fd, int timeout);
int dyco_coroutine_waitWrite(int fd, int timeout);
int dyco_coroutine_waitRW(int fd, int timeout);

// return ID of current coroutine
int dyco_coroutine_coroID();

// stacksize: 0 to cancel independent stack
// return 1 on successfully set, 0 on successfully cancel, < 0 on error
int dyco_coroutine_setStack(int cid, void *stackptr, size_t stacksize);

int dyco_coroutine_getStack(int cid, void **stackptr, size_t *stacksize);

// return 0 on success
int dyco_coroutine_setUdata(int cid, void *udata);
int dyco_coroutine_getUdata(int cid, void **udata);

// return total yield times of a coroutine 
int dyco_coroutine_getSchedCount(int cid);

// return NULL on error
dyco_coropool* dyco_coropool_create(int totalsize, size_t stacksize);
dyco_coropool* dyco_coropool_resize(dyco_coropool* cp, int newsize);

// return 0 on success
int dyco_coropool_destroy(dyco_coropool** cp);

// return number of available coroutine in this pool
int dyco_coropool_available(dyco_coropool* cp);

// obtain a coroutine from the pool. 
// If there is no free coroutine, wait timeout
// return 0 on timeout, -1 on error, > 0 on success
int dyco_coropool_obtain(dyco_coropool* cp, proc_coroutine func, void *arg, int timeout);
```

## Scheduler

这里定义了基本的调度器方法。实际上，dyco里的协程调度是由调度器自动完成的。`create`是可选的，`run`在大多数情况下足矣。详见`example/*`。

```c
// return 0 when done, 1 when stopped, < 0 on error
int dyco_schedule_run();

// stack_size: shared stack memory size
// loopwait_timeout: max delay (ms) of starting new coroutine
// return 0 on success
int dyco_schedule_create(size_t stack_size, uint64_t loopwait_timeout);

void dyco_schedule_free(dyco_schedule *sched);

// return ID of current scheduler
int dyco_schedule_schedID();

int dyco_schedule_setUdata(void *udata);

int dyco_schedule_getUdata(void **udata);

// return total number to coroutines of current scheduler
int dyco_schedule_getCoroCount();
```

## Scheduler Call

调度器调用为协程提供了影响调度器行为的接口，比如阻塞某些信号，暂停或者终止调度器。详见`example/stop_abort.c`。

```c
// see sigprocmask
int dyco_schedcall_sigprocmask(int __how, sigset_t *__set, sigset_t *__oset);
// stop current scheduler and make it return as soon as possible
void dyco_schedcall_stop();
// shutdown the scheduler and kill its all coroutines
void dyco_schedcall_abort();
```

## poll/epoll

尽管开发人员使用协程是为了以同步的编程方式达到接近异步的I/O性能，但是dyco同样也支持传统的I/O多路复用编程方式。如果`COROUTINE_HOOK`宏被开启，调用`epoll_wait`将不会阻塞调度循环。方便起见，dyco也提供了`dyco_epoll_xxx`系列接口。详见`example/epoll.c`。

```c
// return 0 on success
int dyco_epoll_init();
void dyco_epoll_destroy();
int dyco_epoll_add(int fd, struct epoll_event *ev);
int dyco_epoll_del(int fd, struct epoll_event *ev);

// return number of ready events, < 0 on error
int dyco_epoll_wait(struct epoll_event *events, int maxevents, int timeout);

// see epoll_wait
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

// see poll
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```


## Signal

通过调用`dyco_signal`系列接口，等待某些信号的出现将不会导致调度循环被阻塞。`waitchild + fork + exec`在某些情况下尤其有用。等待其他信号也是支持的。

注意，调用`signal_init`后，指定的信号将被暂时屏蔽，直到`signal_destroy`被调用。

详见`example/signal.c`。

```c
// return pid of the child on success, < 0 on error
int dyco_signal_waitchild(const pid_t child, int *status, int timeout);
// return 0 on success
int dyco_signal_init(sigset_t *mask);

void dyco_signal_destroy();

// return sizeof struct signalfd_siginfo on success, < 0 on error
int dyco_signal_wait(struct signalfd_siginfo *sinfo, int timeout);
```

## Half Duplex Channel

半双工信道用于提供**简单**的协程通信。`send`只有在信道空闲或者消息被接收方读走的情况下才能立刻返回，否则将会引起等待。详见`example/channel.c`。

```c
// size: max length of the message
dyco_channel* dyco_channel_create(size_t size);

void dyco_channel_destroy(dyco_channel **chan);

// return the actual sent length, 0 on channel closed, < 0 on error
ssize_t dyco_channel_send(dyco_channel *chan, void *buf, size_t size, int timeout);

// return the actual received length, 0 on channel closed, < 0 on error
ssize_t dyco_channel_recv(dyco_channel *chan, void *buf, size_t maxsize, int timeout);
```

## Publish-subscribe Channel

发布订阅信道提供了**1对N**协程通信。如果没有订阅者，`publish`将会失败。对于每个消息，订阅者都需要提前调用`subscribe`来订阅，换言之，订阅不会自动持续。详见`example/pubsub.c`。


```c
// size: max length of the message
dyco_pubsubchannel* dyco_pubsub_create(size_t size);

void dyco_pubsub_destroy(dyco_pubsubchannel **pschan);

// return the actual published length, 0 on no subscriber, < 0 on error
ssize_t dyco_pubsub_publish(dyco_pubsubchannel *pschan, void *buf, size_t size);

// return the actual received length, < 0 on error
ssize_t dyco_pubsub_subscribe(dyco_pubsubchannel *pschan, void *buf, size_t maxsize, int timeout);
```

## Waitgroup

等待组提供了**N对N**的协程同步。协程可以通过调用`add`来加入某个等待组，调用`done`来提醒这个等待组，或者通过调用`wait`来等待一定数量的协程完成。详见`example/waitgroup.c`。 

```c
// suggest_size: estimated max number of coroutines on the waitgroup
dyco_waitgroup* dyco_waitgroup_create(int suggest_size);

void dyco_waitgroup_destroy(dyco_waitgroup **group);

// add coroutine to the waitgroup
int dyco_waitgroup_add(dyco_waitgroup* group, int cid);

// tell the waitgroup this coroutine is done
int dyco_waitgroup_done(dyco_waitgroup* group);

// target: target wait number, set it to -1 to wait until all group member done
// timeout: unit is ms, and set to -1 for persistent waiting
// return > 0 on success, < 0 on error
int dyco_waitgroup_wait(dyco_waitgroup* group, int target, int timeout);
```

## Semaphore

严格来讲，每个dyco调度器只运行在一个线程上，每个线程同一时刻最多只有一个协程在运行。但是这些协程看起来像是同时发生的。因此，信号量在某些场合可能派上用场，比如我们需要控制同时活跃的连接数。详见`example/semaphore.c`。

```c
// value: initial value of the semaphore
dyco_semaphore* dyco_semaphore_create(size_t value);
void dyco_semaphore_destroy(dyco_semaphore **sem);

// return 0 on success, < 0 on error or timeout
int dyco_semaphore_wait(dyco_semaphore *sem, int timeout);

// return 0 on success, < 0 on error
int dyco_semaphore_signal(dyco_semaphore *sem);
```

## Socket

dyco提供的socket接口可以像系统的socket接口一样调用，而不会阻塞调度循环。
 
如果`COROUTINE_HOOK`宏被开启，系统的socket接口就会表现出dyco提供的socket相同的行为。而且，这也会改变一些高级的网络接口的行为，因为它们底层也调用了系统的socket接口。比如redis的同步接口，和gethostbyname等接口，将不会阻塞调度循环. 详见`example/socket_client.c example/socket_server.c example/network.c`.

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

dyco提供的SSL接口能巨大改善SSL通信的性能。但如果要使用这些接口，`libssl`和`libcrypto`需要在编译dyco之前预先安装。详见`example/ssl_server.c example/ssl_client`。

**缺少SSL部分也是可以的**，dyco的核心部分不需要任何依赖。

```c
// return 1 on success, else on error
int dyco_SSL_accept(SSL *ssl);

// return 1 on success, else on error
int dyco_SSL_connect(SSL *ssl);

// see SSL_read
int dyco_SSL_read(SSL *ssl, void *buf, int num);

// see SSL_write
int dyco_SSL_write(SSL *ssl, const void *buf, int num);
```

# About Coroutine

有3种实现协程切换的主流方案：
- 汇编指令。它比其他两种方案快，但是对于每个平台都要写一些不同的汇编指令代码。
- `setjmp()`/`longjmp()`。它比ucontext快。但是我个人认为这是最不合适的协程切换实现方案。`setjmp()`/`longjmp()`会导致栈丢失，为了避免这种情况，你不得不去设置env_buf的栈指针。这个数据结构在不同平台上截然不同，那为什么不直接用汇编？
- ucontext。ucontext包含了4个平台无关的接口，因此协程切换代码会相对优雅。不幸的是**ucontext已经被移除出POSIX，因为使用带有空括号的函数声明器（而不是原型格式的参数类型声明器）是一个过时的特性**。ucontext在Linux目前还是可以使用的，而且不会为dyco带来问题。因此我选择了这个方案。为了支持更多的平台，dyco在用户层构建uncontext，这部分参考了`jamwt/libtask`的实现。
