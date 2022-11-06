#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "dyco/dyco_coroutine.h"

int OpenListener(int port)
{
	int sd;
	struct sockaddr_in addr;

	sd = socket(PF_INET, SOCK_STREAM, 0);
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		perror("can't bind port");
		DYCO_ABORT();
	}
	if (listen(sd, 10) != 0)
	{
		perror("Can't configure listening port");
		DYCO_ABORT();
	}
	return sd;
}

SSL_CTX *InitServerCTX(void)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;
	
	SSL_library_init();
	OpenSSL_add_all_algorithms(); /* load & register all cryptos, etc. */
	SSL_load_error_strings();     /* load all error messages */

	method = SSLv23_server_method();
	ctx = SSL_CTX_new(method); /* create new context from method */
	if (ctx == NULL)
	{
		ERR_print_errors_fp(stderr);
		DYCO_ABORT();
	}
	return ctx;
}

void LoadCertificates(SSL_CTX *ctx, char *CertFile, char *KeyFile)
{
	// New lines
	if (SSL_CTX_load_verify_locations(ctx, CertFile, KeyFile) != 1)
		ERR_print_errors_fp(stderr);

	if (SSL_CTX_set_default_verify_paths(ctx) != 1)
		ERR_print_errors_fp(stderr);
	// End new lines

	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		DYCO_ABORT();
	}
	/* set the private key from KeyFile (may be the same as CertFile) */
	SSL_CTX_set_default_passwd_cb_userdata(ctx, "12345678");
	if (SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		DYCO_ABORT();
	}
	/* verify private key */
	if (!SSL_CTX_check_private_key(ctx))
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		DYCO_ABORT();
	}

	// New lines - Force the client-side have a certificate
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify_depth(ctx, 4);
	// End new lines
}

void ShowCerts(SSL *ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
	if (cert != NULL)
	{
		printf("Server certificates:\n");
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		printf("Subject: %s\n", line);
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		printf("Issuer: %s\n", line);
		free(line);
		X509_free(cert);
	}
	else
		printf("No certificates.\n");
}

// void sslconnection(void *arg)
// {
// 	SSL *ssl = arg;
// 	char buf[1024];
// 	int ret, err, len;
// 	int fd = SSL_get_fd(ssl);

// 	SSL_set_accept_state(ssl);

// 	while(1)
// 	{
// 		ret = dyco_coroutine_waitRW(fd, 1000);
// 		assert(ret > 0);
// 		ret = SSL_accept(ssl);
// 		if(ret != 1)
// 		{
// 			err = SSL_get_error(ssl, ret);
// 			if ((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ))
// 			{
// 				continue;
// 			}
// 			else
// 			{
// 				printf("SSL_accept failed\n");
// 				SSL_free(ssl);
// 				ssl = NULL;
// 				return;
// 			}
// 		}
// 		else
// 		{
// 			printf("SSL_accept success\n");
// 			break;
// 		}
// 	}

// 	// ShowCerts(ssl);
// 	while (1) {
// 		// read
// 		while (1)
// 		{
// 			ret = dyco_coroutine_waitRead(fd, -1);
// 			assert(ret > 0);
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
// 				return;
// 			}
// 		}

// 		if (ret == 0) {
// 			printf("SSL closed\n");
// 			SSL_shutdown(ssl);
// 			close(SSL_get_fd(ssl));
// 			SSL_free(ssl);
// 			ssl = NULL;
// 			return;
// 		}

// 		printf("Receive from client: %.*s\n", len, buf);


// 		// echo write
// 		while (1)
// 		{
// 			ret = dyco_coroutine_waitWrite(fd, 1000);
// 			assert(ret > 0);
// 			ret = SSL_write(ssl, buf, len);
// 			int nRes = SSL_get_error(ssl, ret);
// 			if(nRes == SSL_ERROR_NONE)
// 			{
// 				len = ret;
// 				break;
// 			}
// 			else if (nRes == SSL_ERROR_WANT_WRITE)
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
// 				return;
// 			}
// 		}
// 		// next read loop
// 	}
	
// 	SSL_shutdown(ssl);
// 	close(SSL_get_fd(ssl));
// 	SSL_free(ssl);
// 	ssl = NULL;
// 	return;
// }

void sslconnection(void *arg)
{
	SSL *ssl = arg;
	char buf[1024];
	int ret, err, len;
	int fd = SSL_get_fd(ssl);

	if (dyco_SSL_accept(ssl) != 1) {
		close(fd);
		SSL_free(ssl);
		return;
	}

	// ShowCerts(ssl);
	while (1) {
		// read
		if ((len = dyco_SSL_read(ssl, buf, 1024)) <= 0) {
			SSL_shutdown(ssl);
			close(fd);
			SSL_free(ssl);
			return;
		}

		printf("Receive from client: %.*s\n", len, buf);

		// echo write
		if ((len = dyco_SSL_write(ssl, buf, 1024)) <= 0) {
			SSL_shutdown(ssl);
			close(fd);
			SSL_free(ssl);
			return;
		}
		// next read loop
	}
	
	SSL_shutdown(ssl);
	close(SSL_get_fd(ssl));
	SSL_free(ssl);
	ssl = NULL;
	return;
}

void sslserver(void *arg)
{
	SSL_CTX *ctx;
	int listenfd;

	char CertFile[] = "cert/certificate.crt";
	char KeyFile[] = "cert/private_key.pem";

	ctx = InitServerCTX();
	LoadCertificates(ctx, CertFile, KeyFile);
	listenfd = OpenListener(5000);
	while (1)
	{
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		SSL *ssl;

		int client = accept(listenfd, (struct sockaddr *)&addr, &len);
		printf("Connection: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, client);
		dyco_coroutine_create(sslconnection, ssl);
	}
	close(listenfd);
	SSL_CTX_free(ctx);
}

void foo(void*arg) {
	while (1) {
		dyco_coroutine_sleep(400);
		putchar('.');
		fflush(stdout);		
	}
	return;	
}

int main()
{
	dyco_coroutine_create(sslserver, NULL);
	dyco_coroutine_create(foo, NULL);

	printf("[+] scheduler start running.\n");
	dyco_schedule_run();
	printf("[-] scheduler stopped.\n");

	return 0;
}
