CC = gcc
ECHO = echo

PROJECTNAME = dyco
SUB_DIR = src/ example/
ROOT_DIR = $(shell pwd)
OBJS_DIR = $(ROOT_DIR)/objs
BIN_DIR = $(ROOT_DIR)/bin
LIB_DIR = $(ROOT_DIR)/lib
SRC_DIR = $(ROOT_DIR)/src
HDR_DIR = $(ROOT_DIR)/include

OBJS = $(patsubst %.c, %.o, $(notdir $(wildcard $(SRC_DIR)/*.c)))
OBJS += $(patsubst %.S, %.o, $(notdir $(wildcard $(SRC_DIR)/*.S)))
HDR_DIR_INSTALL_DIR = /usr/local/include
LIB = libdyco.so
LIB_INSTALL_DIR = /usr/local/lib

BIN = socket_server_example socket_client_example epoll_example sleep_example setstack_example signal_example stop_abort_example channel_example waitgroup_example pubsub_example semaphore_example multithread_example coropool_example asymmetric_example stack_dont_overflow_example

FLAG =  -O3 -lpthread -ldl -I $(HDR_DIR)
SSLFLAG = -lssl -lcrypto -D DYCO_SSL_OK

HASSSL := $(shell if [ -d /usr/local/include/openssl ] || [ -d /usr/include/openssl ]; then echo 1; fi)
HASRDS := $(shell if [ -d /usr/local/include/hiredis ] || [ -d /usr/include/hiredis ]; then echo 1; fi)

ifdef HASRDS
	BIN += network_example
endif

ifdef HASSSL
	BIN += ssl_server_example ssl_client_example
	FLAG += $(SSLFLAG)
endif

export CC BIN_DIR OBJS_DIR ROOT_DIR FLAG BIN ECHO LIB_DIR

all : PREPARE $(SUB_DIR) $(BIN) $(LIB)
.PHONY : all clean

PREPARE:
	mkdir -p $(OBJS_DIR) $(BIN_DIR) $(LIB_DIR)

$(SUB_DIR) : ECHO
	make -C $@

ECHO :
	@echo $(SUB_DIR)


socket_server_example : $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/socket_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

socket_client_example : $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/socket_client.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

epoll_example : $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/epoll.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

sleep_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/sleep.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

setstack_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/setstack.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

signal_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/signal.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) 

stop_abort_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/stop_abort.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

channel_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/channel.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

waitgroup_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/waitgroup.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

pubsub_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/pubsub.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

semaphore_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/semaphore.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

network_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/network.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG) -lhiredis

ssl_server_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/ssl_server.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

ssl_client_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/ssl_client.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

multithread_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/multithread.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

coropool_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/coropool.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

asymmetric_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/asymmetric.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

stack_dont_overflow_example: $(addprefix $(OBJS_DIR)/, $(OBJS)) $(OBJS_DIR)/stack_dont_overflow.o
	$(CC) -o $(BIN_DIR)/$@ $^ $(FLAG)

$(LIB): $(addprefix $(LIB_DIR)/, $(OBJS))
	$(CC) -shared -o $(LIB_DIR)/$@ $^ $(FLAG)
	
install: $(LIB)
	cp $(LIB_DIR)/$(LIB) $(LIB_INSTALL_DIR)
	cp -r $(HDR_DIR)/$(PROJECTNAME) $(HDR_DIR_INSTALL_DIR)
	ldconfig
	rm -rf *.o *.so*
	echo "Installing done."

uninstall:
	rm -rf $(LIB_INSTALL_DIR)/$(LIB)
	rm -rf $(HDR_DIR_INSTALL_DIR)/$(PROJECTNAME)
	ldconfig
	echo "Uninstalling done."

clean:
	rm -rf $(BIN_DIR)/* $(OBJS_DIR)/* $(LIB_DIR)/*

