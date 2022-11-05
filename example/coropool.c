#include "dyco_coroutine.h"
dyco_coropool *pool1;
dyco_coropool *pool2;

void corofunc1(void *arg)
{
	printf("I am coro %d. Hello\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(1000);
	printf("I am coro %d. Bye.\n", dyco_coroutine_coroID());
	return;
}

void corofunc2(void *arg)
{
	int ret;
	printf("I am coro %d. Hello\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(1000);

	ret = dyco_coropool_obtain(pool2, corofunc1, NULL, 3000);
	printf("6: ret = %d\n", ret);

	// will wait a coroutine finishes
	ret = dyco_coropool_obtain(pool2, corofunc1, NULL, 3000);
	printf("7: ret = %d\n", ret);

	dyco_coroutine_sleep(2000);
	dyco_coropool_resize(pool2, 1);
	// will timeout and return 0
	ret = dyco_coropool_obtain(pool2, corofunc1, NULL, 3000);
	printf("8: ret = %d\n", ret);

	printf("I am coro %d. Bye.\n", dyco_coroutine_coroID());
	return;
}

int main()
{
	// independent stack coroutines pool
	pool1 = dyco_coropool_create(2, 4000);

	// shared stack coroutines pool
	pool2 = dyco_coropool_create(2, 0);

	int ret;
	ret = dyco_coropool_obtain(pool1, corofunc1, NULL, 3000);
	printf("1: ret = %d\n", ret);
	ret = dyco_coropool_obtain(pool1, corofunc1, NULL, 3000);
	printf("2: ret = %d\n", ret);

	// will immediately return -1
	ret = dyco_coropool_obtain(pool1, corofunc1, NULL, 3000);
	printf("3: ret = %d\n", ret);

	dyco_coropool_resize(pool1, 4);
	ret = dyco_coropool_obtain(pool1, corofunc1, NULL, 3000);
	printf("4: ret = %d\n", ret);

	ret = dyco_coropool_obtain(pool2, corofunc2, NULL, 3000);
	printf("5: ret = %d\n", ret);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	ret = dyco_coropool_destroy(&pool1);
	assert(ret == 0);
	ret = dyco_coropool_destroy(&pool2);
	assert(ret == 0);
	return 0;
}