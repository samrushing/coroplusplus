// -*- Mode: C++ -*-

#ifndef _CORO_SOCKET_H
#define _CORO_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string>
#include <iostream>

// as a favor to 99.9999% of our users
#include <netinet/in.h>

class coro_socket : public coro_file {

public:

  coro_socket (int fd=-1, int family=PF_INET, int type=SOCK_STREAM, int protocol=0) {
    if (fd == -1) {
      int fd = ::socket (family, type, protocol);
      if (fd != -1) {
	_fd = fd;
	set_blocking (false);
      } else {
	throw OS_Error ("socket", errno);
      }
    } else {
      _fd = fd;
    }
  }

  void
  set_reuse_addr (void)
  {
    int old;
    socklen_t optlen = sizeof old;
    if (::getsockopt (_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &old, &optlen) < 0) {
      throw OS_Error ("getsockopt", errno);
    }
    old |= 1;
    if (::setsockopt (_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &old, optlen) < 0) {
      throw OS_Error ("setsockopt", errno);
    }
  }

  void
  bind (struct sockaddr * addr, size_t length)
  {
    if (::bind (_fd, addr, length) < 0) {
      throw OS_Error ("bind", errno);
    }
  }

  virtual
  void
  listen (unsigned int backlog)
  {
    if (::listen (_fd, backlog) < 0) {
      throw OS_Error ("listen", errno);
    }
  }

  int
  _accept (struct sockaddr * addr, socklen_t * length_ptr)
  {
    while (1) {
      int fd = ::accept (_fd, addr, length_ptr);
      if (fd > 0) {
        return fd;
      } else if (errno == EWOULDBLOCK) {
        wait_for_read();
      } else {
	throw OS_Error ("accept", errno);
      }
    }
  }

  virtual
  coro_socket *
  accept (struct sockaddr * addr, socklen_t * length_ptr)
  {
    return new coro_socket (_accept (addr, length_ptr));
  }

  virtual
  void
  connect (struct sockaddr * addr, socklen_t length_ptr)
  {
    int n = ::connect (_fd, addr, length_ptr);
    if (n < 0) {
      if (errno == EWOULDBLOCK) {
	wait_for_write();
      } else if (errno == EINPROGRESS) {
	wait_for_write();
      } else {
	throw OS_Error ("connect", errno);
      }
    }
  }

  virtual
  void
  shutdown (int how=SHUT_RDWR)
  {
    int n = ::shutdown (_fd, how);
    if (n < 0) {
      throw OS_Error ("shutdown", errno);
    }
  }

};

#endif // _CORO_SOCKET_H
