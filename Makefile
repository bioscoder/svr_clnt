CC = g++

INCLUDES += -I./zlib
LDFLAGS += -L./zlib
LDLIBS  += -lz

S_CFLAGS += -std=c++11 -pthread -g -w -O2
C_CFLAGS += -std=c++11 -pthread -g -w -O2

S_SRCS = srv.c
C_SRCS = clnt.c

S_OBJS = $(S_SRCS:.c=.o)
C_OBJS = $(C_SRCS:.c=.o)

S_OUT = hserver
C_OUT = hclient

default: clean $(S_OUT) $(C_OUT)

$(S_OUT): $(S_OBJS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(S_CFLAGS) -o $(S_OUT) $(S_OBJS) $(LDLIBS)

$(C_OUT): $(C_OBJS)
	$(CC) $(INCLUDES) $(LDFLAGS) $(C_CFLAGS) -o $(C_OUT) $(C_OBJS) $(LDLIBS)

#$(S_OUT): $(S_OBJS)
#	$(CC) $(S_CFLAGS) -o $(S_OUT) $(S_OBJS)

#$(C_OUT): $(C_OBJS)
#	$(CC) $(C_CFLAGS) -o $(C_OUT) $(C_OBJS)
	
clean:
	$(RM) *.o
