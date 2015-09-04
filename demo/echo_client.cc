// -*- Mode: C++ -*-

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "coro++.h"
#include "coro_file.h"
#include "coro_bench.h"
#include "coro_buffer.h"
#include "coro_socket.h"
#ifndef __linux__
#include "event_kqueue.h"
#endif
#include "event_select.h"
#include "event_poll.h"

#include <string>
#include <iostream>

#include <stdlib.h>

static int packet_size = 0;
static int counter     = 0;
static int ntrans      = 0;
static int total_bytes = 0;
static char * message;
static char server_bench[1024];

void
junkify_buffer (char * buffer, size_t length)
{
  const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  unsigned int i, l;

#define END_STRING "\r\n"
  l = strlen (END_STRING);

  for (i=0; i < length - l; i++) {
    buffer[i] = chars [i % strlen(chars)];
  }

  /* We don't want copy the zero character, so we can't use
   * strncpy */
  for (i=0; i < l; i++) {
    buffer[length-l+i] = END_STRING[i];
  }
}

void
client (struct sockaddr_in * addr)
{
  int i;
  coro_socket * s = new coro_socket;
  int buffer_size = std::max (packet_size, 4096);
  char * buffer = new char[buffer_size];
  int last=0;

  try {
    s->connect ((struct sockaddr *) addr, sizeof (struct sockaddr_in));

    for (i=0; i < ntrans; i++) {
      int count = packet_size;
      s->write (message, packet_size);
      while (count) {
        int n = s->read (buffer, count);
        if (!n) {
          throw "client closed unexpectedly";
        } else {
          count -= n;
        }
      }
      total_bytes += packet_size;
    }

    // we must change and check counter ONCE ONLY
    counter--;
    if (counter) {
      last = 0;
    } else {
      last = 1;
    }

    if (last) {
      int n;
      strcpy (buffer, "!bench\r\n");
      // get server benchmark data
      s->write (buffer, strlen (buffer));
      n = s->read (server_bench, sizeof(server_bench));
      if (n <= 0) {
        fprintf (stderr, "\nserver_bench: failed %d %d\n", n, errno);
      }
    }

    if (last) {
      strcpy (buffer, "!shutdown\r\n");
    } else {
      strcpy (buffer, "!quit\r\n");
    }

    s->write (buffer, strlen (buffer));
    s->read (buffer, buffer_size);
    s->close();
  } catch (const char * msg) {
    std::cerr << "client() caught an exception: ";
    std::cerr << "\"" << msg << "\"\n";
    //  } catch (...) {
    //    std::cerr << "client() caught a strange exception\n";
  }

  delete buffer;
  delete s;

  if (last) {
    coro::exit();
  }
}

int
main (int argc, char * argv[])
{
  char * host;
  int nc, port;
  int i;

  if (argc != 6) {
    fprintf (stderr, "Usage: %s <ip> <port> <nconns> <ntrans> <packet_size>\n", argv[0]);
    exit(-1);
  }
  host = argv[1];
  port = atoi (argv[2]);
  nc = atoi (argv[3]);
  ntrans = atoi (argv[4]);
  packet_size = atoi (argv[5]);

#ifdef __linux__
  poller * p = new poll_poller (2500);
#else
  poller * p = new kqueue_poller (2500, 2500);
#endif
  coro::set_poller (p);

  struct sockaddr_in addr;

  message = (char *) calloc (1, packet_size);
  junkify_buffer (message, packet_size);

  total_bytes = 0;

  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);

  if (!inet_aton (host, &(addr.sin_addr))) {
    fprintf (stderr, "invalid IP address\n");
    exit (-1);
  }

  // spawn nc clients
  for (i=0; i < nc; i++) {
    coro * c = new coro ((spawn_fun) client, (void *) &addr);
    c->schedule();
    counter++;
  }

  bench b;
  b.set();
  try {
    coro::event_loop();
  } catch (char * msg) {
    std::cerr << "event_loop() threw an exception\n";
    std::cerr << "\"" << msg << "\"\n";
  } catch (...) {
    std::cerr << "caught a different type of exception\n";
  }

  std::cerr << "server bench: " << server_bench;
  std::string diff;
  b.diff().dump(diff);
  std::cerr << "client bench: " << diff;

  std::cerr << "read_block: " << p->_rb << "  write_block: " << p->_wb << "\n";

  return 0;
}
