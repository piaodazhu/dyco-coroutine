#include "dyco/dyco_coroutine.h"

// socket handle
socket_t socket_f;
close_t close_f;
accept_t accept_f;
connect_t connect_f;
recv_t recv_f;
recvfrom_t recvfrom_f;
send_t send_f;
sendto_t sendto_f;
// IO multiplexing wait handle
epoll_wait_t epoll_wait_f;
poll_t poll_f;

__attribute__((constructor)) static void _inithook()
{
#ifdef COROUTINE_HOOK
	// socket handle
	socket_f = (socket_t)dlsym(RTLD_NEXT, "socket");
	close_f = (close_t)dlsym(RTLD_NEXT, "close");
	accept_f = (accept_t)dlsym(RTLD_NEXT, "accept");
	connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");
	recv_f = (recv_t)dlsym(RTLD_NEXT, "recv");
	recvfrom_f = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");
	send_f = (send_t)dlsym(RTLD_NEXT, "send");
	sendto_f = (sendto_t)dlsym(RTLD_NEXT, "sendto");
	// IO multiplexing wait handle
	epoll_wait_f = (epoll_wait_t)dlsym(RTLD_NEXT, "epoll_wait");
	poll_f = (poll_t)dlsym(RTLD_NEXT, "poll");
#else
	// socket handle
	socket_f = socket;
	close_f = close;
	accept_f = accept;
	connect_f = connect;
	recv_f = recv;
	recvfrom_f = recvfrom;
	send_f = send;
	sendto_f = sendto;
	// IO multiplexing wait handle
	epoll_wait_f = epoll_wait;
	poll_f = poll;
#endif
}
