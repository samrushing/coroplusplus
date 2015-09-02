
// OpenSSL version...

#include "coro++.h"
#include "coro_file.h"
#include "coro_bench.h"
#include "coro_buffer.h"
#include "coro_socket.h"
#include "event_kqueue.h"
#include "event_select.h"
#include "event_poll.h"
#include "coro_openssl.h"

#include <string>
#include <iostream>

#include <stdlib.h>

// a simple echo server

static bench the_bench;

void
echo_session (coro_socket * s)
{
  try {
    coro_buffer buf(s);
    std::string banner ("Hi There. Say Something\r\n");
    s->write (banner);
    while (1) {
      std::string line;
      bool closed = buf.get (line, "\r\n");
      if (closed) {
	s->close();
	break;
      } else if (line[0] == '!') {
        if (line == "!quit\r\n") {
          s->close();
          break;
	} else if (line == "!shutdown\r\n") {
          s->close();
          coro::exit();
          break;
        } else if (line[1] == 'b') {
          std::string diff;
          the_bench.diff().dump(diff);
          s->write (diff);
        } else {
	  s->write ("unknown command");
	}
      } else {
        s->write (line);
      }
    }
  } catch (...) {
    std::cerr << "caught an exception in echo_session()\n";
  }
}

coro_ssl_ctx * the_ctx;

void
echo_server (int port)
{
  coro_socket * s = new coro_ssl (the_ctx);

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = htonl (INADDR_ANY);

  std::cerr << "echo_server starting on port " << port << "\n";

  s->set_reuse_addr();
  s->bind ((struct sockaddr *) &addr, sizeof(addr));
  s->listen (1024);

  while (1) {
    struct sockaddr client_addr;
    socklen_t addr_len = sizeof (struct sockaddr);
    coro_socket * c = s->accept ((struct sockaddr *) &client_addr, &addr_len);
    // spawn a new coroutine to handle the session
    coro * session = new coro ((spawn_fun) echo_session, (void *) c);
    session->schedule();
  }
}

typedef std::list <std::string> string_list;

int
main (int argc, char * argv[])
{

  SSL_library_init();
  SSL_load_error_strings();
  the_ctx = new coro_ssl_ctx ("cert/combined.pem");
  kqueue_poller * p = new kqueue_poller (2500, 2500);
  coro::set_poller (p);
  coro * server = new coro ((spawn_fun) echo_server, (void *) 9001);
  server->schedule();

  p->set_signal_handler (SIGINT, default_signal_handler);
  p->set_signal_handler (SIGTERM, default_signal_handler);

  try {
    coro::event_loop();
  } catch (char * msg) {
    std::cerr << "event_loop() threw an exception\n";
    std::cerr << "\"" << msg << "\"\n";
  } catch (...) {
    std::cerr << "caught a different type of exception\n";
  }
  std::cerr << "exiting main coroutine\n";

  std::cerr << "read_block: " << p->_rb << "  write_block: " << p->_wb << "\n";

  return EXIT_SUCCESS;
}
