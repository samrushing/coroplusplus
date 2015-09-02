// -*- Mode: C++ -*-

#ifndef _CORO_BUFFER_H
#define _CORO_BUFFER_H

#include "coro_file.h"
#include <string>

// XXX should we have an outgoing buffer as well?
// XXX should we have char[PAGE] buffer as ivar?

class coro_buffer {

protected:
  coro_file * _f;
  std::string _buffer;

public:
  coro_buffer (coro_file * f) : _f(f) {}

  // get data from <_f>, appending it to <s>, until we see <delim>
  // Returns: boolean - has the stream closed?
  bool
  get (std::string& s, const char * delim = "\r\n")
  {
    std::string::size_type i = _buffer.find (delim);
    if (i == std::string::npos) {
      // not here, get another buffer
      int n = _f->read (_buffer);
      if (!n) {
	return true;
      } else {
	return get (s, delim); // tail call
      }
    } else {
      i += strlen(delim); // include the delimiter?
      s = _buffer.substr (0, i);
      _buffer = _buffer.substr (i);
      return false;
    }
  }

  std::string
  flush ()
  {
    std::string result = _buffer;
    _buffer = "";
    return result;
  }

  size_t
  consume_buffer (consumer &c)
  {
    size_t bsize = _buffer.size();
    if (bsize > 0) {
      c (_buffer);
      _buffer = "";
    }
    return bsize;
  }

  void
  read_exact (size_t nbytes, consumer &c)
  {
    int bsize = _buffer.size();
    if (bsize > nbytes) {
      c (_buffer.substr (0, nbytes));
      _buffer = _buffer.substr (nbytes);
    } else if (bsize == nbytes) {
      c (_buffer);
      _buffer = "";
    } else {
      nbytes -= consume_buffer (c);
      _f->read_exact (nbytes, c);
    }
  }

  // note: appends onto <s>
  void
  read_exact (size_t nbytes, std::string & s)
  {
    int bsize = _buffer.size();
    if (bsize > nbytes) {
      s += _buffer.substr (0, nbytes);
      _buffer = _buffer.substr (nbytes);
    } else if (bsize == nbytes) {
      s += _buffer;
      _buffer = "";
    } else {
      s += _buffer;
      nbytes -= _buffer.size();
      _buffer = "";
      _f->read_exact (nbytes, s);
    }
  }

  void
  read_to_eof (consumer &c, size_t block_size=8000)
  {
    consume_buffer (c);
    std::string block;
    while (_f->read (block, block_size) > 0) {
      c (block);
    }
  }


};


#endif // _CORO_BUFFER_H
