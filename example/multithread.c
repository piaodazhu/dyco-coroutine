#include "dyco_coroutine.h"
#include <arpa/inet.h>
#define MAX_CLIENT_NUM_PERTHEAD	1000000
struct thread_arg {
	int		coreidx;
	unsigned short	port;
	pthread_t	tid;
};

struct reader_arg {
	int			fd;
	struct thread_arg	*targ;
};

void server_reader(void *arg) {
	struct reader_arg *rarg = arg;
	int fd = rarg->fd;
	int ret = 0;

	while (1) {
		char buf[1024] = {0};
		ret = dyco_recv(fd, buf, 1024, 0);
		if (ret > 0) {
			if(fd > MAX_CLIENT_NUM_PERTHEAD)
				DYCO_ABORT();
			printf("[thread %lu] read from client: %.*s\n", rarg->targ->tid, ret, buf);

			ret = dyco_send(fd, buf, strlen(buf), 0);
			if (ret == -1) {
				dyco_close(fd);
				printf("[thread %lu] server_reader send failed: fd=%d\n", rarg->targ->tid, fd);
				break;
			}
		} else if (ret == 0) {	
			dyco_close(fd);
			printf("[thread %lu] server_reader close: fd=%d\n", rarg->targ->tid, fd);
			break;
		}
	}
	dyco_close(fd);
	return;
}

void server(void *arg)
{
	struct thread_arg *args = arg;
	unsigned short port = args->port;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd > 0);

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 100);
	printf("[thread %lu] listen port : %d\n", args->tid, port);

	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = dyco_accept(fd, (struct sockaddr*)&remote, &len);
		printf("[thread %lu] new client comming\n", args->tid);
		struct reader_arg *rarg = calloc(1, sizeof(struct reader_arg));
		rarg->fd = cli_fd;
		rarg->targ = args;
		dyco_coroutine_create(server_reader, rarg);
	}
	dyco_close(fd);
	return;
}

void* tfunc(void *arg)
{
	struct thread_arg *args = arg;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(args->coreidx, &cpuset);
	pthread_setaffinity_np(args->tid, sizeof(cpu_set_t), &cpuset);
	
	int cid = dyco_coroutine_create(server, args);
	char stack[4096];
	dyco_coroutine_setStack(cid, stack, 4096);

	printf("[thread %lu] scheduler start running.\n", args->tid);
	dyco_schedule_run();
	printf("[thread %lu] scheduler stopped.\n", args->tid);
	return NULL;
}

int main()
{
	int corenum = sysconf(_SC_NPROCESSORS_CONF);
	struct thread_arg *args = (struct thread_arg*)malloc(corenum * sizeof(struct thread_arg));
	pthread_t *tid = (pthread_t*)malloc(corenum * sizeof(pthread_t));
	int i;
	unsigned short base_port = 5000;

	for (i = 0; i < corenum; i++) {
		args[i].coreidx = i;
		args[i].port = base_port + i;
		pthread_create(&args[i].tid, NULL, tfunc, &args[i]);
	}
	for (i = 0; i < corenum; i++) {
		pthread_join(args[i].tid, NULL);
	}
	return 0;
}