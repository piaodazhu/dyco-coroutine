#include "dyco/dyco_coroutine.h"
#include <arpa/inet.h>

char* ip;
unsigned short port;

int init_client(void)
{
	int clientfd = dyco_socket(AF_INET, SOCK_STREAM, 0);
	if (clientfd <= 0) {
		printf("socket failed\n");
		return -1;
	}

	struct sockaddr_in serveraddr = {0};
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = inet_addr(ip);

	int result = dyco_connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (result != 0) {
		printf("connect failed\n");
		return -2;
	}

	return clientfd;
}

void client(void *arg)
{
	int clientfd = init_client();
	char *msg = "Hello. I'm TCP Client.";
	int length;
	char buf[2048];

	while (1) {

		length = dyco_send(clientfd, msg, strlen(msg), 0);
		printf("echo send length : %d\n", length);
		if (length <= 0) {
			perror("dyco_send() wrong!\n");
			DYCO_ABORT();
		}
		length = dyco_recv(clientfd, buf, 2048, 0);
		printf("echo recv length : %d\n", length);
		if (length <= 0) {
			perror("dyco_recv() wrong!\n");
			DYCO_ABORT();
		}
		dyco_coroutine_sleep(1000);
	}
	dyco_close(clientfd);
}


int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: ./socket_client [ip] [port]\n");
		return 1;
	}
	ip = argv[1];
	port = atoi(argv[2]);
	dyco_coroutine_create(client, NULL);
	
	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	return 0;
}
