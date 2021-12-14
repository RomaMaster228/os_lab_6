CC = gcc

CC_FLAGS = -pedantic -Wall
LD_FLAGS = -lzmq -L.

CC_LIB_FLAGS = -c -pedantic -Wall -fPIC
LD_LIB_FLAGS = -shared -lzmq

EXECUTABLE1 = server
EXECUTABLE2 = client

SRC_EXE1 = server.c
SRC_EXE2 = client.c
OBJ_EXE1 = $(SRC_EXE1:.c=.o)
OBJ_EXE2 = $(SRC_EXE2:.c=.o)

LIB    = -ltools
SRC_LIB = zmq_comm.c
OBJ_LIB = $(SRC_LIB:.c=.o)
SO_LIB  = libtools.so


all: $(EXECUTABLE1) $(EXECUTABLE2)

$(EXECUTABLE1) : $(SRC_EXE1) $(SO_LIB)
        $(CC) $(CC_FLAGS) $(SRC_EXE1) -o $(EXECUTABLE1) $(LD_FLAGS) $(LIB)

$(EXECUTABLE2) : $(SRC_EXE2) $(SO_LIB)
        $(CC) $(CC_FLAGS) $(SRC_EXE2) -o $(EXECUTABLE2) $(LD_FLAGS) $(LIB)

$(SO_LIB) : $(OBJ_LIB)
        $(CC) $^ -o $@ $(LD_LIB_FLAGS)

$(OBJ_LIB) : $(SRC_LIB)
        $(CC) $(CC_LIB_FLAGS) $^

clean:
        rm -f *.o *.so $(EXECUTABLE1) $(EXECUTABLE2)