#include "frontend.h"

#if H8_HAVE_NETWORK_IMPL

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET h8_socket_raw_t;
  #define H8_INVALID_SOCKET INVALID_SOCKET
  #define H8_CLOSE(h) closesocket(h)
  #define H8_LAST_ERROR WSAGetLastError()
  #define H8_WOULD_BLOCK(e) ((e) == WSAEWOULDBLOCK)
  typedef int h8_send_size_t;
#else
  #include <unistd.h>
  #include <errno.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  typedef int h8_socket_raw_t;
  #define H8_INVALID_SOCKET -1
  #define H8_CLOSE(h) close(h)
  #define H8_LAST_ERROR errno
  #define H8_WOULD_BLOCK(e) ((e) == EAGAIN || (e) == EWOULDBLOCK)
  typedef size_t h8_send_size_t;
#endif

/* Writing to a closed connection must report an error, not raise SIGPIPE */
#ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
#endif

struct h8_socket_s
{
  h8_socket_raw_t handle;
};

static h8_network_ctx_t fe_ctx = { NULL, "", 0, 0, 0, "" };

/**
 * The connection to the other system. For a server, this is the accepted
 * client, and the server keeps listening so a new client can connect after the
 * old one leaves. For a client, this is the socket in fe_ctx.
 */
static h8_socket_raw_t fe_peer = H8_INVALID_SOCKET;

static void h8_fe_set_error(h8_network_ctx_t *ctx, h8_network_error error,
                            const char *what, int code)
{
  snprintf(ctx->error_message, sizeof(ctx->error_message), "%s: %d", what,
           code);
  ctx->error = error;
}

static void h8_fe_set_nonblocking(h8_socket_raw_t handle)
{
#ifdef _WIN32
  u_long on = 1;
  ioctlsocket(handle, FIONBIO, &on);
#else
  fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK);
#endif
}

/** Configures a connected socket for small, latency-sensitive records */
static void h8_fe_setup_peer(h8_socket_raw_t handle)
{
  int on = 1;

  /* Without this, Nagle's algorithm can hold records back for tens of ms */
  setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(on));
#ifdef SO_NOSIGPIPE
  setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&on, sizeof(on));
#endif
  h8_fe_set_nonblocking(handle);
}

static void h8_fe_disconnect(void)
{
  if (fe_peer == H8_INVALID_SOCKET)
    return;
  if (fe_ctx.server)
    H8_CLOSE(fe_peer);
  else
  {
    /* The client's only socket is the connection itself */
    H8_CLOSE(fe_ctx.socket->handle);
    fe_ctx.socket->handle = H8_INVALID_SOCKET;
  }
  fe_peer = H8_INVALID_SOCKET;
}

/** For a server without a client, accepts one if it is waiting */
static void h8_fe_accept(void)
{
  if (fe_ctx.server && fe_ctx.socket && fe_peer == H8_INVALID_SOCKET)
  {
    h8_socket_raw_t handle = accept(fe_ctx.socket->handle, NULL, NULL);

    if (handle != H8_INVALID_SOCKET)
    {
      h8_fe_setup_peer(handle);
      fe_peer = handle;
    }
  }
}

h8_bool h8_fe_network_init(h8_network_ctx_t *ctx)
{
  struct sockaddr_in addr;
  h8_socket_raw_t handle;

#ifdef _WIN32
  WSADATA wsadata;
  int result = WSAStartup(MAKEWORD(2, 2), &wsadata);

  if (result != 0)
  {
    h8_fe_set_error(ctx, H8_NETWORK_ERROR_INIT, "WSAStartup failed", result);
    return FALSE;
  }
#endif

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)ctx->port);
  addr.sin_addr.s_addr = inet_addr(ctx->ip);
  if (addr.sin_addr.s_addr == INADDR_NONE)
  {
    snprintf(ctx->error_message, sizeof(ctx->error_message),
             "Invalid IP address: %s", ctx->ip);
    ctx->error = H8_NETWORK_ERROR_CONNECT;
    return FALSE;
  }

  handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (handle == H8_INVALID_SOCKET)
  {
    h8_fe_set_error(ctx, H8_NETWORK_ERROR_SOCKET, "Socket creation failed",
                    H8_LAST_ERROR);
    return FALSE;
  }

  if (ctx->server)
  {
    int on = 1;

    /* Allow restarting the server right after a previous one closed */
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    if (bind(handle, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
      h8_fe_set_error(ctx, H8_NETWORK_ERROR_BIND, "Server bind failed",
                      H8_LAST_ERROR);
      H8_CLOSE(handle);
      return FALSE;
    }
    if (listen(handle, 1) < 0)
    {
      h8_fe_set_error(ctx, H8_NETWORK_ERROR_LISTEN, "Server listen failed",
                      H8_LAST_ERROR);
      H8_CLOSE(handle);
      return FALSE;
    }

    /* Clients are accepted later without blocking emulation */
    h8_fe_set_nonblocking(handle);
  }
  else
  {
    if (connect(handle, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
      h8_fe_set_error(ctx, H8_NETWORK_ERROR_CONNECT, "Client connect failed",
                      H8_LAST_ERROR);
      H8_CLOSE(handle);
      return FALSE;
    }
    h8_fe_setup_peer(handle);
  }

  ctx->socket = malloc(sizeof(struct h8_socket_s));
  if (!ctx->socket)
  {
    snprintf(ctx->error_message, sizeof(ctx->error_message),
             "Socket allocation failed");
    ctx->error = H8_NETWORK_ERROR_SOCKET;
    H8_CLOSE(handle);
    return FALSE;
  }
  ctx->socket->handle = handle;
  ctx->error = H8_NETWORK_ERROR_NONE;
  ctx->error_message[0] = '\0';

  memcpy(&fe_ctx, ctx, sizeof(h8_network_ctx_t));
  fe_peer = ctx->server ? H8_INVALID_SOCKET : handle;

  return TRUE;
}

h8_bool h8_fe_network_transmit(const void *data, unsigned size)
{
  const char *buffer = (const char*)data;
  unsigned sent_total = 0;

  h8_fe_accept();
  if (fe_peer == H8_INVALID_SOCKET || !data)
    return FALSE;

  while (sent_total < size)
  {
    int sent = (int)send(fe_peer, buffer + sent_total,
                         (h8_send_size_t)(size - sent_total), MSG_NOSIGNAL);

    if (sent > 0)
      sent_total += (unsigned)sent;
    else
    {
      int err = H8_LAST_ERROR;

      if (sent < 0 && H8_WOULD_BLOCK(err))
      {
        /* The send buffer is full; wait briefly rather than split a record */
        fd_set writable;
        struct timeval timeout;

        FD_ZERO(&writable);
        FD_SET(fe_peer, &writable);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        if (select((int)fe_peer + 1, NULL, &writable, NULL, &timeout) > 0)
          continue;
      }
      h8_fe_set_error(&fe_ctx, H8_NETWORK_ERROR_TRANSMIT, "Send failed", err);
      h8_fe_disconnect();
      return FALSE;
    }
  }

  return TRUE;
}

unsigned h8_fe_network_receive(void *data, unsigned size)
{
  int received;

  h8_fe_accept();
  if (fe_peer == H8_INVALID_SOCKET || !data || !size)
    return 0;

  received = (int)recv(fe_peer, (char*)data, (h8_send_size_t)size, 0);
  if (received > 0)
    return (unsigned)received;
  else if (received == 0)
  {
    /* The other side closed the connection */
    h8_fe_disconnect();
    return 0;
  }
  else
  {
    int err = H8_LAST_ERROR;

    if (!H8_WOULD_BLOCK(err))
    {
      h8_fe_set_error(&fe_ctx, H8_NETWORK_ERROR_RECEIVE, "Receive failed",
                      err);
      h8_fe_disconnect();
    }
    return 0;
  }
}

#elif H8_HAVE_NETWORK_STUB

h8_bool h8_fe_network_init(h8_network_ctx_t *ctx)
{
  (void)ctx;
  return FALSE;
}

h8_bool h8_fe_network_transmit(const void *data, unsigned size)
{
  (void)data;
  (void)size;
  return FALSE;
}

unsigned h8_fe_network_receive(void *buffer, unsigned size)
{
  (void)buffer;
  (void)size;
  return 0;
}

#endif
