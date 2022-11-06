#include "dyco/dyco_coroutine.h"

void alice(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_channel *c = udata;

	printf("[alice] I am coro %d. I'm about read channel\n", dyco_coroutine_coroID());

	ssize_t ret;
	char buf[64];
	printf("[alice] I am coro %d. I want to recv msg1\n", dyco_coroutine_coroID());
	ret = dyco_channel_recv(c, buf, 64, -1);
	printf("[alice] I am coro %d. recv {%.*s}, ret = %ld\n", dyco_coroutine_coroID(), (int)ret, buf, ret);

	printf("[alice] I am coro %d. I'm about sleep 2s\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(2000);

	printf("[alice] I am coro %d. I want to recv msg2\n", dyco_coroutine_coroID());
	ret = dyco_channel_recv(c, buf, 64, -1);
	printf("[alice] I am coro %d. recv {%.*s}, ret = %ld\n", dyco_coroutine_coroID(), (int)ret, buf, ret);

	printf("[alice] I am coro %d. I want to recv msg3\n", dyco_coroutine_coroID());
	ret = dyco_channel_recv(c, buf, 64, -1);
	printf("[alice] I am coro %d. recv {%.*s}, ret = %ld\n", dyco_coroutine_coroID(), (int)ret, buf, ret);

	char *msg1 = "is any one there?";
	char *msg2 = "xxxxyyyyzzzz";
	printf("[alice] I am coro %d. I want to send msg1\n", dyco_coroutine_coroID());
	ret = dyco_channel_send(c, msg1, strlen(msg1), 3000);
	printf("[alice] I am coro %d. send {%s}, ret = %ld\n", dyco_coroutine_coroID(), msg1, ret);

	printf("[alice] I am coro %d. I want to send msg2\n", dyco_coroutine_coroID());
	ret = dyco_channel_send(c, msg2, strlen(msg2), 3000);
	printf("[alice] I am coro %d. send {%s}, ret = %ld\n", dyco_coroutine_coroID(), msg2, ret);

	return;
}

void bob(void *arg)
{
	void *udata;
	dyco_schedule_getUdata(&udata);
	dyco_channel *c = udata;

	printf("[bob] I am coro %d. I'm about sleep 3 s\n", dyco_coroutine_coroID());
	dyco_coroutine_sleep(3000);
	printf("[bob] I am coro %d. I wake up. Now I write the channel 3 times\n", dyco_coroutine_coroID());

	char *msg1 = "hello";
	char *msg2 = "world";
	char *msg3 = "!";
	ssize_t ret;

	printf("[bob] I am coro %d. I want to send msg1\n", dyco_coroutine_coroID());
	ret = dyco_channel_send(c, msg1, strlen(msg1), -1);
	printf("[bob] I am coro %d. send {%s}, ret = %ld\n", dyco_coroutine_coroID(), msg1, ret);

	printf("[bob] I am coro %d. I want to send msg2\n", dyco_coroutine_coroID());
	dyco_channel_send(c, msg2, strlen(msg2), -1);
	printf("[bob] I am coro %d. send {%s}, ret = %ld\n", dyco_coroutine_coroID(), msg2, ret);

	printf("[bob] I am coro %d. I want to send msg3\n", dyco_coroutine_coroID());
	dyco_channel_send(c, msg3, strlen(msg3), -1);
	printf("[bob] I am coro %d. send {%s}, ret = %ld. EXIT...\n", dyco_coroutine_coroID(), msg3, ret);

	return;
}


int main()
{
	dyco_coroutine_create(alice, NULL);
	dyco_coroutine_create(bob, NULL);
	
	dyco_channel *c = dyco_channel_create(64);
	dyco_schedule_setUdata(c);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	dyco_channel_destroy(&c);
	return 0;
}