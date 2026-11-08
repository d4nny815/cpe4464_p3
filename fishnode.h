#ifndef FISHNODE_H
#define FISHNODE_H

#include <stdint.h>
#include "fish.h"

// L3 Structs

#define FISH_BROADCAST_ADDR 0xFFFFFFFF

typedef struct __attribute__((packed)) l3Header_t {
    uint8_t ttl;
    uint8_t protocol;
    uint32_t packet_id;
    fnaddr_t src_l3_addr;
    fnaddr_t dst_l3_addr;
} l3Header_t;

typedef enum {
    L3_PROTO_PACKET = 1,
    L3_PROTO_ECHO = 2,
    L3_PROTO_NAMING = 3,
    L3_PROTO_LINK_STATE = 4,
    L3_PROTO_TRANSPORT = 5,
    L3_PROTO_FCMP = 6,
} L3Protocol_t;

// L4 Structs

typedef enum {
    L4_PROTO_ECHO_REQ = 1,
    L4_PROTO_ECHO_RESP = 2,
    L4_PROTO_REQ_SPOOF = 3,
} L4Protocol_t;


#endif /* FISHNODE_H */
