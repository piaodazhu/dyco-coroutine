#include "dyco/dyco_coroutine.h"

#include <arpa/inet.h>
#include <assert.h>

#define MAX_CLIENT_NUM			1000000

void server_reader(void *arg) {
	int fd = *(int *)arg;
	free(arg);
	int ret = 0;

	printf("server_reader start\n");
	while (1) {
		char buf[1024] = {0};
		ret = dyco_recv(fd, buf, 1024, 0);
		if (ret > 0) {
			if(fd > MAX_CLIENT_NUM) 
				DYCO_ABORT();
			printf("read from client: %.*s\n", ret, buf);

			ret = dyco_send(fd, buf, strlen(buf), 0);
			if (ret == -1) {
				dyco_close(fd);
				printf("server_reader send failed: fd=%d\n",fd);
				break;
			}
		} else if (ret == 0) {	
			dyco_close(fd);
			printf("server_reader close: fd=%d\n",fd);
			break;
		}
	}
	dyco_close(fd);
	return;
}

void server(void *arg) {

	unsigned short port = *(unsigned short *)arg;
	free(arg);

	int fd = dyco_socket(AF_INET, SOCK_STREAM, 0);
	assert(fd > 0);

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 20);
	printf("listen port : %d\n", port);

	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = dyco_accept(fd, (struct sockaddr*)&remote, &len);
		printf("new client comming\n");
		int *client = malloc(sizeof(int));
		*client = cli_fd;
		dyco_coroutine_create(server_reader, client);
	}
	dyco_close(fd);
	return;
}

int main() 
{
	int i = 0;
	unsigned short base_port = 5000;
	for (i = 0;i < 100;i ++) {
		unsigned short *port = calloc(1, sizeof(unsigned short));
		*port = base_port + i;
		dyco_coroutine_create(server, port);
	}

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	return 0;
}
