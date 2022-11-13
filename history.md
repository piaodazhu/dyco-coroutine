## 2022-11-13: v2.1.0 released!

Updates:
1. 2-level priority scheduling to support urgent coroutine.
2. Adjusted the behaviors of the scheduler to make it more reasonable.
3. Recorrect the performance evaluation.
4. `libdyco` now can be linked with Cpp code.
5. Improve the documents, and fix some bugs.

## 2022-11-10: v2.0.0 released!

Updates:
1. Build the testbench and conduct the performance evaluation.
2. Add some debug macros.
3. Add some security patches for better rubustness.
4. Improve the documents. Bring some new plans in future works.

## 2022-11-5: v1.2.0 released!

Updates:
1. Support Asymmetric Coroutine & Asymmetric Coroutines Pool for manually schedualing.
2. Adjust scheduler loop policy for better event reaction.
3. Better multi-platform support.
3. Support Meson build.
4. Fix some bugs.
5. Improve the documents. Bring some new plans in future works.

## 2022-10-29: v1.1.0 released!

Updates:
1. Support Coroutines Pool for better performance.
2. Support `poll()` hook.
3. `libdyco` is build and can be linked easily.
4. Fix some bugs.
5. Improve the documents. Bring some new plans in future works.

## 2022-10-24: Happy Programers' Day!
Now `libdyco` can be installed, then programers can use it by simply link this lib when compiling.

## 2022-10-20: v1.0.0 released!

Today, the first version of dyco-coroutine has been finished. I hope this framework to be TRULY **practical** and **user-friendly**, rather than just a coroutine demo. This framework was first inspired by the `wangbojing/NtyCo` project.

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

There are still some future works:
1. Support different platforms. This part can be referred to `jamwt/libtask`.
2. Make dyco-coroutine a shared library: **libdyco**. Then programers can use it by simply link this lib when compiling.
3. Discover more feature requests and bugs by getting more people to use them.
4. Performance optimization. Using ucontext predestines the framework to not be the best at switching performance. But there is still room for optimization.
