CXX=g++
CFLAGS=-g -O2 -Wall -fPIC -std=c++11 -Wno-deprecated

INC=-I../../include
LIB=-L../../lib -lereactor /usr/local/lib/libprotobuf.a -lrt -pthread
OBJS = $(addsuffix .o, $(basename $(wildcard *.cc)))

all:
	$(CXX) -o server.prog $(CFLAGS) server.cc echoMsg.pb.cc $(INC) $(LIB)
	$(CXX) -o client.prog $(CFLAGS) client.cc echoMsg.pb.cc $(INC) $(LIB)

clean:
	-rm -f *.o *.d server client
