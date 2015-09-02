// -*- Mode: C++ -*-

#ifndef _CORO_S2N_H
#define _CORO_S2N_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string>
#include <iostream>

#include <s2n.h>
#include "coro_socket.h"

class S2N_Error : public Coro_Error {
public:
  S2N_Error (const char * fun) : Coro_Error (fun) {};
};

class coro_s2n_cfg {

private:

  struct s2n_config * _cfg;

public:

  coro_s2n_cfg (const char * cert_chain_pem, const char * private_key_pem)
  {
    _cfg = s2n_config_new();
    if (!_cfg) {
      throw S2N_Error ("s2n_config_new()");
    } else if (s2n_config_add_cert_chain_and_key (_cfg, (char*)cert_chain_pem, (char*)private_key_pem) < 0) {
      throw S2N_Error ("s2n_config_add_cert_chain_and_key()");
    }
  };

  struct s2n_config *
  get_cfg()
  {
    return _cfg;
  }

  ~coro_s2n_cfg()
  {
    if (_cfg) {
      s2n_config_free (_cfg);
    }
  };

  // XXX next_protos, etc...

};

class coro_s2n : public coro_socket {
private:
  coro_s2n_cfg * _cfg;
  s2n_connection * _conn;
  bool _negotiated;
public:

  coro_s2n (coro_s2n_cfg * cfg, int fd=-1, s2n_mode mode=S2N_SERVER) :
    coro_socket (fd, PF_INET, SOCK_STREAM),
    _cfg(cfg),
    _negotiated (false)
  {
    _conn = s2n_connection_new (mode);
    if (!_conn) {
      throw S2N_Error ("s2n_connection_new");
    } else if (s2n_connection_set_config (_conn, _cfg->get_cfg()) < 0) {
      throw S2N_Error ("s2n_connection_set_config");
    } else if (s2n_connection_set_fd (_conn, _fd)) {
      throw S2N_Error ("s2n_connection_set_fd");
    }
  };

  ~coro_s2n()
  {
    if (_conn) {
      s2n_connection_free (_conn);
    }
  }

  coro_s2n *
  accept (struct sockaddr * addr, socklen_t * length_ptr)
  {
    return new coro_s2n (_cfg, _accept (addr, length_ptr), S2N_SERVER);
  }

  int
  _check (const char * fun, int r)
  {
    if (r < 0) {
      if (errno == EWOULDBLOCK) {
	return 0;
      } else {
	throw S2N_Error (fun);
      }
    } else {
      return r;
    }
  }

  bool
  _wait_on_event (s2n_blocked_status & blocked)
  {
    switch (blocked) {
      case S2N_BLOCKED_ON_READ:
	wait_for_read();
	return false;
      case S2N_BLOCKED_ON_WRITE:
	wait_for_write();
	return false;
      case S2N_NOT_BLOCKED:
	return true;
    }
  }

  // we use the _negotiated flag in order to avoid doing
  //   s2n_negotiate() inside an accept thread.
  void
  _check_negotiate()
  {
    if (!_negotiated) {
      s2n_blocked_status blocked = S2N_NOT_BLOCKED;
      while (1) {
	_check ("s2n_negotiate", s2n_negotiate (_conn, &blocked));
	if (_wait_on_event (blocked)) {
	  break;
	}
      }
      _negotiated = true;
    }
  }


  ssize_t
  read (void * buffer, size_t length)
  {
    _check_negotiate();
    s2n_blocked_status blocked = S2N_NOT_BLOCKED;
    ssize_t nbytes = 0;
    char * p = (char *) buffer;
    while (1) {
      nbytes += _check ("s2n_recv", s2n_recv (_conn, p + nbytes, length - nbytes, &blocked));
      if (_wait_on_event (blocked)) {
	break;
      }
    }
    return nbytes;
  }

  void
  write (const void * buffer, size_t length)
  {
    _check_negotiate();
    ssize_t nbytes = 0;
    s2n_blocked_status blocked = S2N_NOT_BLOCKED;
    char * p = (char *) buffer;
    while (1) {
      nbytes += _check ("s2n_send", s2n_send (_conn, p + nbytes, length - nbytes, &blocked));
      if (_wait_on_event (blocked)) {
	break;
      }
    }
  }

  void
  shutdown (int how)
  {
    // how is ignored
    s2n_blocked_status blocked = S2N_NOT_BLOCKED;
    while (1) {
      _check ("s2n_shutdown", s2n_shutdown (_conn, &blocked));
      if (_wait_on_event (blocked)) {
	break;
      }
    }
  }

};

#endif // _CORO_S2N_H
