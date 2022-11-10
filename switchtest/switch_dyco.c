#include <sys/time.h>
#include "dyco/dyco_coroutine.h"

#define TOTAL_SWITCH_COUNTER	10000000
// #define SHARED_STACK
static int count;
struct timeval tv1, tv2;

void corofun(void *arg)
{
	// printf("hello\n");
	while (1) {
		if (count == 0) {
			gettimeofday(&tv1, NULL);
		}
		
		// printf("ok\n");
		++count;
		dyco_coroutine_sleep(0);
		++count;

		if (count > TOTAL_SWITCH_COUNTER) {
			gettimeofday(&tv2, NULL);
			time_t diff = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
			double speed = 1.0 * diff / TOTAL_SWITCH_COUNTER;
			printf("diff = %ld us, switch speed = %f M/s\n", diff, speed);
			dyco_schedcall_abort();
		}
	}
	return;
}

int main()
{
	count = 0;
	int i = 0;
	while (i++ < 2) {
		int cid = dyco_coroutine_create(corofun, NULL);
		// printf("cid = %d\n", cid);
#ifndef SHARED_STACK
		dyco_coroutine_setStack(cid, NULL, DYCO_DEFAULT_STACKSIZE);
#endif		
	}

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");
	return 0;
}