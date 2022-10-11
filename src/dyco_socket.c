#include "dyco_coroutine.h"

#define TIMEOUT_DEFAULT		3000
#define TIMEOUT_INFINIT		-1
#define TIMEOUT_NONE		0

static int
_wait_events(int fd, unsigned int events, int timeout)
{
	// events: uint32 epoll event
	// timeout: Specifying a negative value  in  timeout  means  an infinite  timeout. Specifying  a timeout of zero causes dyco_poll_inner() to return immediately
	
	int fastret = 0;
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = (uint16_t)(events & 0xffff);
	if ((fastret = poll(&pfd, 1, 0)) != 0) {
		return fastret;
	}

	if (timeout == 0) {
		return fastret;
	}

	dyco_schedule *sched = _get_sched();
	if (sched == NULL) {
		printf("dyco_poll_inner() no scheduler!\n");
		abort();
	}

	dyco_coroutine *co = sched->curr_thread;

	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	epoll_ctl(sched->epollfd, EPOLL_CTL_ADD, fd, &ev);
	_schedule_sched_wait(co, fd, events);
	_schedule_sched_sleep(co, timeout);

	_yield(co);

	_schedule_cancel_sleep(co);
	_schedule_cancel_wait(co, fd);
	epoll_ctl(sched->epollfd, EPOLL_CTL_DEL, fd, &ev);

	return 0;
}

int
dyco_socket(int domain, int type, int protocol)
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

int
dyco_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	int sockfd = -1;
	int timeout = TIMEOUT_INFINIT;

	while (1)
	{
		_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

		sockfd = accept(fd, addr, len);
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

int
dyco_connect(int fd, struct sockaddr *name, socklen_t namelen)
{

	int ret = 0;
	int timeout = TIMEOUT_DEFAULT;

	while (1)
	{
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

ssize_t
dyco_recv(int fd, void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_DEFAULT;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recv(fd, buf, len, flags);

	return ret;
}

ssize_t
dyco_recvfrom(int fd, void *buf, size_t len, int flags,
		      struct sockaddr *src_addr, socklen_t *addrlen)
{
	int timeout = TIMEOUT_DEFAULT;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recvfrom(fd, buf, len, flags, src_addr, addrlen);
	return ret;
}

ssize_t
dyco_send(int fd, const void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = send(fd, ((char *)buf) + sent, len - sent, flags);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
		
		ret = send(fd, ((char *)buf) + sent, len - sent, flags);
		if (ret <= 0)
		{
			if (errno == EAGAIN)
				continue;
			else if (errno == ECONNRESET)
			{
				return ret;
			}
			else {
				printf("send() errno : %d, ret : %d\n", errno, ret);
				break;
			}
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0)
		return ret;

	return sent;
}

ssize_t
dyco_sendto(int fd, const void *buf, size_t len, int flags,
			const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = sendto(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);

		ret = sendto(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
		if (ret <= 0)
		{
			if (errno == EAGAIN)
				continue;
			else if (errno == ECONNRESET)
			{
				return ret;
			}
			else {
				printf("sendto() errno : %d, ret : %d\n", errno, ret);
				break;
			}
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0)
		return ret;
	
	return sent;
}

int
dyco_close(int fd)
{
	return close(fd);
}

#ifdef COROUTINE_HOOK

socket_t socket_f = NULL;
close_t close_f = NULL;
accept_t accept_f = NULL;
connect_t connect_f = NULL;
recv_t recv_f = NULL;
recvfrom_t recvfrom_f = NULL;
send_t send_f = NULL;
sendto_t sendto_f = NULL;

int
init_hook(void)
{

	socket_f = (socket_t)dlsym(RTLD_NEXT, "socket");
	close_f = (close_t)dlsym(RTLD_NEXT, "close");

	accept_f = (accept_t)dlsym(RTLD_NEXT, "accept");
	connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");
	
	recv_f = (recv_t)dlsym(RTLD_NEXT, "recv");
	recvfrom_f = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");
	send_f = (send_t)dlsym(RTLD_NEXT, "send");
	sendto_f = (sendto_t)dlsym(RTLD_NEXT, "sendto");

	return 0;
}

int
socket(int domain, int type, int protocol)
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


int
accept(int fd, struct sockaddr *addr, socklen_t *len)
{

	if (!accept_f)
		init_hook();

	int sockfd = -1;
	int timeout = TIMEOUT_INFINIT;

	while (1)
	{
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


int
connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{

	if (!connect_f)
		init_hook();

	// printf("[%s:%s:%d]connect\n", __FILE__, __func__, __LINE__);
	int ret = 0;
	int timeout = TIMEOUT_DEFAULT;

	while (1)
	{
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

ssize_t
recv(int fd, void *buf, size_t len, int flags)
{

	if (!recv_f)
		init_hook();

	int timeout = TIMEOUT_DEFAULT;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recv(fd, buf, len, flags);

	return ret;
}

ssize_t
recvfrom(int fd, void *buf, size_t len, int flags,
		 struct sockaddr *src_addr, socklen_t *addrlen)
{

	if (!recvfrom_f)
		init_hook();

	int timeout = TIMEOUT_DEFAULT;

	_wait_events(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recvfrom(fd, buf, len, flags, src_addr, addrlen);
	return ret;
}

ssize_t
send(int fd, const void *buf, size_t len, int flags)
{

	if (!send_f)
		init_hook();

	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = send(fd, ((char *)buf) + sent, len - sent, flags);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
		
		ret = send(fd, ((char *)buf) + sent, len - sent, flags);
		if (ret <= 0)
		{
			if (errno == EAGAIN)
				continue;
			else if (errno == ECONNRESET)
			{
				return ret;
			}
			else {
				printf("send() errno : %d, ret : %d\n", errno, ret);
				break;
			}
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0)
		return ret;

	return sent;
}

ssize_t
sendto(int sockfd, const void *buf, size_t len, int flags,
	       const struct sockaddr *dest_addr, socklen_t addrlen)
{

	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = sendto(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_wait_events(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);

		ret = sendto(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
		if (ret <= 0)
		{
			if (errno == EAGAIN)
				continue;
			else if (errno == ECONNRESET)
			{
				return ret;
			}
			else {
				printf("sendto() errno : %d, ret : %d\n", errno, ret);
				break;
			}
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0)
		return ret;
	
	return sent;
}


int
close(int fd)
{

	if (!close_f)
		init_hook();

	return close_f(fd);
}


#endif
