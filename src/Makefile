CUR_SOURCE = ${wildcard *.c}

CUR_OBJS = ${patsubst %.c, %.o, $(CUR_SOURCE)}

CUR_ASM_SOURCE = ${wildcard *.S}

CUR_ASM_OBJS = ${patsubst %.S, %.o, $(CUR_ASM_SOURCE)}

all : $(CUR_OBJS) $(CUR_ASM_OBJS)

$(CUR_OBJS) : %.o : %.c
	$(CC) -c $^ -o $(OBJS_DIR)/$@ $(FLAG)
	$(CC) -c $^ -o $(LIB_DIR)/$@ -fPIC $(FLAG)

$(CUR_ASM_OBJS) : %.o : %.S
	$(CC) -c $^ -o $(OBJS_DIR)/$@ $(FLAG)
	$(CC) -c $^ -o $(LIB_DIR)/$@ -fPIC $(FLAG)

ECHO : 
	@echo $(SUB_DIR)
