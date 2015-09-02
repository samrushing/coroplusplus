# -*- Mode: Makefile -*-

CC = c++
OPT = -O2 -g -Wall
CFLAGS = $(OPT) -I..
LIBS = -lstdc++ -lm

HEADERS = \
	../coro++.h \
	../coro_file.h \
	../coro_socket.h \
	../coro_buffer.h \
	../coro_bench.h \
	../sync.h \
	../event_poller.h \
	../event_kqueue.h \
	../event_select.h \
	../event_poll.h \
	../coro_openssl.h

OPENSSL_INCLUDE = /usr/local/ssl/include
OPENSSL_LINK = -L /usr/local/ssl/lib -lssl -lcrypto

S2N_INCLUDE = /usr/local/include
S2N_LINK = -L/usr/local/lib -ls2n

all: echo_server smtp_server echo_client # http_server spdy3_proxy

echo_server.o: echo_server.cc $(HEADERS)

echo_server: echo_server.o
	$(CC) $(OPT) -o echo_server echo_server.o $(LIBS)

smtp_server.o: smtp_server.cc $(HEADERS)

smtp_server: smtp_server.o
	$(CC) $(OPT) -o smtp_server smtp_server.o $(LIBS)

echo_client: echo_client.o
	$(CC) $(OPT) -o echo_client echo_client.o $(LIBS)

echo_server_s2n.o: echo_server.cc
	$(CC) -DUSE_S2N $(CFLAGS) -I $(S2N_INCLUDE) -c echo_server.cc -o echo_server_s2n.o

echo_server_s2n: echo_server_s2n.o
	$(CC) $(CFLAGS) -I $(LIBS) $< -o $@ $(S2N_LINK)

echo_server_openssl.o: echo_server.cc
	$(CC) -DUSE_OPENSSL $(CFLAGS) -I $(OPENSSL_INCLUDE) -c $< -o echo_server_openssl.o

echo_server_openssl: echo_server_openssl.o
	$(CC) $(CFLAGS) $< -o $@ $(OPENSSL_LINK) $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) -c $<

clean:
	python ../util/clean.py
