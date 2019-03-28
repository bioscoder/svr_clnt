CC = gcc

CUSTOM_LIBS= z lzma
CUSTOM_INCLUDED_LIBS  = $(addprefix -l,$(CUSTOM_LIBS))

BUILD_DIR = build

S_DIR = server/
S_SRC = $(wildcard $(S_DIR)*.c)
S_OBJ = $(S_SRC:%.c=%.o)
S_CFLAGS += -std=c++11 -g3 -w -O2

C_DIR = client/
C_SRC = $(wildcard $(C_DIR)*.c)
C_OBJ = $(C_SRC:%.c=%.o)
C_CFLAGS = -std=c++11 -pthread -g3 -w -O2

P_DIR = protocol/
P_SRC = $(wildcard $(P_DIR)*.c)
P_OBJ = $(P_SRC:%.c=%.o)
P_CFLAGS = -std=c++11 -g3 -w -O2

S_OUT = hserver
C_OUT = hclient

all: clean server client

server: $(P_OBJ) $(S_OBJ)
	$(CC) $(S_CFLAGS) -o $(S_OUT) $(S_OBJ) $(CUSTOM_INCLUDED_LIBS) $(P_OBJ)

client: $(P_OBJ) $(C_OBJ)
	$(CC) $(C_CFLAGS) -o $(C_OUT) $(C_OBJ) $(P_OBJ)
	
clean:
	$(RM) $(S_OBJ) $(C_OBJ) $(P_OBJ) $(C_OUT) $(S_OUT)

