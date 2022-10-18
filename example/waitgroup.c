#include "dyco_coroutine.h"

void worker(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_waitgroup *wg = udata;

	int *t = arg;
	
	printf("[worker] I am coro %d. I'm about work %d ms\n", dyco_coroutine_coroID(), *t);
	dyco_coroutine_sleep(*t);

	printf("[worker] I am coro %d. I'm done\n", dyco_coroutine_coroID());
	dyco_waitgroup_done(wg);
	
	return;
}

void watcher1(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_waitgroup *wg = udata;

	printf("[watcher1] I am coro %d. I'm about wait just 5 workers\n", dyco_coroutine_coroID());
	int ret = dyco_waitgroup_wait(wg, 5, -1);
	printf("[watcher1] I am coro %d. I have finish waiting and ret = %d\n", dyco_coroutine_coroID(), ret);
	return;
}

void watcher2(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_waitgroup *wg = udata;

	printf("[watcher2] I am coro %d. I'm about wait just 5 workers\n", dyco_coroutine_coroID());
	int ret = dyco_waitgroup_wait(wg, 5, -1);
	printf("[watcher2] I am coro %d. I have finish waiting and ret = %d\n", dyco_coroutine_coroID(), ret);
	return;
}

void watcher3(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_waitgroup *wg = udata;

	dyco_coroutine_sleep(6000);
	printf("[watcher3] I am coro %d. I'm about wait just 5 workers\n", dyco_coroutine_coroID());
	int ret = dyco_waitgroup_wait(wg, 5, -1);
	printf("[watcher3] I am coro %d. I have finish waiting and ret = %d\n", dyco_coroutine_coroID(), ret);
	return;
}

void boss(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_waitgroup *wg = udata;
	
	printf("[boss] I am coro %d. I'm about create 8 workers\n", dyco_coroutine_coroID());

	int i, cid, totol = 8;
	int *sleeptime = (int*)malloc(8 * sizeof(int));
	for (i = 0; i < totol; i++) {
		sleeptime[i] = (i + 1) * 1000;
		cid = dyco_coroutine_create(worker, sleeptime + i);
		assert(dyco_waitgroup_add(wg, cid) == 1);
	}
	
	printf("[boss] I am coro %d. I'm about wait all workers\n", dyco_coroutine_coroID());
	int ret = dyco_waitgroup_wait(wg, -1, -1);
	printf("[boss] I am coro %d. I have finish waiting and ret = %d\n", dyco_coroutine_coroID(), ret);
	
	// printf("[boss] I am coro %d. I'm about wait all workers\n", dyco_coroutine_coroID());
	// int ret = dyco_waitgroup_wait(wg, -1, 6000);
	// printf("[boss] I am coro %d. I have finish waiting and ret = %d\n", dyco_coroutine_coroID(), ret);

	// printf("[boss] I am coro %d. I'm about wait just 6 workers\n", dyco_coroutine_coroID());
	// int ret = dyco_waitgroup_wait(wg, 6, -1);
	// printf("[boss] I am coro %d. I have finish waiting and ret = %d\n", dyco_coroutine_coroID(), ret);
	return;
}

int main()
{
	int i;
	dyco_coroutine_create(boss, NULL);
	dyco_coroutine_create(watcher1, NULL);
	dyco_coroutine_create(watcher2, NULL);
	dyco_coroutine_create(watcher3, NULL);
	
	dyco_waitgroup *wg = dyco_waitgroup_create(10);
	dyco_schedule_setUdata(wg);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	dyco_waitgroup_destroy(&wg);
	return 0;
}