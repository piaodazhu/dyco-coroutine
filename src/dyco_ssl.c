#ifndef DYCO_SSL_H
#define DYCO_SSL_H
#include "dyco_coroutine.h"

#ifdef DYCO_SSL_OK
#ifdef DYCO_SSL_ENABLE

#define SSL_TIMEOUT_DEFAULT	1000

int
dyco_SSL_accept(SSL *ssl)
{
	int ret, err;
	int fd = SSL_get_fd(ssl);
	SSL_set_accept_state(ssl);

label:
	if (dyco_coroutine_waitRW(fd, SSL_TIMEOUT_DEFAULT) <= 0) {
		return -1;
	}
	ret = SSL_accept(ssl);
	if(ret != 1)
	{
		err = SSL_get_error(ssl, ret);
		if ((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ))
			goto label;
		else 
			return ret;
	}
	return 1;
}


int
dyco_SSL_connect(SSL *ssl)
{
	int ret, err;
	int fd = SSL_get_fd(ssl);
	SSL_set_connect_state(ssl);

label:
	if (dyco_coroutine_waitRW(fd, SSL_TIMEOUT_DEFAULT) <= 0) {
		return -1;
	}
	ret = SSL_connect(ssl);
	if(ret == -1)
	{
		err = SSL_get_error(ssl, ret);
		if ((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ))
			goto label;
		else 
			return -1;
	}
	printf("SSL_connect success\n");
	return 1;
}


int
dyco_SSL_read(SSL *ssl, void *buf, int num)
{
	int ret, err;
	int fd = SSL_get_fd(ssl);

label:
	if (dyco_coroutine_waitRead(fd, -1) <= 0) {
		return -1;
	}
	ret = SSL_read(ssl, buf, num);
	err = SSL_get_error(ssl, ret);
	if(err == SSL_ERROR_NONE)
	{
		return num;
	}
	else if (err == SSL_ERROR_WANT_READ)
	{
		goto label;
	}
	
	return ret;
}


int
dyco_SSL_write(SSL *ssl, const void *buf, int num)
{
	int ret, err;
	int fd = SSL_get_fd(ssl);

label:
	if (dyco_coroutine_waitWrite(fd, SSL_TIMEOUT_DEFAULT) <= 0) {
		return -1;
	}
	ret = SSL_write(ssl, buf, num);
	err = SSL_get_error(ssl, ret);
	if(err == SSL_ERROR_NONE)
	{
		return num;
	}
	else if (err == SSL_ERROR_WANT_WRITE)
	{
		goto label;
	}
	
	return ret;
}

#endif
#endif

#endif