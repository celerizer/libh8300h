#ifndef H8_FRONTEND_H
#define H8_FRONTEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

typedef enum
{
  H8_NETWORK_ERROR_NONE = 0,

  H8_NETWORK_ERROR_INIT,
  H8_NETWORK_ERROR_SOCKET,
  H8_NETWORK_ERROR_CONNECT,
  H8_NETWORK_ERROR_BIND,
  H8_NETWORK_ERROR_LISTEN,
  H8_NETWORK_ERROR_TRANSMIT,
  H8_NETWORK_ERROR_RECEIVE,

  H8_NETWORK_ERROR_SIZE
} h8_network_error;

typedef struct h8_socket_s *h8_socket_t;

typedef struct
{
  h8_socket_t socket;
  char ip[32];
  unsigned port;
  h8_bool server;
  h8_network_error error;
  char error_message[256];
} h8_network_ctx_t;

/**
 * Requests the frontend to initialize the network for infrared communication.
 * The frontend will provide its own settings for IP and port. If the frontend
 * does not support networking, this function should return FALSE.
 */
h8_bool h8_fe_network_init(h8_network_ctx_t *ctx);

/**
 * Requests the frontend to send data to the connected system. Must not block
 * for long, and must not split the data if it cannot all be sent.
 * @return FALSE if nothing is connected or the connection failed
 */
h8_bool h8_fe_network_transmit(const void *data, unsigned size);

/**
 * Requests the frontend to receive data from the network without blocking.
 * Called frequently during emulation.
 * @param size The maximum number of bytes to receive
 * @return Number of bytes received, or 0 if none are waiting
 */
unsigned h8_fe_network_receive(void *data, unsigned size);

#ifdef __cplusplus
}
#endif

#endif
