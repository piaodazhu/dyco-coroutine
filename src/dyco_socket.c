#include "dyco_coroutine.h"

#define TIMEOUT_DEFAULT		1000
#define TIMEOUT_INFINIT		-1
#define TIMEOUT_NONE		0

#define EV_POLL_EPOLL(x)	((uint32_t)(x) | EPOLLET)
#define EV_EPOLL_POLL(x)	((uint16_t)((x) & 0xffff))

// static uint32_t dyco_pollevent_2epoll(short events)
// {
// 	uint32_t e = 0;
// 	if (events & POLLIN)
// 		e |= EPOLLIN;
// 	if (events & POLLOUT)
// 		e |= EPOLLOUT;
// 	if (events & POLLHUP)
// 		e |= EPOLLHUP;
// 	if (events & POLLERR)
// 		e |= EPOLLERR;
// 	if (events & POLLRDNORM)
// 		e |= EPOLLRDNORM;
// 	if (events & POLLWRNORM)
// 		e |= EPOLLWRNORM;
// 	return e;
// }
// static short dyco_epollevent_2poll(uint32_t events)
// {
// 	short e = 0;
// 	if (events & EPOLLIN)
// 		e |= POLLIN;
// 	if (events & EPOLLOUT)
// 		e |= POLLOUT;
// 	if (events & EPOLLHUP)
// 		e |= POLLHUP;
// 	if (events & EPOLLERR)
// 		e |= POLLERR;
// 	if (events & EPOLLRDNORM)
// 		e |= POLLRDNORM;
// 	if (events & EPOLLWRNORM)
// 		e |= POLLWRNORM;
// 	return e;
// }
/*
 * dyco_poll_inner --> 1. sockfd--> epoll, 2 yield, 3. epoll x sockfd
 * fds :
 */

static int _wait_events(int fd, unsigned int events, int timeout)
{
	// events: uint32 epoll event
	// timeout: Specifying a negative value  in  timeout  means  an infinite  timeout. Specifying  a timeout of zero causes dyco_poll_inner() to return immediately
	
	int fastret = 0;
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = EV_EPOLL_POLL(events);
	if ((fastret = poll(&pfd, 1, 0)) != 0) {
		return fastret;
	}

	if (timeout == 0) {
		return fastret;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		printf("dyco_poll_inner() no scheduler!\n");
		return -1;
	}

	dyco_coroutine *co = sched->curr_thread;

	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, fd, &ev);
	dyco_schedule_sched_wait(co, fd, events, timeout);

	dyco_coroutine_yield(co);

	dyco_schedule_desched_wait(fd);
	// dyco_schedule_desched_sleepdown(co);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, fd, &ev);

	return 0;
}

// static int dyco_poll_inner(struct pollfd *fds, nfds_t nfds, int timeout)
// {
// 	// timeout: Specifying a negative value  in  timeout  means  an infinite  timeout. Specifying  a timeout of zero causes dyco_poll_inner() to return immediately
	
// 	int fastret = 0;
// 	if ((fastret = poll(fds, nfds, 0)) != 0) {
// 		return fastret;
// 	}

// 	if (timeout == 0) {
// 		return fastret;
// 	}

// 	dyco_schedule *sched = _get_sched();
// 	if (sched == NULL) {
// 		printf("dyco_poll_inner() no scheduler!\n");
// 		return -1;
// 	}

// 	dyco_coroutine *co = sched->curr_thread;

// 	int i = 0;
// 	for (i = 0; i < nfds; i++) {

// 		struct epoll_event ev;
// 		// ev.events = dyco_pollevent_2epoll(fds[i].events);
// 		ev.events = EV_POLL_EPOLL(fds[i].events);
// 		ev.data.fd = fds[i].fd;
// 		epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, fds[i].fd, &ev);
// // printf("tag0\n");
// 		dyco_schedule_sched_wait(co, fds[i].fd, ev.events, timeout);
// 	}
// 	dyco_coroutine_yield(co);
// // printf("tag1\n");
// 	for (i = 0; i < nfds; i++) {
// 		struct epoll_event ev;
// 		// ev.events = dyco_pollevent_2epoll(fds[i].events);
// 		ev.events = EV_POLL_EPOLL(fds[i].events);
// 		ev.data.fd = fds[i].fd;
// 		epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, fds[i].fd, &ev);
// 		dyco_schedule_desched_wait(fds[i].fd);
// 	}

// 	return nfds;
// }

int dyco_socket(int domain, int type, int protocol)
{

	int fd = socket(domain, type, protocol);
	if (fd == -1) {
		printf("Failed to create a new socket\n");
		return -1;
	}
	int ret = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (ret == -1) {
		close(ret);
		return -1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	return fd;
}

// dyco_accept
// return failed == -1, success > 0

int dyco_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	int sockfd = -1;
	int timeout = TIMEOUT_INFINIT;
	dyco_coroutine *co = _get_sched()->curr_thread;

	while (1)
	{
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLIN | POLLERR | POLLHUP;
		_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

		sockfd = accept(fd, addr, len);
		if (sockfd < 0)
		{
			if (errno == EAGAIN)
			{
				// printf("again\n");
				continue;
			}
			else if (errno == ECONNABORTED)
			{
				printf("accept : ECONNABORTED\n");
			}
			else if (errno == EMFILE || errno == ENFILE)
			{
				printf("accept : EMFILE || ENFILE\n");
			}
			return -1;
		}
		else
		{
			break;
		}
	}

	int ret = fcntl(sockfd, F_SETFL, O_NONBLOCK);
	if (ret == -1)
	{
		close(sockfd);
		return -1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

	return sockfd;
}

int dyco_connect(int fd, struct sockaddr *name, socklen_t namelen)
{

	int ret = 0;
	int timeout = TIMEOUT_DEFAULT;

	while (1)
	{

		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;
		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

		ret = connect(fd, name, namelen);
		if (ret == 0)
			break;

		if (ret == -1 && (errno == EAGAIN ||
				  errno == EWOULDBLOCK ||
				  errno == EINPROGRESS))
		{
			continue;
		}
		else
		{
			break;
		}
	}

	return ret;
}

// recv
//  add epoll first
//
ssize_t dyco_recv(int fd, void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_DEFAULT;
	// struct pollfd fds;
	// fds.fd = fd;
	// fds.events = POLLIN | POLLERR | POLLHUP;
	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

	int ret = recv(fd, buf, len, flags);
	// printf("dyco_recv: %d\n", ret);
	if (ret < 0)
	{
		// if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET)
			return -1;
		// printf("recv error : %d, ret : %d\n", errno, ret);
	}
	return ret;
}

ssize_t dyco_send(int fd, const void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_DEFAULT;
	int sent = 0;

	int ret = send(fd, ((char *)buf) + sent, len - sent, flags);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;

		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);
		ret = send(fd, ((char *)buf) + sent, len - sent, flags);
		// printf("send --> len : %d\n", ret);
		if (ret <= 0)
		{
			break;
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0)
		return ret;

	return sent;
}

ssize_t dyco_sendto(int fd, const void *buf, size_t len, int flags,
		    const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int timeout = TIMEOUT_DEFAULT;
	int sent = 0;

	while (sent < len)
	{
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;

		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);
		int ret = sendto(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
		if (ret <= 0)
		{
			if (errno == EAGAIN)
				continue;
			else if (errno == ECONNRESET)
			{
				return ret;
			}
			printf("send errno : %d, ret : %d\n", errno, ret);
			assert(0);
		}
		sent += ret;
	}
	return sent;
}

ssize_t dyco_recvfrom(int fd, void *buf, size_t len, int flags,
		      struct sockaddr *src_addr, socklen_t *addrlen)
{
	int timeout = TIMEOUT_DEFAULT;
	// struct pollfd fds;
	// fds.fd = fd;
	// fds.events = POLLIN | POLLERR | POLLHUP;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

	int ret = recvfrom(fd, buf, len, flags, src_addr, addrlen);
	if (ret < 0)
	{
		if (errno == EAGAIN)
			return ret;
		if (errno == ECONNRESET)
			return 0;

		printf("recv error : %d, ret : %d\n", errno, ret);
		assert(0);
	}
	return ret;
}

int dyco_close(int fd)
{
#if 0
	dyco_schedule *sched = _get_sched();

	dyco_coroutine *co = sched->curr_thread;
	if (co) {
		TAILQ_INSERT_TAIL(&_get_sched()->ready, co, ready_next);
		co->status |= BIT(COROUTINE_STATUS_FDEOF);
	}
#endif
	return close(fd);
}

#ifdef COROUTINE_HOOK

socket_t socket_f = NULL;

read_t read_f = NULL;
recv_t recv_f = NULL;
recvfrom_t recvfrom_f = NULL;

write_t write_f = NULL;
send_t send_f = NULL;
sendto_t sendto_f = NULL;

accept_t accept_f = NULL;
close_t close_f = NULL;
connect_t connect_f = NULL;

int init_hook(void)
{

	socket_f = (socket_t)dlsym(RTLD_NEXT, "socket");

	// read_f = (read_t)dlsym(RTLD_NEXT, "read");
	recv_f = (recv_t)dlsym(RTLD_NEXT, "recv");
	recvfrom_f = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");

	// write_f = (write_t)dlsym(RTLD_NEXT, "write");
	send_f = (send_t)dlsym(RTLD_NEXT, "send");
	sendto_f = (sendto_t)dlsym(RTLD_NEXT, "sendto");

	accept_f = (accept_t)dlsym(RTLD_NEXT, "accept");
	close_f = (close_t)dlsym(RTLD_NEXT, "close");
	connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");
}

int socket(int domain, int type, int protocol)
{

	if (!socket_f)
		init_hook();

	int fd = socket_f(domain, type, protocol);
	if (fd == -1)
	{
		printf("Failed to create a new socket\n");
		return -1;
	}
	int ret = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (ret == -1)
	{
		close(ret);
		return -1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

	return fd;
}

ssize_t recv(int fd, void *buf, size_t len, int flags)
{

	if (!recv_f)
		init_hook();

	printf("recv\n");
	int timeout = TIMEOUT_DEFAULT;
	// struct pollfd fds;
	// fds.fd = fd;
	// fds.events = POLLIN | POLLERR | POLLHUP;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

	int ret = recv_f(fd, buf, len, flags);
	if (ret < 0)
	{
		// if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET)
			return -1;
		// printf("recv error : %d, ret : %d\n", errno, ret);
	}
	return ret;
}

ssize_t recvfrom(int fd, void *buf, size_t len, int flags,
		 struct sockaddr *src_addr, socklen_t *addrlen)
{

	if (!recvfrom_f)
		init_hook();

	printf("recvfrom\n");
	int timeout = TIMEOUT_DEFAULT;
	// struct pollfd fds;
	// fds.fd = fd;
	// fds.events = POLLIN | POLLERR | POLLHUP;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

	int ret = recvfrom_f(fd, buf, len, flags, src_addr, addrlen);
	if (ret < 0)
	{
		if (errno == EAGAIN)
			return ret;
		if (errno == ECONNRESET)
			return 0;

		printf("recv error : %d, ret : %d\n", errno, ret);
		assert(0);
	}
	return ret;
}

ssize_t send(int fd, const void *buf, size_t len, int flags)
{

	if (!send_f)
		init_hook();

	printf("send\n");
	int timeout = TIMEOUT_DEFAULT;
	int sent = 0;

	int ret = send_f(fd, ((char *)buf) + sent, len - sent, flags);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;

		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);
		ret = send_f(fd, ((char *)buf) + sent, len - sent, flags);
		// printf("send --> len : %d\n", ret);
		if (ret <= 0)
		{
			break;
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0)
		return ret;

	return sent;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
	       const struct sockaddr *dest_addr, socklen_t addrlen)
{

	if (!sendto_f)
		init_hook();

	printf("sendto\n");
	int timeout = TIMEOUT_DEFAULT;
	// struct pollfd fds;
	// fds.fd = sockfd;
	// fds.events = POLLOUT | POLLERR | POLLHUP;

	_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

	int ret = sendto_f(sockfd, buf, len, flags, dest_addr, addrlen);
	if (ret < 0)
	{
		if (errno == EAGAIN)
			return ret;
		if (errno == ECONNRESET)
			return 0;

		printf("recv error : %d, ret : %d\n", errno, ret);
		assert(0);
	}
	return ret;
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{

	if (!accept_f)
		init_hook();

	printf("accept\n");
	int timeout = TIMEOUT_INFINIT;
	int sockfd = -1;
	dyco_coroutine *co = _get_sched()->curr_thread;

	while (1)
	{
		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLIN | POLLERR | POLLHUP;
		_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

		sockfd = accept_f(fd, addr, len);
		if (sockfd < 0)
		{
			if (errno == EAGAIN)
			{
				continue;
			}
			else if (errno == ECONNABORTED)
			{
				printf("accept : ECONNABORTED\n");
			}
			else if (errno == EMFILE || errno == ENFILE)
			{
				printf("accept : EMFILE || ENFILE\n");
			}
			return -1;
		}
		else
		{
			break;
		}
	}

	int ret = fcntl(sockfd, F_SETFL, O_NONBLOCK);
	if (ret == -1)
	{
		close(sockfd);
		return -1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

	return sockfd;
}

int close(int fd)
{

	if (!close_f)
		init_hook();

	return close_f(fd);
}

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{

	if (!connect_f)
		init_hook();

	printf("[%s:%s:%d]connect\n", __FILE__, __func__, __LINE__);
	int timeout = TIMEOUT_DEFAULT;
	int ret = 0;

	while (1)
	{

		// struct pollfd fds;
		// fds.fd = fd;
		// fds.events = POLLOUT | POLLERR | POLLHUP;
		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

		printf("dyco_poll_inner\n");
		ret = connect_f(fd, addr, addrlen);
		if (ret == 0)
			break;

		if (ret == -1 && (errno == EAGAIN ||
				  errno == EWOULDBLOCK ||
				  errno == EINPROGRESS))
		{
			continue;
		}
		else
		{
			break;
		}
	}

	printf("connect ret: %d\n", ret);

	return ret;
}

#endif
