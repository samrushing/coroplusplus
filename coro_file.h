// -*- Mode: C++ -*-

#ifndef _CORO_FILE_H
#define _CORO_FILE_H

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>
#include <iostream>

class consumer {
public:
  virtual void operator() (const std::string s) = 0;
};

// XXX TODO:
//  optimistic syscall + selfishness

#define CORO_EXCEPTION_ON_DESTROYING_OPEN_DESCRIPTOR 1

class Destroying_Open_Descriptor : public Coro_Error {
public:
  int _fd;
  Destroying_Open_Descriptor (int fd) : Coro_Error ("coro_file destroyed with open descriptor") {
    _fd = fd;
  };
};

class coro_file {

protected:

  int _fd;

public:

  coro_file() : _fd(0) {}
  coro_file (int fd) {_fd = fd;}

  virtual ~coro_file()
  {
    if (_fd != -1) {
#if 0 // CORO_EXCEPTION_ON_DESTROYING_OPEN_DESCRIPTOR
      throw Destroying_Open_Descriptor (_fd);
#else
      close();
#endif
    }
  }

  int get_fd() { return _fd; }

  void
  close() {
    // XXX check result
    ::close (_fd);
    // invalidate this fd
    // std::cerr << "close(" << _fd << ")\n";
    _fd = -1;
  }

  // kqueue gives us information about how much we can read
  // and write.  How can the API accommodate this?

  void
  wait_for_read (int timeout=0)
  {
    coro * me = coro::current();
    if (me == 0) {
      throw Yield_From_Main();
    } else {
      coro::get_poller()->wait_for_read (_fd, me);
    }
  }

  void
  wait_for_write (int timeout=0)
  {
    coro * me = coro::current();
    if (me == 0) {
      throw Yield_From_Main();
    } else {
      coro::get_poller()->wait_for_write (_fd, me);
    }
  }

  void set_blocking (bool blocking)
  {
    int flag;
    flag = ::fcntl (_fd, F_GETFL, 0);
    if (blocking) {
      flag &= (~O_NDELAY);
    } else {
      flag |= (O_NDELAY);
    }
    if (::fcntl (_fd, F_SETFL, flag) < 0) {
      throw OS_Error ("fcntl", errno);
    }
  }

  // writev would be nice: however the logic for handling an incomplete
  //   write is a bit tricky.

  // read/write

  virtual
  void
  write (const void * buffer, size_t length)
  {
    char * p = (char *) buffer;
    while (length) {
      int r = ::write (_fd, p, length);
      if (r > 0) {
	length -= r;
	p += r;
      } else if (errno == EWOULDBLOCK) {
        wait_for_write();
      } else {
	throw OS_Error ("write", errno);
      }
    }
  }

  void
  write (const std::string &s)
  {
    write (s.data(), s.size());
  }

  void
  write (const char * s)
  {
    write (s, strlen (s));
  }

  virtual
  ssize_t
  read (char * buffer, size_t length)
  {
    while (1) {
      ssize_t n = ::read (_fd, buffer, length);
      if (n >= 0) {
        return n;
      } else if (errno == EWOULDBLOCK) {
        wait_for_read();
      } else {
	throw OS_Error ("read", errno);
      }
    }
  }

  // XXX another thing to consider... a more common use case here might be
  //     to *append* to a string reference, not assume that we can empty it
  size_t
  read (std::string &s, size_t length = 8192)
  {
    // XXX not sure what's the best way to do this... I'm *trying* to avoid declaring
    //   a char[8192] on the stack, which just bloats the stack copy of every thread
    //   waiting on a read.  [does vector<char> end up on the stack anyway?]
    // XXX another option might be to use malloc/free, but exceptions and RAII seem to
    //   want to rule this out completely.
    std::vector<char> buffer;
    buffer.resize (length);
    int n = read (&buffer[0], length);
    // XXX is this an expensive operation that I should avoid?
    buffer.resize (n);
    // XXX does this make *yet another copy*??
    s = std::string (buffer.begin(), buffer.end());
    return n;
  }

  void
  read_exact (size_t nbytes, consumer &c, unsigned int block_size=8000)
  {
    std::string s = "";
    while (nbytes) {
      nbytes -= read (s, std::min (nbytes, (size_t)block_size));
      c (s);
      s = "";
    }
  }

  typedef std::list <std::string> string_list;
  void
  read_exact (size_t nbytes, string_list & dst, unsigned int block_size=8000)
  {
    while (nbytes) {
      std::string s;
      nbytes -= read (s, std::min (nbytes, (size_t)block_size));
      dst.push_back (s);
    }
  }

  void
  read_exact (size_t nbytes, std::string & dst)
  {
    while (nbytes) {
      std::string buf;
      int nread = read (buf, nbytes);
      if (nread == 0) {
	return;
      } else {
	dst += buf;
	nbytes -= nread;
      }
    }
  }

};


#endif // _CORO_FILE_H
