#ifndef FISHNODE_H
#define FISHNODE_H

#include <stdint.h>
#include "fish.h"

// L2 Structs

typedef enum {
    L2_PROTO_L3 = 1,
} L2Protocol_t;

// L3 Structs

#define FISH_BROADCAST_ADDR (0xFFFFFFFF)

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

// Forwarding Table Structs

typedef struct fwEntry_t {
    fnaddr_t dst;          
    fnaddr_t mask;         
    int prefix_len;        // 32 for /32 
    fnaddr_t next_hop;        
    int metric;
    char type;
    void *user_data;
    struct fwEntry_t *next;
} fwEntry_t;

// L4 Structs

typedef enum {
    L4_PROTO_ECHO_REQ = 1,
    L4_PROTO_ECHO_RESP = 2,
    L4_PROTO_REQ_SPOOF = 3,
} L4Protocol_t;

typedef enum {
    FCMP_TTL_EXCEEDED = 1,
    FCMP_NET_UNREACHABLE = 2,
    FCMP_HOST_UNREACHABLE = 3,
} FCMPError_t;


#endif /* FISHNODE_H */
