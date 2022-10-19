#include "dyco_coroutine.h"

void corofun(void *arg)
{
	int sleeptime = *(int*)arg;
	int counter = 0;
	int randadder = 0;
	int iter = 10;
	int i;
	for (i = 0; i < iter; i++) {
		randadder = rand() % 100;
		printf("I am coro %d [ownstack? %d]. I will calculate %d + %d\n", dyco_coroutine_coroID(), dyco_coroutine_getStack(dyco_coroutine_coroID(), NULL, NULL), counter, randadder);
		dyco_coroutine_sleep(sleeptime);
		counter += randadder;
		printf("I am coro %d [ownstack? %d]. My answer is %d\n", dyco_coroutine_coroID(), dyco_coroutine_getStack(dyco_coroutine_coroID(), NULL, NULL), counter);
	}
	return;
}

int main()
{
	int i, cid, ret;
	int sleeptime = 1000;
	char stack[3][4096];
	// char* stack[3];
	// for (i = 0; i < 3; i++) {
	// 	stack[i] = malloc(4096);
	// }

	for (i = 0; i < 3; i++) {
		dyco_coroutine_create(corofun, &sleeptime);
	}
	for (i = 0; i < 3; i++) {
		cid = dyco_coroutine_create(corofun, &sleeptime);
		ret = dyco_coroutine_setStack(cid, stack[i], 4096);
		assert(ret == 1);
	}
	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	// for (i = 0; i < 3; i++) {
	// 	free(stack[i]);
	// }
	return 0;
}
