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

TBD

# Get Started

TBD

# Examples

TBD

# About Coroutine

TBD
