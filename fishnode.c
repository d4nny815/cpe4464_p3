#include "fishnode.h"
#include "smartalloc.h"
#include "fish.h"
#include "hashtable.h"
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#define DEBUG
#define L3_IMPL

static int noprompt = 0;
static hashTable_t* broadcast_table = NULL;

void sigint_handler(int sig) {
   if (SIGINT == sig)
       fish_main_exit();
}

static int print_route(void *callback_data __attribute__((unused)),
        fnaddr_t dest, int prefix_len __attribute((unused)),
        fnaddr_t net_hop __attribute((unused)),
        int metric __attribute((unused)),
        void *entry_data __attribute__((unused))) {
   printf("%s\n", fn_ntoa(dest));
   return 0;
}

static void keyboard_callback(char *line) {
   if (0 == strcasecmp("show neighbors", line))
      fish_print_neighbor_table();
   else if (0 == strcasecmp("show arp", line))
      fish_print_arp_table();
   else if (0 == strcasecmp("show route", line)) {
      fish_print_forwarding_table();
      fish_fwd.iterate_entries(&print_route, NULL, FISH_FWD_TYPE_BROADCAST);
   }
   else if (0 == strcasecmp("show dv", line))
      fish_print_dv_state();
   else if (0 == strcasecmp("quit", line) || 0 == strcasecmp("exit", line))
      fish_main_exit();
   else if (0 == strcasecmp("show topo", line))
      fish_print_lsa_topo();
   else if (0 == strcasecmp("help", line) || 0 == strcasecmp("?", line)) {
      printf("Available commands are:\n"
             "    exit                         Quit the fishnode\n"
             "    help                         Display this message\n"
             "    quit                         Quit the fishnode\n"
             "    show arp                     Display the ARP table\n"
             "    show dv                      Display the dv routing state\n"
             "    show neighbors               Display the neighbor table\n"
             "    show route                   Display the forwarding table\n"
             "    show topo                    Display the link-state routing\n"
             "                                 algorithm's view of the network\n"
             "                                 topology\n"
             "    ?                            Display this message\n"
            );
   }
   else if (line[0] != 0)
      printf("Type 'help' or '?' for a list of available commands.  "
             "Unknown command: %s\n", line);

   if (!noprompt)
      printf("> ");

   fflush(stdout);
}

#ifdef L3_IMPL


int my_fishnode_l3_receive(void *l3frame, int len, uint8_t protocol) {
    /*
    Decapsulates the frame and forwards it or passes it the L4 as needed. 
    

    l3frame A pointer to the received L3 frame. The only part of the frame that can be modified is the TTL header field. The caller will free the memory for the frame as necessary.
    len The length of the layer 3 frame, in host byte order
    protocol The protocol of the L3 frame

    Returns
        The return value is ignored

    This function is responsible for correctly directing packets to the higher network layers or to the forwarding engine. This requires following general steps:

    If the L3 destination is the node's L3 address, remove the L3 header and pass the frame to the L4 code.
    If the L3 destination is the broadcast address AND the frame was received by this node previously (use the combination of src L3 address and packet ID to determine this), drop the frame without sending an FCMP message.
    Otherwise, if the L3 destination is the broadcast address, the frame must be both passed up the network stack and forwarded back out over the fishnet with a decremented TTL.
    If the L3 addresses isn't the broadcast address and the L3 address isn't the node's address, the TTL is decremented and the frame is forwarded back over the fishnet.
    */

    fish_debugframe(FISH_DEBUG_USER1, "[L3 RECV] Received L3 frame",
                    l3frame, 3, len, protocol, 0);

    l3Header_t *header = (l3Header_t *)l3frame;

    void* l4frame = (void*)((uint8_t*)l3frame + sizeof(l3Header_t)); // TODO: placeholder
    int l4len = len - sizeof(l3Header_t);
    uint8_t l4proto = header->protocol;
    fnaddr_t l4src = header->src_l3_addr;

    // Case 1: Packet is meant for this node
    if (header->dst_l3_addr == fish_getaddress()) {
        if (header->ttl > MAX_TTL) {
            // illegal TTL, drop packet
            fish_debug(FISH_DEBUG_USER1, "[L3 RECV] Illegal TTL, dropping packet\n");
            return 0;
        }

        fish_debug(FISH_DEBUG_USER1, "[L3 RECV] L3 Packet meant for me\n");

        // pass it to l4
        fish_l4.fish_l4_receive(l4frame, l4len, l4proto, l4src);
    }

    // Case 2: Packet is meant for broadcast
    else if (header->dst_l3_addr == FISH_BROADCAST_ADDR) { 
        fish_debug(FISH_DEBUG_USER1, "[L3 RECV] L3 Packet meant for broadcast\n");
        if (!broadcast_table) {
            broadcast_table = hashTable_create();
        }

        // get broadcast table key
        char* key_str = (char*)calloc(1, 22);
        snprintf(key_str, 22, "%u:%u", header->src_l3_addr, header->packet_id);
        fish_debug(FISH_DEBUG_USER1, "[L3 RECV] Inserting into broadcast table: %s\n", key_str);

        if (hashTable_exists(broadcast_table, key_str)) {
            fish_debug(FISH_DEBUG_USER1, "[L3 RECV] Packet already seen, dropping\n");
            free(key_str);
        } else {
            hashTable_insert(broadcast_table, key_str, NULL);
            header->ttl--;
            fish_l3.fish_l3_forward(l3frame, len);
            fish_l4.fish_l4_receive(l4frame, l4len, l4proto, l4src);
        }
    }

    // if l3 dest != broadcast and l3 dest != node l3
    //   dec TTL in og header
    //   forward it back out over the fishnet
    // Case 3: Packet is meant for someone else
    else if ((header->dst_l3_addr != fish_getaddress()) && (header->dst_l3_addr != FISH_BROADCAST_ADDR)) {
        fish_debug(FISH_DEBUG_USER1, "[L3 RECV] L3 Packet meant for someone else\n");
        
        if (header->ttl == 0) {
            // if TTL reaches zero, send an FCMP message
            // TODO: send FCMP message
            fish_debug(FISH_DEBUG_USER1, "[L3 RECV] TTL reached zero, sending FCMP message\n");
            return 0;
        }

        header->ttl--;
        fish_l3.fish_l3_forward(l3frame, len);
    }

    return 0;
}

int my_fish_l3_send(void *l4frame, int len, fnaddr_t dst_addr,
                  uint8_t proto, uint8_t ttl) {
    /*
    Receives a new L4 frame to be sent over the network.

    l4frame A pointer to the L4 frame. The original frame memory must not be modified. The caller is responsible for freeing any memory after this function has completed.
    len The length of the L4 frame
    dst_addr The L3 address of the final destination for this frame.
    proto The fishnet protocol number of the L4 frame
    ttl The TTL for the frame, or 0 to use the maximum TTL. If the TTL is greater than the maximum TTL, set it to the maximum TTL.

    Returns
        The return value is ignored

    This function is responsible for encapsulating a L4 frame in a L3 header and then forwarding the packet through the fishnet. Forwarding is accomplished by calling the fish_l3_forward() function.
    */
    fish_debugframe(FISH_DEBUG_USER1, "[L3 SEND] Received L4 frame",
                    l4frame, 4, len, 0, proto);

    // printf("--------------------------------\n");
    // printf("[L3 SEND] Sending L3 frame to %s\n", fn_ntoa(dst_addr));
    // printf("[L3 SEND] Length: %d\n", len);
    // printf("[L3 SEND] Protocol: %d\n", proto);
    // printf("[L3 SEND] TTL: %d\n", ttl);
    // printf("[L3 SEND] L4 frame: %s\n", (char *)l4frame);

    // make l4
    //   if l4_ttl = min(ttl, MAX_TTL)
    //   if l4_ttl == 0
    //     l4_ttl = MAX_TTL
    // forward the packet

    // construct l4 header based off of proto

    uint8_t l4_ttl = ttl == 0 ? MAX_TTL : (ttl < MAX_TTL ? ttl : MAX_TTL);
    // printf("[L3 SEND] L4 TTL: %d\n", l4_ttl);

    fnaddr_t src_addr = fish_getaddress();
    uint8_t packet_id = fish_next_pktid();
    l3Header_t header = (l3Header_t){
        l4_ttl,
        proto, // ? is this l3 protocol?
        packet_id,
        src_addr,
        dst_addr,
    };

    void* l3frame = (void*)calloc(1, sizeof(l3Header_t) + len);
    memcpy(l3frame, &header, sizeof(l3Header_t));
    memcpy((uint8_t*)l3frame + sizeof(l3Header_t), l4frame, len);

    fish_debugframe(FISH_DEBUG_USER1, "[L3 SEND] Built L3 frame",
        l3frame, 3, len + sizeof(l3Header_t), 1, 0);

    fish_l3.fish_l3_forward(l3frame, sizeof(l3Header_t) + len);
    
    free(l3frame);

    return 0;
}

int my_fish_l3_forward(void *l3frame, int len) {
    /*
    Takes an already-encapsulated L3 frame, looks up the destination in the forwarding table, and passes the frame off to L2.

    l3frame A pointer to the L3 frame. Note that this is different than fish_l3_send which takes a L4 frame. The original frame memory must not be modified. The caller is responsible for freeing any memory after this function has completed.
    len The length of the L3 frame.

    Returns
        The return value is ignored

    fish_l3_forward is given a L3 encapsulated frame and is responsible for delivering it. This requires the following steps:

    If the TTL is 0 and the destination is not local, drop the packet and generate the correct FCMP error message.
    Lookup the L3 destination in the forwarding table.
    If there is no route to the destination, drop the frame and generate the correct FCMP error.
    Use fish_l2_send to send the frame to the next-hop neighbor indicated by the forwarding table.
    */

    fish_debugframe(FISH_DEBUG_USER1, "[L3 FORWARD] Forwarding L3 frame",
                    l3frame, 3, len, L2_PROTO_L3, 0);

    l3Header_t *header = (l3Header_t *)l3frame;

    // case 1: TTL expired on a packet that isn't for me
    if (header->ttl == 0 && header->dst_l3_addr != fish_getaddress()) {
        fish_debug(FISH_DEBUG_USER1, "[L3 FORWARD] TTL reached zero, sending FCMP TTL exceeded\n");
        fish_fcmp.send_fcmp_response(l3frame, len, FCMP_TTL_EXCEEDED);
        return 0;
    }

    // case 2: no route to the destination
    fnaddr_t next_hop = fish_fwd.longest_prefix_match(header->dst_l3_addr);
    if (next_hop == 0) {
        fish_debug(FISH_DEBUG_USER1, "[L3 FORWARD] No route to %s, sending FCMP network unreachable\n",
                   fn_ntoa(header->dst_l3_addr));
        fish_fcmp.send_fcmp_response(l3frame, len, FCMP_NET_UNREACHABLE);
        return 0;
    }

    // case 3: valid 
    fish_debug(FISH_DEBUG_USER1, "[L3 FORWARD] Next hop %s\n", fn_ntoa(next_hop));
    fish_l2.fish_l2_send(l3frame, next_hop, len, L2_PROTO_L3);

    return 0;
}

// // Callback to broadcast DV advertisement
// void my_timed_event(void*) {
//     return;
// }

//    // Full functionality
// void* my_add_fwtable_entry(fnaddr_t dst, int prefix_length, fnaddr_t next_hop,
//                      int metric, char type, void *user_data) {
//     return NULL;
// }

// void* my_remove_fwtable_entry(void *route_key) {
//     return NULL;
// }

// int my_update_fwtable_metric(void *route_key, int new_metric) {
//     return 0;
// }

// fnaddr_t my_longest_prefix_match(fnaddr_t addr) {
//     return 0;
// }

#endif 


int main(int argc, char **argv)
{
    struct sigaction sa;
   int arg_offset = 1;

   /* Verify and parse the command line parameters */
    if (argc != 2 && argc != 3 && argc != 4)
    {
        printf("Usage: %s [-noprompt] <fishhead address> [<fn address>]\n", argv[0]);
        return 1;
    }

   if (0 == strcasecmp(argv[arg_offset], "-noprompt")) {
      noprompt = 1;
      arg_offset++;
   }

   /* Install the signal handler */
    sa.sa_handler = sigint_handler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (-1 == sigaction(SIGINT, &sa, NULL))
    {
        perror("Couldn't set signal handler for SIGINT");
        return 2;
    }

   /* Set up debugging output */
#ifdef DEBUG
    // fish_setdebuglevel(FISH_DEBUG_INTERNAL);
    // fish_setdebuglevel(FISH_DEBUG_ALL);
    fish_setdebuglevel(FISH_DEBUG_USER1);
#else
    fish_setdebuglevel(FISH_DEBUG_NONE);
#endif
    fish_setdebugfile(stdout);

   /* Join the fishnet */
    if (argc-arg_offset == 1)
        fish_joinnetwork(argv[arg_offset]);
    else
        fish_joinnetwork_addr(argv[arg_offset], fn_aton(argv[arg_offset+1]));

   /* Install the command line parsing callback */
   fish_keybhook(keyboard_callback);
   if (!noprompt)
      printf("> ");
   fflush(stdout);

#ifdef L3_IMPL
    fish_l3.fishnode_l3_receive = &my_fishnode_l3_receive;
    fish_l3.fish_l3_send = &my_fish_l3_send;
    fish_l3.fish_l3_forward = &my_fish_l3_forward;
   
   // Set up a callback to broadcast DV advertisement
//    fish_scheduleevent(0, &my_timed_event, NULL);
   
// Full functionality
//    fish_fwd.add_fwtable_entry = &my_add_fwtable_entry;
//    fish_fwd.remove_fwtable_entry = &my_remove_fwtable_entry;
//    fish_fwd.update_fwtable_metric = &my_update_fwtable_metric;
//    fish_fwd.longest_prefix_match = &my_longest_prefix_match;
#endif

#if 1
   /* Enable the built-in neighbor protocol implementation.  This will discover
    * one-hop routes in your fishnet.  The link-state routing protocol requires
    * the neighbor protocol to be working, whereas it is redundant with DV.
    * Running them both doesn't break the fishnode, but will cause extra routing
    * overhead */
   fish_enable_neighbor_builtin( 0
         | NEIGHBOR_USE_LIBFISH_NEIGHBOR_DOWN
      );
#endif

   /* Enable the link-state routing protocol.  This requires the neighbor
    * protocol to be enabled. */
//    fish_enable_lsarouting_builtin(0);

#if 1
   /* Full-featured DV routing.  I suggest NOT using this until you have some
    * reasonable expectation that your code works.  This generates a lot of
    * routing traffic in fishnet */

   fish_enable_dvrouting_builtin( 0
         | DVROUTING_WITHDRAW_ROUTES
         | DVROUTING_TRIGGERED_UPDATES
         | RVROUTING_USE_LIBFISH_NEIGHBOR_DOWN
         | DVROUTING_SPLIT_HOR_POISON_REV
         | DVROUTING_KEEP_ROUTE_HISTORY
    );
#endif

    /* Execute the libfish event loop */
    fish_main();

    /* Clean up and exit */
    if (!noprompt)
      printf("\n");
    fish_keybhook(NULL);

    printf("Fishnode exiting cleanly.\n");

    fishnet_cleanup();

    // Cleanup your data structures here
    if (broadcast_table) {
        hashTable_destroy(broadcast_table);
    }
    return 0;
}