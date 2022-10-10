
CC = gcc
ECHO = echo

SUB_DIR = src/ sample/
ROOT_DIR = $(shell pwd)
OBJS_DIR = $(ROOT_DIR)/objs
BIN_DIR = $(ROOT_DIR)/bin

BIN = dyco_server dyco_client dyco_bench dyco_server_mulcore dyco_http_server dyco_mysql_client dyco_mysql_oper dyco_websocket_server dyco_http_server_mulcore dyco_httpd dyco_rediscli
FLAG = -lpthread -O3 -lcrypto -lssl -lmysqlclient -lhiredis -ldl -I $(ROOT_DIR)/src  -I /usr/include/mysql/ -I /usr/local/include/hiredis/

CUR_SOURCE = ${wildcard *.c}
CUR_OBJS = ${patsubst %.c, %.o, %(CUR_SOURCE)}

export CC BIN_DIR OBJS_DIR ROOT_IDR FLAG BIN ECHO EFLAG

all : $(SUB_DIR) $(BIN)
.PHONY : all


$(SUB_DIR) : ECHO
	make -C $@

#DEBUG : ECHO
#	make -C bin

ECHO :
	@echo $(SUB_DIR)


dyco_server : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_client : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_client.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_bench : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_bench.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_server_mulcore : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_server_mulcore.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_http_server : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_http_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_websocket_server : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_websocket_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_mysql_client : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_mysql_client.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

dyco_mysql_oper : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_mysql_oper.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

dyco_rediscli : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_rediscli.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

dyco_http_server_mulcore : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_http_server_mulcore.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

dyco_httpd : $(OBJS_DIR)/dyco_socket.o $(OBJS_DIR)/dyco_coroutine.o $(OBJS_DIR)/dyco_epoll.o $(OBJS_DIR)/dyco_schedule.o $(OBJS_DIR)/dyco_httpd.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

clean :
	rm -rf $(BIN_DIR)/* $(OBJS_DIR)/*

