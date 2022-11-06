#include "dyco/dyco_coroutine.h"


void subscriber(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_pubsubchannel *psc = udata;

	int *times = arg;

	printf("[subscriber] I am coro %d. I'm about subscribe %d messages\n", dyco_coroutine_coroID(), *times);
	
	int ret, i;
	char buf[64];
	for (i = 0; i < *times; i++) {
		ret = dyco_pubsub_subscribe(psc, buf, 64, 3000);
		if (ret > 0) {
			printf("[subscriber] I am coro %d. I receive {%.*s}\n", dyco_coroutine_coroID(), ret, buf);
		} else {
			printf("[subscriber] I am coro %d. ret = %d\n", dyco_coroutine_coroID(), ret);
		}
	}

	return;
}

void publisher(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_pubsubchannel *psc = udata;

	printf("[publisher] I am coro %d. I'm about sleep 1 s\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(1000);

	printf("[publisher] I am coro %d. I'm about wait publish 5 messages\n", dyco_coroutine_coroID());

	int ret, i, len;
	char msg[64];
	for (i = 0; i < 5; i++) {
		len = sprintf(msg, "message %d", i);
		ret = dyco_pubsub_publish(psc, msg, len);
		printf("[publisher] I am coro %d. message %d is sent\n", dyco_coroutine_coroID(), i);
	}
	printf("[publisher] I am coro %d. I'm about sleep 4 s\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(4000);
	return;
}
int main()
{
	int times[3] = {2, 5, 6};
	dyco_coroutine_create(publisher, NULL);
	dyco_coroutine_create(subscriber, times);
	dyco_coroutine_create(subscriber, times + 1);
	dyco_coroutine_create(subscriber, times + 2);

	dyco_pubsubchannel *psc = dyco_pubsub_create(64);
	dyco_schedule_setUdata(psc);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	dyco_pubsub_destroy(&psc);
	return 0;
}