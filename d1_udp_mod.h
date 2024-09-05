/* ======================================================================
 * YOU CAN MODIFY THIS FILE.
 * ====================================================================== */

#ifndef D1_UDP_MOD_H
#define D1_UDP_MOD_H

#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>


/* This structure keeps all information about this client's association
 * with the server in one place.
 * It is expected that d1_create_client() allocates such a D1Peer object
 * dynamically, and that d1_delete() frees it.
 */
struct D1Peer
{
    int32_t            socket;      /* the peer's UDP socket */
    struct sockaddr_in addr;        /* addr of my peer, initialized to zero */
    int                next_seqno;  /* either 0 or 1, initialized to zero */
};

typedef struct D1Peer D1Peer;

/* Egendefinerte hjelpemetoder som skal inkluderes og implementeres i d1_udp.c*/

/*
sjekker at det står riktig størrelse i headeren
*/
int correct_size_in_header(uint32_t header_size, uint32_t actual_size );

/*
returnerer 1 hvis flagget som sier at det er en data packet er satt
*/
int flag_data_packet(uint16_t flags);

/*
returnerer 1 hvis flagget som sier at det er en ACK packet er satt
*/
int flag_ACK(uint16_t flags);

/*
returnerer sekvensnummeret på datapakken
*/
int flag_sequence_number(uint16_t flags);

/*
returnerer sekensnummeret på ack-pakken
*/
int flag_ack_number(uint16_t flags);

/*
genererer checksum for en pakke. Fungerer også for å sjekke
at checksummen er som forventet.
*/
uint16_t generate_checksum_for_packet(char* packet, size_t size);




#endif /* D1_UDP_MOD_H */

