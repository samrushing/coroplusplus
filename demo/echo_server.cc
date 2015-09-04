
#include "coro++.h"
#include "coro_file.h"
#include "coro_bench.h"
#include "coro_buffer.h"
#include "coro_socket.h"
#include "event_kqueue.h"
#include "event_select.h"
#include "event_poll.h"

#include <string>
#include <iostream>

#include <stdlib.h>

#ifdef USE_S2N
#include "certkey.h"
#include "coro_s2n.h"
#elif USE_OPENSSL
#include "coro_openssl.h"
#endif

// a simple echo server

static bench the_bench;

// NOTE: this expects 'lines' of input delimited by CRLF.
//  [so if you're using "openssl s_client", add the -crlf argument]

void
echo_session (coro_socket * s)
{
  // take ownership of <s>
  std::auto_ptr<coro_socket> owner(s);
  try {
    coro_buffer buf(s);
    std::string line;

    while (1) {
      bool closed = buf.get (line);
      if (closed) {
        s->close();
	break;
      } else if (line[0] == '!') {
        if (line[1] == 'q') {	// quit
	  s->shutdown();
          s->close();
          break;
        } else if (line[1] == 's') { // shutdown
	  s->shutdown();
          s->close();
          coro::exit();
          break;
        } else if (line[1] == 'b') { // benchmark
          std::string diff;
          the_bench.diff().dump(diff);
          s->write (diff);
        }
      } else {
        s->write (line);
      }
    }
  } catch (OS_Error & e) {
    std::cerr << "OS_Error fun=" << e._fun << " error=" << strerror (e._errno) << std::endl;
  } catch (Coro_Error & e) {
    std::cerr << "exception in echo_server: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "caught an exception in echo_session()\n";
  }
}

void
echo_server (int port)
{
  try {
#ifdef USE_S2N
    s2n_init();
    coro_s2n_cfg * cfg = new coro_s2n_cfg (crt, key);
    coro_s2n * s = new coro_s2n (cfg);
#elif USE_OPENSSL
    SSL_library_init();
    SSL_load_error_strings();
    coro_ssl_ctx * ctx = new coro_ssl_ctx ("cert/combined.pem");
    coro_socket * s = new coro_ssl (ctx);
#else
    coro_socket * s = new coro_socket();
#endif

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
  } catch (OS_Error & e) {
    std::cerr << "OS_Error fun=" << e._fun << " error=" << strerror (e._errno) << std::endl;
  } catch (Coro_Error & e) {
    std::cerr << "exception in echo_server: " << e.what() << std::endl;
  }
}

// test/demo of a member thread function.
class thing : public spawner {
public:
  int _x;
  thing (int x) : _x (x) {};
  void thing_thread (void * arg0) {
    coro * me = coro::current();
    int i = 0;
    while (1) {
      std::cerr << "thing thread " << _x << " #" << i << "\n";
      me->sleep (5000);
      i++;
    }
  }
};

int
main (int argc, char * argv[])
{
  int ch;
  int ksp = 0; // default to kqueue

  while ((ch = getopt (argc, argv, "ksp")) != -1) {
    switch (ch) {
    case 'k':
      ksp = 0;
      break;
    case 's':
      ksp = 1;
      break;
    case 'p':
      ksp = 2;
      break;
    default:
      std::cerr << argv[0] << " [ -k (kqueue) | -s (select) | -p (poll) ]\n";
      exit (EXIT_FAILURE);
      break;
    }
  }

  poller * p = 0;
  switch (ksp) {
  case 0:
    p = new kqueue_poller (2500, 2500);
    std::cerr << "using kqueue()\n";
    break;
  case 1:
    p = new select_poller();
    std::cerr << "using select()\n";
    break;
  case 2:
    p = new poll_poller (2500);
    std::cerr << "using poll()\n";
    break;
  default:
    break;
  }

  coro::set_poller (p);
  coro * server = new coro ((spawn_fun) echo_server, (void *) 9001);
  server->schedule();

  the_bench.set();

  thing T (99);
  coro * thingum = new coro ((spawn_method)(&thing::thing_thread), (void *) &T);
  thingum->schedule();

  std::cerr << "coro::stack_base = " << coro::_stack_base << "\n";

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
