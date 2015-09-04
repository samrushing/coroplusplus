// -*- Mode: C++ -*-

// XXX counting_socket equivalent to keep track of how many fd's
// we have open, block on unavailable, etc...

#include "coro++.h"
#include "coro_file.h"
#include "coro_buffer.h"
#include "coro_socket.h"
#include "event_kqueue.h"
//#include "event_select.h"

#include <stdlib.h>

#include <list>
#include <string>
#include <algorithm>

std::string::size_type
split_first (std::string & result, std::string & source, std::string & delims)
{
  std::string::size_type i = source.find_first_of (delims);
  if (i == std::string::npos) {
    result = "";
    return 0;
  } else {
    result = source.substr (0, i);
    return i;
  }
}

std::string::size_type
find_email_addr (std::string & result, std::string & line)
{
  // search a command line for email address in brackets
  std::string::size_type i = line.find_first_of ("<");
  std::string::size_type j = line.find_last_of (">");

  if (i == std::string::npos || j == std::string::npos) {
    return 0;
  } else {
    result = line.substr (i+1, j-1);
    return (j-i)-2;
  }
}

// a simple smtp server

typedef std::list < std::string > string_list;

class smtp_session {

private:
  static std::string _hostname;
  static int _counter;
  int _id;
  coro_socket * _s;
  coro_buffer * _b;
  string_list _rcpts;
  std::string _env_from;

public:

  smtp_session (coro_socket * s) {
    _s = s;
    _b = new coro_buffer (_s);
    _id = _counter++;
  }

  ~smtp_session() {
    delete _s;
    delete _b;
    std::cerr << "smtp session " << _id << " destroyed\n";
  }

  static
  void set_hostname (void) {
    char buffer[1024];
    gethostname (buffer, sizeof(buffer));
    _hostname = buffer;
  }

  void
  send_reply (const std::string& c) {
    // ugh, we should make _b do an outgoing buffer
    _s->write (c + "\r\n");
    //_s->write (c);
    //_s->write ("\r\n");
  }

  void go () {
    send_reply ("250 " + _hostname + " SMTP");
    std::string line;
    std::string delim = " \t\r\n";

    while (1) {

      _b->get (line, "\r\n");

      std::string command;
      split_first (command, line, delim);

      transform (command.begin(), command.end(), command.begin(), tolower);

      if (command == "quit") {
	// std::cerr << "got quit for " << _id << "\n";
	send_reply ("221 bye.");
	_s->close();
	//	string_list::iterator i;
	//	for (i = _rcpts.begin(); i != _rcpts.end(); i++) {
	//	  std::cerr << "rcpt(" << (*i) << ")\n";
	//	}
	break;
      } else if (command == "rcpt") {
	if (_env_from.size()) {
	  std::string addr;
	  if (find_email_addr (addr, line) > 0) {
	    //	    _rcpts.push_back (addr);
	    send_reply ("250 Ok");
	  } else {
	    send_reply ("501 Syntax Error");
	  }
	} else {
	  send_reply ("503 send MAIL command first");
	}
      } else if (command == "helo") {
	send_reply ("250 " + _hostname);
      } else if (command == "ehlo") {
	send_reply ("250-" + _hostname + "\r\n250-PIPELINING\r\n250 8BITMIME");
      } else if (command == "mail") {
	if (find_email_addr (_env_from, line) > 0) {
	  send_reply ("250 Ok");
	} else {
	  send_reply ("501 Syntax Error");
	}
      } else if (command == "data") {
	send_reply ("354 Ok");
	// fetch body
#if 0
	std::string body;
	_b->get (body, "\r\n.\r\n");
	std::cerr << "got body on " << _id << ", " << body.size() << " bytes\n";
        std::cerr << ".";
#else
	// using the string_list accumulator...
	string_list body_parts;
	_b->get (body_parts, "\r\n.\r\n");
	std::cerr << "body parts {\n";
	for (std::string & part : body_parts) {
	  std::cerr << "part (" << part << ")\n";
	}
	std::cerr << "}\n";
#endif
	_env_from = "";
	_rcpts.clear();
	send_reply ("250 Ok");
      } else if (command == "rset") {
	_env_from = "";
	_rcpts.clear();
	send_reply ("250 Ok");
      } else {
	send_reply ("502 command not implemented");
      }
    }
    //std::cerr << "leaving command loop... for " << _id << "\n";
  }

};

std::string smtp_session::_hostname = "";
int smtp_session::_counter = 0;

void
smtp_session_entry (coro_socket * s)
{
  try {
    smtp_session ss(s);
    ss.go();
    //std::cerr << "after ss.go()\n";
  } catch (...) {
    s->close();
    std::cerr << "exception thrown in client...\n";
  }
}

void
smtp_server (int port)
{
  coro_socket * s = new coro_socket;

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = htonl (INADDR_ANY);

  s->set_reuse_addr();
  s->bind ((struct sockaddr *) &addr, sizeof(addr));
  s->listen (1024);

  while (1) {
    struct sockaddr client_addr;
    socklen_t addr_len = sizeof (struct sockaddr);
    coro_socket * c = s->accept ((struct sockaddr *) &client_addr, &addr_len);
    // spawn a new coroutine to handle the session
    coro * session = new coro ((spawn_fun) smtp_session_entry, (void *) c);
    session->schedule();
  }
}

int
main (int argc, char * argv[])
{
  smtp_session::set_hostname();
  poller * p = new kqueue_poller (2500, 2500);
  //poller * p = new select_poller();
  coro::set_poller (p);
  coro * server = new coro ((spawn_fun) smtp_server, (void *) 9025);
  server->schedule();
  coro::event_loop();
  std::cerr << "exiting main coroutine\n";
  return EXIT_SUCCESS;
}


//
// Local variables:
// compile-command: "make smtp_server"
// End:
//
