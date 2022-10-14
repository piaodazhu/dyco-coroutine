#ifndef _DYCO_SIGNAL_H
#define _DYCO_SIGNAL_H

#include "dyco_coroutine.h"

int dyco_signal_waitchild(const pid_t child, int *status, int timeout)
{
	if (timeout == 0) {
		return waitpid(child, status, WNOHANG | WUNTRACED);
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	sigset_t sigmask;
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sigmask, NULL);
	int sigfd = signalfd(-1, &sigmask, SFD_NONBLOCK);

	struct epoll_event ev;
	ev.data.fd = sigfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;

	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, sigfd, &ev);
	_schedule_sched_wait(co, sigfd);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, sigfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, sigfd, NULL);
	
	close(sigfd);
	return waitpid(child, status, WNOHANG | WUNTRACED);
}


int dyco_signal_init(const sigset_t *mask)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	
	if (TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		return -1;
	}
	sigprocmask(SIG_BLOCK, mask, NULL);
	int sigfd = signalfd(-1, mask, SFD_NONBLOCK);
	co->sigfd = sigfd;
	SETBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL);

	struct epoll_event ev;
	ev.data.fd = sigfd;
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
	_schedule_sched_wait(co, ev.data.fd);
	return sigfd;
}


void dyco_signal_destroy()
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return;
	}
	if (!TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		return;
	}
	_schedule_cancel_wait(co, co->sigfd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, co->sigfd, NULL);
	CLRBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL);
	close(co->sigfd);

	return;
}


int dyco_signal_wait(struct signalfd_siginfo *sinfo, int timeout)
{
	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		return -1;
	}
	dyco_coroutine *co = sched->curr_thread;
	if (co == NULL) {
		return -1;
	}
	if (!TESTBIT(co->status, COROUTINE_FLAGS_WAITSIGNAL)) {
		return -1;
	}
	
	int ret;
	if (timeout == 0) {
		return read(co->sigfd, sinfo, sizeof(struct signalfd_siginfo));
	}

	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);

	return read(co->sigfd, sinfo, sizeof(struct signalfd_siginfo));
}

#endif