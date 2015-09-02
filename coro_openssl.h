// -*- Mode: C++ -*-

#ifndef _CORO_OPENSSL_H
#define _CORO_OPENSSL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string>
#include <iostream>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "coro_socket.h"

// note: use of openssl/err.h functions requires linking to -lcrypto as well as -lssl

// might be able to skip mem_bio, rsa, pkey, and x509 if we just have a dumb ssl_ctx constructor.
// also helps to assume everything is one file...

class SSL_Error : public Coro_Error {
public:
  SSL_Error (const char * fun) : Coro_Error (fun) {};
};

class coro_ssl_ctx;

int next_protos_server_callback (SSL * ssl, const unsigned char **out, unsigned int *outlen, void * arg);

class coro_ssl_ctx {
public:
  SSL_CTX * _ctx;
  std::string _next_protos;

  coro_ssl_ctx (const char * combined_file)
  {
    const SSL_METHOD * meth;
    meth = SSLv23_method();
    _ctx = SSL_CTX_new (meth);
    if (!_ctx) {
      throw SSL_Error ("SSL_CTX_new()");
    } else if (!SSL_CTX_use_certificate_chain_file (_ctx, combined_file)) {
      throw SSL_Error ("SSL_CTX_use_certificate_chain_file()");
    } else if (!SSL_CTX_use_PrivateKey_file (_ctx, combined_file, SSL_FILETYPE_PEM)) {
      throw SSL_Error ("SSL_CTX_use_PrivateKey_file()");
    }
  };

  ~coro_ssl_ctx()
  {
    if (_ctx) {
      SSL_CTX_free (_ctx);
    }
  };

  void
  set_proto (int proto)
  {
    SSL_CTX_set_options (_ctx, SSL_CTX_get_options (_ctx) | proto);
  }

  typedef std::list <std::string> string_list;
  void
  set_next_protos (string_list next_protos)
  {
    for (string_list::iterator i = next_protos.begin(); i != next_protos.end(); ++i) {
      _next_protos += (char) i->size();
      _next_protos += *i;
    }
    SSL_CTX_set_next_protos_advertised_cb (_ctx, next_protos_server_callback, (void *) this);
  }

};

int
next_protos_server_callback (SSL * ssl, const unsigned char **out, unsigned int *outlen, void * arg)
{
  coro_ssl_ctx * self = (coro_ssl_ctx *) arg;
  *out = (unsigned char *) self->_next_protos.c_str();
  *outlen = self->_next_protos.size();
  return SSL_TLSEXT_ERR_OK;
}


class coro_ssl : public coro_socket {
private:
  coro_ssl_ctx * _ctx;
  SSL * _ssl;
public:
  coro_ssl (coro_ssl_ctx * ctx, int fd=-1) :
    coro_socket (fd, PF_INET, SOCK_STREAM),
    _ctx (ctx)
  {
    _ssl = SSL_new (_ctx->_ctx);
    if (!_ssl) {
      throw SSL_Error ("SSL_new()");
    }
    if (SSL_set_fd (_ssl, _fd) == 0) {
      throw SSL_Error ("SSL_set_fd()");
    }
  };

  std::string
  get_next_proto_negotiated()
  {
    const unsigned char * p;
    unsigned int len;
    SSL_get0_next_proto_negotiated (_ssl, &p, &len);
    return std::string ((const char*) p, len);
  }

  void
  throw_ssl_error()
  {
    int error = ERR_get_error();
    if (error == SSL_ERROR_SYSCALL) {
      throw OS_Error ("ssl", errno);
    } else {
      const char * error_string = ERR_error_string (error, NULL);
      ERR_clear_error();
      throw SSL_Error (error_string);
    }
  }

  void
  wait_or_error (int code)
  {
    int error = SSL_get_error (_ssl, code);
    if (error == SSL_ERROR_WANT_READ) {
      wait_for_read();
    } else if (error == SSL_ERROR_WANT_WRITE) {
      wait_for_write();
    } else if (error == SSL_ERROR_SYSCALL) {
      throw OS_Error ("wait_or_error", errno);
    } else {
      throw_ssl_error();
    }
  };

  ssize_t
  read (char * buffer, size_t length)
  {
    while (1) {
      int r = SSL_read (_ssl, buffer, length);
      if (r < 0) {
	wait_or_error (r);
      } else {
	return r;
      }
    }
  };

  void
  write (const void * buffer, size_t length)
  {
    while (1) {
      int r = SSL_write (_ssl, (char *) buffer, length);
      if (r <= 0) {
	wait_or_error (r);
      } else {
	// assumes !SSL_MODE_ENABLE_PARTIAL_WRITE
	return;
      }
    }
  };

  void
  listen (unsigned int backlog)
  {
    coro_socket::listen (backlog);
    SSL_set_accept_state (_ssl);
  }

  void
  ssl_accept()
  {
    while (1) {
      int r = SSL_accept (_ssl);
      if (r <= 0) {
	wait_or_error (r);
      } else {
	break;
      }
    }
  }

  coro_ssl *
  accept (struct sockaddr * addr, socklen_t * length_ptr)
  {
    while (1) {
      int fd = ::accept (_fd, addr, length_ptr);
      if (fd > 0) {
	coro_ssl * conn = new coro_ssl (_ctx, fd);
	// XXX this should not happen here, because accept is done by the server coro.
	conn->ssl_accept();
	return conn;
      } else if (errno == EWOULDBLOCK) {
        wait_for_read();
      } else {
	throw OS_Error ("accept", errno);
      }
    }
  };

  void
  shutdown (int how)
  {
    // how is ignored
    while (1) {
      int r = SSL_shutdown (_ssl);
      if (r < 0) {
	wait_or_error (r);
      } else {
	return;
      }
    }
  }

};

#endif // _CORO_OPENSSL_H
