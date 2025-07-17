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
  #define H8_CLOSESOCKET(s) closesocket((s)->handle)
  #define H8_SOCKET_VALID(s) ((s)->handle != INVALID_SOCKET)
#else
  #include <unistd.h>
  #include <errno.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  typedef int h8_socket_raw_t;
  #define H8_INVALID_SOCKET -1
  #define H8_CLOSESOCKET(s) close((s)->handle)
  #define H8_SOCKET_VALID(s) ((s)->handle >= 0)
#endif

struct h8_socket_s
{
  h8_socket_raw_t handle;
};

static h8_network_ctx_t *fe_ctx = NULL;

h8_bool h8_fe_network_init(h8_network_ctx_t *ctx)
{
  int result;

#ifdef _WIN32
  WSADATA wsadata;
  result = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (result != 0)
  {
    snprintf(ctx->error_message, sizeof(ctx->error_message),
             "WSAStartup failed: %d", result);
    ctx->error = H8_NETWORK_ERROR_INIT;
    return FALSE;
  }
#endif

  ctx->socket = malloc(sizeof(struct h8_socket_s));
  if (!ctx->socket)
  {
    snprintf(ctx->error_message, sizeof(ctx->error_message),
             "Socket allocation failed");
    ctx->error = H8_NETWORK_ERROR_SOCKET;
    return FALSE;
  }

  ctx->socket->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ctx->socket->handle == H8_INVALID_SOCKET)
  {
#ifdef _WIN32
    WSACleanup();
    int err = WSAGetLastError();
#else
    int err = errno;
#endif
    snprintf(ctx->error_message, sizeof(ctx->error_message),
             "Socket creation failed: %d", err);
    ctx->error = H8_NETWORK_ERROR_SOCKET;
    free(ctx->socket);
    ctx->socket = NULL;
    return FALSE;
  }
  else
  {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->port);
    addr.sin_addr.s_addr = inet_addr(ctx->ip);

    if (addr.sin_addr.s_addr == INADDR_NONE)
    {
      H8_CLOSESOCKET(ctx->socket);
      free(ctx->socket);
      ctx->socket = NULL;
  #ifdef _WIN32
      WSACleanup();
  #endif
      snprintf(ctx->error_message, sizeof(ctx->error_message),
              "Invalid IP address: %s", ctx->ip);
      ctx->error = H8_NETWORK_ERROR_CONNECT;

      return FALSE;
    }

    if (ctx->server)
    {
      result = bind(ctx->socket->handle, (struct sockaddr*)&addr, sizeof(addr));
      if (result < 0)
      {
        H8_CLOSESOCKET(ctx->socket);
        free(ctx->socket);
        ctx->socket = NULL;
  #ifdef _WIN32
        WSACleanup();
        int err = WSAGetLastError();
  #else
        int err = errno;
  #endif
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                "Server bind failed: %d", err);
        ctx->error = H8_NETWORK_ERROR_BIND;

        return FALSE;
      }

      result = listen(ctx->socket->handle, SOMAXCONN);
      if (result < 0)
      {
        H8_CLOSESOCKET(ctx->socket);
        free(ctx->socket);
        ctx->socket = NULL;
  #ifdef _WIN32
        WSACleanup();
        int err = WSAGetLastError();
  #else
        int err = errno;
  #endif
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                "Server listen failed: %d", err);
        ctx->error = H8_NETWORK_ERROR_LISTEN;

        return FALSE;
      }
    }
    else
    {
      result = connect(ctx->socket->handle, (struct sockaddr*)&addr, sizeof(addr));
      if (result < 0)
      {
        H8_CLOSESOCKET(ctx->socket);
        free(ctx->socket);
        ctx->socket = NULL;
  #ifdef _WIN32
        WSACleanup();
        int err = WSAGetLastError();
  #else
        int err = errno;
  #endif
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                "Client connect failed: %d", err);
        ctx->error = H8_NETWORK_ERROR_CONNECT;

        return FALSE;
      }
    }
  }
  ctx->error = H8_NETWORK_ERROR_NONE;
  ctx->error_message[0] = '\0';
  fe_ctx = ctx;

  return TRUE;
}


h8_bool h8_fe_network_transmit(const void *data, unsigned size)
{
  if (!fe_ctx || !H8_SOCKET_VALID(fe_ctx->socket) || !data || size == 0)
  {
    snprintf(fe_ctx->error_message, sizeof(fe_ctx->error_message),
             "Invalid transmit parameters");
    fe_ctx->error = H8_NETWORK_ERROR_TRANSMIT;
    return FALSE;
  }

  size_t total_sent = 0;
  const char *buffer = (const char*)data;

  while (total_sent < size)
  {
#ifdef _WIN32
    int sent = send(fe_ctx->socket->handle, buffer + total_sent, (int)(size - total_sent), 0);
#else
    ssize_t sent = send(fe_ctx->socket->handle, buffer + total_sent, size - total_sent, 0);
#endif
    if (sent <= 0)
    {
#ifdef _WIN32
      int err = WSAGetLastError();
#else
      int err = errno;
#endif
      snprintf(fe_ctx->error_message, sizeof(fe_ctx->error_message),
               "Send failed after %zu bytes: %d", total_sent, err);
      fe_ctx->error = H8_NETWORK_ERROR_TRANSMIT;
      return FALSE;
    }

    total_sent += sent;
  }

  return TRUE;
}

unsigned h8_fe_network_receive(void *buffer, unsigned size)
{
  if (!fe_ctx || !H8_SOCKET_VALID(fe_ctx->socket) || !buffer)
  {
    snprintf(fe_ctx->error_message, sizeof(fe_ctx->error_message),
             "Invalid receive parameters");
    fe_ctx->error = H8_NETWORK_ERROR_RECEIVE;
    return FALSE;
  }

  size_t total_received = 0;
  char *buf = (char *)buffer;

  if (size == 0)
  {
    char peek_buf[4096];
#ifdef _WIN32
    int available = recv(fe_ctx->socket->handle, peek_buf, sizeof(peek_buf), MSG_PEEK);
#else
    ssize_t available = recv(fe_ctx->socket->handle, peek_buf, sizeof(peek_buf), MSG_PEEK);
#endif
    if (available <= 0)
    {
#ifdef _WIN32
      int err = WSAGetLastError();
#else
      int err = errno;
#endif
      snprintf(fe_ctx->error_message, sizeof(fe_ctx->error_message),
               "Receive peek failed: %d", err);
      fe_ctx->error = H8_NETWORK_ERROR_RECEIVE;
      return FALSE;
    }

    size = (unsigned)available;
  }

  while (total_received < size)
  {
#ifdef _WIN32
    int received = recv(fe_ctx->socket->handle, buf + total_received, (int)(size - total_received), 0);
#else
    ssize_t received = recv(fe_ctx->socket->handle, buf + total_received, size - total_received, 0);
#endif
    if (received <= 0)
    {
#ifdef _WIN32
      int err = WSAGetLastError();
#else
      int err = errno;
#endif
      if (received == 0)
      {
        snprintf(fe_ctx->error_message, sizeof(fe_ctx->error_message),
                 "Connection closed by peer after %zu bytes", total_received);
      }
      else
      {
        snprintf(fe_ctx->error_message, sizeof(fe_ctx->error_message),
                 "Receive failed after %zu bytes: %d", total_received, err);
      }
      fe_ctx->error = H8_NETWORK_ERROR_RECEIVE;
      return FALSE;
    }

    total_received += received;
  }

  return TRUE;
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
