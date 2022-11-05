#include "dyco_coroutine.h"

#define TIMEOUT_DEFAULT		3000
#define TIMEOUT_INFINIT		-1
#define TIMEOUT_NONE		0

int
dyco_socket(int domain, int type, int protocol)
{
	int fd = socket_f(domain, type, protocol);
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
		_waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

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
dyco_connect(int fd, struct sockaddr *name, socklen_t namelen)
{

	int ret = 0;
	int timeout = TIMEOUT_DEFAULT;

	while (1)
	{
		_waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

		ret = connect_f(fd, name, namelen);
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

	_waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recv_f(fd, buf, len, flags);

	return ret;
}

ssize_t
dyco_recvfrom(int fd, void *buf, size_t len, int flags,
		      struct sockaddr *src_addr, socklen_t *addrlen)
{
	int timeout = TIMEOUT_DEFAULT;

	_waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recvfrom_f(fd, buf, len, flags, src_addr, addrlen);
	return ret;
}

ssize_t
dyco_send(int fd, const void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = send_f(fd, ((char *)buf) + sent, len - sent, flags);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
		
		ret = send_f(fd, ((char *)buf) + sent, len - sent, flags);
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

	int ret = sendto_f(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);

		ret = sendto_f(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
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

int
socket(int domain, int type, int protocol)
{
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
	int sockfd = -1;
	int timeout = TIMEOUT_INFINIT;

	while (1)
	{
		_waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

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
	int ret = 0;
	int timeout = TIMEOUT_DEFAULT;

	while (1)
	{
		_waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET, timeout);

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

	return ret;
}

ssize_t
recv(int fd, void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_DEFAULT;

	_waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recv_f(fd, buf, len, flags);

	return ret;
}

ssize_t
recvfrom(int fd, void *buf, size_t len, int flags,
		 struct sockaddr *src_addr, socklen_t *addrlen)
{
	int timeout = TIMEOUT_DEFAULT;

	_waitev(fd, EPOLLIN | EPOLLERR | EPOLLHUP, timeout);

	int ret = recvfrom_f(fd, buf, len, flags, src_addr, addrlen);
	return ret;
}

ssize_t
send(int fd, const void *buf, size_t len, int flags)
{
	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = send_f(fd, ((char *)buf) + sent, len - sent, flags);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);
		
		ret = send_f(fd, ((char *)buf) + sent, len - sent, flags);
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
sendto(int fd, const void *buf, size_t len, int flags,
	       const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int timeout = TIMEOUT_INFINIT;
	int sent = 0;

	int ret = sendto_f(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
	if (ret == 0)
		return ret;
	if (ret > 0)
		sent += ret;

	while (sent < len)
	{
		_waitev(fd, EPOLLOUT | EPOLLERR | EPOLLHUP, timeout);

		ret = sendto_f(fd, ((char *)buf) + sent, len - sent, flags, dest_addr, addrlen);
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
	return close_f(fd);
}


#endif
