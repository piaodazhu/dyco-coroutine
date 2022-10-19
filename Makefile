
CC = gcc
ECHO = echo

SUB_DIR = src/ example/
ROOT_DIR = $(shell pwd)
OBJS_DIR = $(ROOT_DIR)/objs
BIN_DIR = $(ROOT_DIR)/bin

BIN = socket_server_example socket_client_example ssl_server_example ssl_client_example epoll_example sleep_example setstack_example signal_example stop_abort_example channel_example waitgroup_example pubsub_example semaphore_example network_example multithread_example
FLAG = -lpthread -O3 -ldl -I $(ROOT_DIR)/src -I /usr/local/include/hiredis/
SSLFLAG = -lssl -lcrypto
RDSFLAG = -lhiredis

CUR_SOURCE = ${wildcard *.c}
CUR_OBJS = ${patsubst %.c, %.o, %(CUR_SOURCE)}

export CC BIN_DIR OBJS_DIR ROOT_IDR FLAG BIN ECHO EFLAG

all : $(SUB_DIR) $(BIN)
.PHONY : all


$(SUB_DIR) : ECHO
	make -C $@

ECHO :
	@echo $(SUB_DIR)


socket_server_example : $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/socket_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

socket_client_example : $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/socket_client.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

epoll_example : $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/epoll.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

sleep_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/sleep.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

setstack_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/setstack.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

signal_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/signal.o $(OBJS_DIR)/dyco_signal.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

stop_abort_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/stop_abort.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

channel_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/dyco_channel.o $(OBJS_DIR)/channel.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

waitgroup_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/dyco_waitgroup.o $(OBJS_DIR)/waitgroup.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

pubsub_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/dyco_pubsub.o $(OBJS_DIR)/pubsub.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

semaphore_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/dyco_semaphore.o $(OBJS_DIR)/semaphore.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

network_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/network.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) $(RDSFLAG)

ssl_server_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/dyco_ssl.o $(OBJS_DIR)/ssl_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) $(SSLFLAG)

ssl_client_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/dyco_ssl.o $(OBJS_DIR)/ssl_client.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) $(SSLFLAG)

multithread_example: $(OBJS_DIR)/dyco_hook.o $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_schedcall.o $(OBJS_DIR)/multithread.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

clean :
	rm -rf $(BIN_DIR)/* $(OBJS_DIR)/*

