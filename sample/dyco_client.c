/*
 *  Author : WangBoJing , email : 1989wangbojing@gmail.com
 * 
 *  Copyright Statement:
 *  --------------------
 *  This software is protected by Copyright and the information contained
 *  herein is confidential. The software may not be copied and the information
 *  contained herein may not be used or disclosed except with the written
 *  permission of Author. (C) 2017
 * 
 *

****       *****                                      *****
  ***        *                                       **    ***
  ***        *         *                            *       **
  * **       *         *                           **        **
  * **       *         *                          **          *
  *  **      *        **                          **          *
  *  **      *       ***                          **
  *   **     *    ***********    *****    *****  **                   ****
  *   **     *        **           **      **    **                 **    **
  *    **    *        **           **      *     **                 *      **
  *    **    *        **            *      *     **                **      **
  *     **   *        **            **     *     **                *        **
  *     **   *        **             *    *      **               **        **
  *      **  *        **             **   *      **               **        **
  *      **  *        **             **   *      **               **        **
  *       ** *        **              *  *       **               **        **
  *       ** *        **              ** *        **          *   **        **
  *        ***        **               * *        **          *   **        **
  *        ***        **     *         **          *         *     **      **
  *         **        **     *         **          **       *      **      **
  *         **         **   *          *            **     *        **    **
*****        *          ****           *              *****           ****
                                       *
                                      *
                                  *****
                                  ****



 *
 */



#include "dyco_coroutine.h"

#include <arpa/inet.h>



#define dyco_SERVER_IPADDR		"127.0.0.1"
#define dyco_SERVER_PORT			9096

int init_client(void) {

	int clientfd = dyco_socket(AF_INET, SOCK_STREAM, 0);
	if (clientfd <= 0) {
		printf("socket failed\n");
		return -1;
	}

	struct sockaddr_in serveraddr = {0};
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(dyco_SERVER_PORT);
	serveraddr.sin_addr.s_addr = inet_addr(dyco_SERVER_IPADDR);

	int result = dyco_connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (result != 0) {
		printf("connect failed\n");
		return -2;
	}

	return clientfd;
	
}

void client(void *arg) {

	int clientfd = init_client();
	char *buffer = "dyco_client\r\n";
	int length;
	char buf[2048];

	while (1) {

		length = dyco_send(clientfd, buffer, strlen(buffer), 0);
		printf("echo send length : %d\n", length);
		if (length <= 0) {
			perror("dyco_send() wrong!\n");
			abort();
		}
		length = dyco_recv(clientfd, buf, 2048, 0);
		printf("echo recv length : %d\n", length);
		if (length <= 0) {
			perror("dyco_recv() wrong!\n");
			abort();
		}
		sleep(1);
	}

}



int main(int argc, char *argv[]) {
	dyco_coroutine *co = NULL;

	dyco_coroutine_create(&co, client, NULL);
	
	dyco_schedule_run(); //run

	return 0;
}







