// -*- Mode: C++ -*-

#ifndef _CORO_BUFFER_H
#define _CORO_BUFFER_H

#include "coro_file.h"
#include <string>

// XXX should we have an outgoing buffer as well?
// XXX should we have char[PAGE] buffer as ivar?

typedef std::list<std::string> string_list;

// still no std template for this?
void
string_join (std::string & result, string_list & sl)
{
  // first pass, get length
  ssize_t size = result.size();
  for (string_list::iterator i = sl.begin(); i != sl.end(); ++i) {
    size += i->size();
  }
  // second pass, copy
  result.reserve (size);
  for (string_list::iterator i = sl.begin(); i != sl.end(); ++i) {
    result += *i;
  }
}

class coro_buffer {

protected:
  coro_file * _f;
  std::string _buffer;

public:
  coro_buffer (coro_file * f) : _f(f) {}

  // get data from <_f>, appending pieces to it to <sl>, until we see <delim>
  // Returns: boolean - has the stream closed?
  bool
  get (string_list & sl, std::string delim)
  {
    std::string::size_type matched = 0;
    while (1) {
      if ((_buffer.size() == 0) && (_f->read (_buffer) == 0)) {
	return true;
      } else {
	for (std::string::size_type i = 0; i < _buffer.size(); ++i) {
	  if (_buffer[i] == delim[matched]) {
	    matched += 1;
	    if (matched == delim.size()) {
	      sl.push_back (_buffer.substr (0, i + 1));
	      _buffer = _buffer.substr (i + 1);
	      return false;
	    }
	  } else {
	    matched = 0;
	  }
	}
	sl.push_back (_buffer);
	_buffer.clear();
      }
    }
  }

  bool
  get (std::string & s, std::string delim)
  {
    string_list sl;
    s.clear();
    bool closed = get (sl, delim);
    string_join (s, sl);
    return closed;
  }

  bool
  get (std::string& s, const char * delim = "\r\n")
  {
    return get (s, std::string(delim));
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
      _buffer.clear();
    }
    return bsize;
  }

  void
  read_exact (size_t nbytes, consumer &c)
  {
    std::string::size_type bsize = _buffer.size();
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
    std::string::size_type bsize = _buffer.size();
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
