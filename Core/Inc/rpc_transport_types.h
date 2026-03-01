#ifndef RPC_TRANSPORT_TYPES_H
#define RPC_TRANSPORT_TYPES_H

#include <stdint.h>

#define RX_BUFFER_SIZE 2048

typedef struct {
    char data[RX_BUFFER_SIZE];
    uint16_t length;
} rpc_rxbuffer_t;

#endif