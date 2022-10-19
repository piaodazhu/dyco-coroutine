#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "dyco_coroutine.h"

// Added the LoadCertificates how in the server-side makes.
void LoadCertificates(SSL_CTX *ctx, char *CertFile, char *KeyFile, char *password)
{
	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		ABORT();
	}

	/* set the private key from KeyFile (may be the same as CertFile) */
	SSL_CTX_set_default_passwd_cb_userdata(ctx, password);
	if (SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		ABORT();
	}

	/* verify private key */
	if (!SSL_CTX_check_private_key(ctx))
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		ABORT();
	}
}

int OpenConnection(const char *hostname, int port)
{
	int fd;
	struct hostent *host;
	struct sockaddr_in addr;

	fd = socket(PF_INET, SOCK_STREAM, 0);
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(hostname);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		close(fd);
		perror(hostname);
		ABORT();
	}
	return fd;
}

SSL_CTX *InitCTX(void)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	SSL_library_init();
	OpenSSL_add_all_algorithms(); /* Load cryptos, et.al. */
	SSL_load_error_strings();     /* Bring in and register error messages */

	method = SSLv23_client_method();
	ctx = SSL_CTX_new(method); /* Create new context */
	if (ctx == NULL)
	{
		ERR_print_errors_fp(stderr);
		ABORT();
	}
	return ctx;
}

void ShowCerts(SSL *ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
	if (cert != NULL)
	{
		printf("Server certificates:\n");
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		printf("Subject: %s\n", line);
		free(line); /* free the malloc'ed string */
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		printf("Issuer: %s\n", line);
		free(line);	 /* free the malloc'ed string */
		X509_free(cert); /* free the malloc'ed certificate copy */
	}
	else
		printf("No certificates.\n");
}

// void sslclient(void *arg)
// {
// 	SSL_CTX *ctx;
// 	SSL *ssl;
// 	int serverfd;
// 	int len, ret, err;
// 	char buf[1024];
// 	char *msg = "Hello. I'm SSL Client.";

// 	char CertFile[] = "key/certificate.crt";
// 	char KeyFile[] = "key/private_key.pem";

// 	ctx = InitCTX();
// 	LoadCertificates(ctx, CertFile, KeyFile, "12345678");
// 	serverfd = OpenConnection("127.0.0.1", 5000);
	
// 	ssl = SSL_new(ctx);
// 	SSL_set_fd(ssl, serverfd); 

// 	while(1)
// 	{
// 		ret = SSL_connect(ssl);
// 		if(ret == -1)
// 		{
// 			err = SSL_get_error(ssl, ret);
// 			if ((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ))
// 			{
// 				continue;
// 			}
// 			else
// 			{
// 				SSL_free(ssl);
// 				ssl = NULL;
// 				close(serverfd);
// 				return;
// 			}
// 		}
// 		else
// 		{
// 			printf("SSL_connect success\n");
// 			break;
// 		}
// 	}

	

// 	printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
// 	// ShowCerts(ssl);
// 	while (1) {
// 		// write message	
// 		while (1) {
// 			ret = SSL_write(ssl, msg, strlen(msg));
// 			err = SSL_get_error(ssl, ret);
// 			if(err == SSL_ERROR_NONE)
// 			{
// 				len = ret;
// 				break;
// 			}
// 			else if (err == SSL_ERROR_WANT_WRITE)
// 			{
// 				continue;
// 			}
// 			else
// 			{
// 				printf("SSL_write failed\n");
// 				SSL_shutdown(ssl);
// 				close(SSL_get_fd(ssl));
// 				SSL_free(ssl);
// 				ssl = NULL;
// 				SSL_CTX_free(ctx);
// 				return;
// 			}
// 		}
		
// 		if (ret == 0) {
// 			printf("SSL closed\n");
// 			SSL_shutdown(ssl);
// 			close(SSL_get_fd(ssl));
// 			SSL_free(ssl);
// 			ssl = NULL;
// 			SSL_CTX_free(ctx);
// 			return;
// 		}

// 		while (1)
// 		{
// 			ret = SSL_read(ssl, buf, 1024);
// 			err = SSL_get_error(ssl, ret);
// 			if(err == SSL_ERROR_NONE)
// 			{
// 				len = ret;
// 				break;
// 			}
// 			else if (err == SSL_ERROR_WANT_READ)
// 			{
// 				continue;
// 			}
// 			else
// 			{
// 				printf("SSL_read failed\n");
// 				SSL_shutdown(ssl);
// 				close(SSL_get_fd(ssl));
// 				SSL_free(ssl);
// 				ssl = NULL;
// 				SSL_CTX_free(ctx);
// 				return;
// 			}
// 		}

// 		printf("Receive from server: %.*s\n", len, buf);
// 		dyco_coroutine_sleep(1000);
// 	}

// 	SSL_shutdown(ssl);
// 	close(SSL_get_fd(ssl));
// 	SSL_free(ssl);
// 	ssl = NULL;
// 	SSL_CTX_free(ctx);
// 	return;
// }

void sslclient(void *arg)
{
	SSL_CTX *ctx;
	SSL *ssl;
	int serverfd;
	int len, ret, err;
	char buf[1024];
	char *msg = "Hello. I'm SSL Client.";

	char CertFile[] = "cert/certificate.crt";
	char KeyFile[] = "cert/private_key.pem";

	ctx = InitCTX();
	LoadCertificates(ctx, CertFile, KeyFile, "12345678");
	serverfd = OpenConnection("127.0.0.1", 5000);
	
	ssl = SSL_new(ctx);
	SSL_set_fd(ssl, serverfd); 

	ret = dyco_SSL_connect(ssl);
	printf("ret = %d\n", ret);
	if (ret != 1) {
		close(serverfd);
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return;
	}

	printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
	// ShowCerts(ssl);
	while (1) {
		// write message
		if ((len = dyco_SSL_write(ssl, msg, strlen(msg))) <= 0) {
			SSL_shutdown(ssl);
			close(serverfd);
			SSL_free(ssl);
			SSL_CTX_free(ctx);
			return;
		}	
		
		// read response
		if ((len = dyco_SSL_read(ssl, buf, 1024)) <= 0) {
			SSL_shutdown(ssl);
			close(serverfd);
			SSL_free(ssl);
			SSL_CTX_free(ctx);
			return;
		}

		printf("Receive from client: %.*s\n", len, buf);
		dyco_coroutine_sleep(1000);
	}

	SSL_shutdown(ssl);
	close(SSL_get_fd(ssl));
	SSL_free(ssl);
	ssl = NULL;
	SSL_CTX_free(ctx);
	return;
}

void foo(void*arg) {
	while (1) {
		dyco_coroutine_sleep(400);
		putchar('.');
	}
	return;	
}

int main()
{
	dyco_coroutine_create(sslclient, NULL);
	dyco_coroutine_create(foo, NULL);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	return 0;
}