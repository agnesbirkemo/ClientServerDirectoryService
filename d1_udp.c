/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h> //lagt til av meg for å bruke select

#include "d1_udp.h"




/** Create a UDP socket that is not bound to any port yet. This is used as a
 *  client port.
 *  Returns the pointer to a structure on the heap in case of success or NULL
 *  in case of failure.
 * 
 * allokerer dynamisk minne til en D1Peer, d1_delete frigjør dette minnet
 */
D1Peer* d1_create_client()
{
    int sock;
    struct sockaddr_in addr;
    D1Peer *peer = calloc(1, sizeof(D1Peer));

    sock = socket(AF_INET, SOCK_DGRAM, 0); //lager socket for klienten
    if (sock == -1){ //socket() returnerer -1 hvis noe gikk galt
        perror("socket");
        free(peer);
        return NULL;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in)); //setter adressen til 0 

    //initialiserer verdiene til peer
    peer->socket = sock;
    peer->addr = addr;
    peer->next_seqno = 0; //settes til 0 som spesifisert i d1_udp_mod.h

    printf("created client with socket: %i and addr + seqnr initialized to zero.\n", sock);
    return peer;
}

/** Take a pointer to a D1Peer struct. If it is non-NULL, close the socket
 *  and free the memory of the server struct.
 *  The return value is always NULL.
 */
D1Peer* d1_delete( D1Peer* peer )
{
    if (peer != NULL){ //sjekker om peer er NULL
        printf("deleting peer\n");
        close(peer->socket); //lukker socket før frigjøring av minnet til peer
        free(peer);
    }
    return NULL;
}

/** Determine the socket address structure that belongs to the server's name
 *  and port. The server's name may be given as a hostname, or it may be given
 *  in dotted decimal format.
 *  In this excersize, you do not support IPv6. There are no extra points for
 *  supporting IPv6.
 *  Returns 0 in case of error and 1 in case of success.
 */
int d1_get_peer_info( struct D1Peer* peer, const char* peername, uint16_t server_port )
{
    int wc;
    struct addrinfo hints;
    struct addrinfo *server_addr; //getaddrinfo allokerer dynamisk minne til denne, 
    

    //her spesifiseres det at adressen skal være på IPv4 og vi skal bruke UDP
    printf("Initializing hints to IPv4 and UDP\n");
    memset(&hints, 0, sizeof(struct addrinfo)); //initialiserer alle attributter til 0
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_DGRAM; //UDP
    hints.ai_flags = 0;              
    hints.ai_protocol = 0;           
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    char port_str[5] = {0}; // Portnummer
    sprintf(port_str, "%d", server_port); //konvertere til string

    //bruker getaddrinfo som returnerer en eller flere addrinfo structs
    printf("getaddrinfo\n");
    wc = getaddrinfo(peername, port_str, &hints, &server_addr);
    if (wc != 0){
        //no gikk galt
        perror("getaddrinfo");
        freeaddrinfo(server_addr);
        d1_delete(peer);
        
        return 0;
    }
    printf("getaddrinfo ok.\n");

    //setter peer sin addr til å være første struct addrinfo som returneres av getaddrinfo()
    memcpy(&peer->addr, server_addr->ai_addr, sizeof(struct sockaddr_in)); //TODO: evt sjekk for om memcpy fungerte?
    //gjør om portnummer til network byteorder
    //peer->addr.sin_port = htons(server_port);
    printf("portnumber = %u\n",ntohs(peer->addr.sin_port));
    freeaddrinfo(server_addr); //frigjør hele listen man får av getaddrinfo
    printf("d1_get_peer_info() done.\n");
    return 1;
}



//egendefinert hjelpemetode
int correct_size_in_header(uint32_t header_size, uint32_t actual_size ){
    if (header_size == actual_size){
        return 1;
    }
    return 0;
}


//1 hvis satt, 0 hvis ikke
int flag_data_packet(uint16_t flags){
    flags &= FLAG_DATA;

    if (flags == 32768){
        return 1;
    }else{
        return 0;
    }
}

//rreturnerer 1 hvis flagget som sier at det er en ACK packet er satt
int flag_ACK(uint16_t flags){
    flags &= FLAG_ACK;

    if (flags == 256){
        return 1;
    }else{
        return 0;
    }
}

//returnerer sekvensnummeret
int flag_sequence_number(uint16_t flags) {
    return (flags & SEQNO) != 0;
}

//returnerer sekvensnummeret på ack-pakke
int flag_ack_number(uint16_t flags){
    flags &= ACKNO;
    return flags;
}


uint16_t generate_checksum_for_packet(char* packet, size_t size){
    
    uint16_t checksum = 0;

    char *local_packet;

    size_t tmp_size = size;

    //legge til null-byte hvis data_size er oddetall
    if (size % 2 != 0){
        printf("Data size is an odd number, adding a null byte.\n");
        local_packet = calloc(1, size+1);
        memcpy(local_packet, packet, size);
        local_packet[size] = '\0';
        tmp_size += 1;

    }else{
        local_packet = calloc(1, size);
        memcpy(local_packet, packet, size);
    }


    printf("Generating checksum...\n");
    // Iteraerer over pakken i 16-bit chunks
    for (size_t i = 0; i < tmp_size; i += 2) {

        // Konvertere bytes til 2 bytes-verdier
        uint16_t next_16_bits = ((u_int8_t)local_packet[i] << 8) | ((u_int8_t)local_packet[i + 1]);

        // Utfoerer XOR paa 16-bit-verdien
        checksum ^= next_16_bits;
    }
    printf("Checksum result: %u\n", checksum);  

    free(local_packet);

    return checksum;
}


/** Call this to wait for a single packet from the peer. The function checks if the
 *  size indicated in the header is correct and if the checksum is correct.
 *  If size or checksum are incorrect, an ACK with the opposite value is sent (this should
 *  trigger the sender to retransmit).
 *  In other cases, an error message is printed and a negative number is returned to the
 *  calling function.
 *  Returns the number of bytes received in case of success, and a negative value in case
 *  of error.
 */

int d1_recv_data( struct D1Peer* peer, char* buffer, size_t sz )
{
    printf("\n\n----In d1_recv_data()----\n");

    int rc;
    char* data = NULL;

    rc = recv(peer->socket, buffer, sz, 0);
    if (rc == -1){
        perror("recv");
        d1_delete(peer);
        return -1; //returnerer negativt tall ved feil
    }else if (!rc){
        fprintf(stderr, "Klarte ikke å lese noe");
        d1_delete(peer);
        return -1;
    }

    buffer[rc] = '\0';

    printf("Recieved a packet of size %u.\n", rc);

    int correct_checksum;

    //sjekke at checksum stemmer:
    if (generate_checksum_for_packet(buffer, rc) == 0){
        printf("Correct checksum!\n");
        correct_checksum = 1;
    }else{
        printf("Incorrect checksum! Sending wrong ACK.\n");
        correct_checksum = 0;
    }

    //header består av 64 bits - extract header
    D1Header *header = (D1Header*) buffer;
    header->flags = ntohs(header->flags);           //16 bit - short
    header->checksum = ntohs(header->checksum);     //16 bit - short
    header->size = ntohl(header->size);             //32 bit - long

    
    //sjekke at det faktisk er en datapakke:
    if (flag_data_packet(header->flags) == 1){
        data = buffer + sizeof(D1Header); //data starter etter headeren, altsa etter 8 bytes
        printf("This is a datapacket.\n");
    }//hvis ikke fortsetter data å være NULL
    else{
        printf("The bit representing a data packet is not set.\n");
    }

    //sjekker om det er en disconnect-packet: de skal ikke ACK-es
    char disconnect = 0;
    if (strcmp(data, "disconnect") == 0){
        disconnect = 1;
    }
    
    printf("Checking size..\n");
    //sjekker at størrelsen stemmer:
    int correct_size = correct_size_in_header(header->size, rc);

    if (!correct_size && !disconnect){
        d1_send_ack(peer, !flag_sequence_number(header->flags));
        printf("Wrong size in header! Expected %u, got %u\n", rc, header->size);
    }else{
        printf("Size is correct! Expected %u, got %u\n", rc, header->size);
    }

    if (correct_checksum && !disconnect){
        //Sende en ack med samme seqnr ved vellykket mottak
       d1_send_ack(peer, flag_sequence_number(header->flags));
    }else if (!correct_checksum && !disconnect){
        //Sende en ack med motsatt seqnr ved feil mottak
        d1_send_ack(peer, !flag_sequence_number(header->flags));
    }
    
    //bufferet skal kunn inneholde payload:
    memcpy(buffer, data, rc-8);

    //returnerer størrelsen på payloaden
    return rc-8;
}

/** Called by d1_send_data when it has successfully sent its packet. This functions waits for
 *  an ACK from the peer.
 *  If the sequence number of the ACK matches the next_seqno value in the D1Peer stucture,
 *  this function changes the next_seqno in the D1Peer data structure
 *  (0->1 or 1->0) and returns to the caller.
 *  If the sequence number does not match, d1_send_data followed by d1_wait_ack is called
 *  again.
 *  Returns a positive value in case of success, and a negative value in case of error.
 *
 *  This function is only meant to be called by d1_send_data. You don't have to implement it.
 */

int d1_wait_ack( D1Peer* peer, char* buffer, size_t sz )
{
    /* This is meant as a helper function for d1_send_data.
     * When D1 data has send a packet, this one should wait for the suitable ACK.
     * If the arriving ACK is wrong, it resends the packet and waits again.
     *
     * Implementation is optional.
     */

    int rc;
    fd_set fds;
    struct timeval timeout;

    FD_ZERO(&fds);
    FD_SET(peer->socket, &fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    //kun read fd i select
    rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    if(rc == -1){
        perror("select");
        d1_delete(peer);
    }else if (rc == 0){
        printf("Timeout in select - waitet more than 1 second\n");
    }

    if (FD_ISSET(peer->socket, &fds)){
    //hvis den ikke faar ack innen 1 sek, resend
        rc = recv(peer->socket, buffer, sz, 0);
        if (rc == -1){
            perror("recv");
            d1_delete(peer);
            return -1; //returnerer negativt tall ved feil
        }else if (!rc){
            fprintf(stderr, "Klarte ikke å lese noe\n");
            d1_delete(peer);
            return -1; 
        }
        printf("Recieved an ACK!\n");
    } else{
        printf("No packet was recieved in the waiting time\n");
        return -1;
    }

    //hvis den kommer hit har den fått en pakke innen timeout 
    D1Header *header = (D1Header*) buffer;
    header->flags = ntohs(header->flags);           //16 bit - short
    header->checksum = ntohs(header->checksum);     //16 bit - short
    header->size = ntohl(header->size);             //32 bit - long

    //sjekke at det faktisk er en ACK-pakke:
    if (flag_ACK(header->flags) == 1){
        printf("This is an ACK packet.\n");
    }//hvis ikke fortsetter data å være NULL
    else{
        printf("The bit representing an ACK packet is not set.\n");
        return -1;
    }

    if (flag_ack_number(header->flags) == peer->next_seqno){
        printf("d1_wait_ack() - Correct sequence number: %u\n", flag_ack_number(header->flags));
        peer->next_seqno = !(peer->next_seqno);
        return 1;
    }
    printf("d1_wait_ack(): Wrong sequence number! was expecting %i, got %i\n",peer->next_seqno, flag_ack_number(header->flags));
    return -1;
}



/** If the buffer does not exceed the packet size, add the D1 header and send
 *  it to the peer.
 *  Returns the number of bytes sent in case of success, and a negative value in case
 *  of error.
 */


int d1_send_data( D1Peer* peer, char* buffer, size_t sz )
{
    int wc, rc;
    D1Header header;
    memset(&header, 0, sizeof(D1Header));
    
    printf("\n\nAt d1_send_data() - trying to send packet containing: %s\n", buffer);

    //sjekke at stoerrelsen på bufferet + header ikke overstiger grensen paa 1024 bytes
    if (sz + sizeof(D1Header) > 1024){
        printf("Could not send packet! Buffer size + header size is more than 1024.");
        d1_delete(peer);
        return -1;
    }

    //lage header
    printf("Making header..\n");
    uint16_t flags = 0;
    char seq_number = peer->next_seqno;

    flags |= FLAG_DATA;

    if (seq_number ){ //settes hvis next seqnr er 1 
        printf("Seqence number set to 1.\n");
        flags |= SEQNO;
    }

    uint32_t size = sz + sizeof(D1Header);

    //konvertere fra host byte order til network byte order
    header.flags = htons(flags);
    header.checksum = 0;
    header.size = htonl(size);

    //samle pakken til én
    char* packet = (char*) calloc(1, size); //stoerrelsen på buffer + header
    if (packet == NULL) {
        perror("calloc");
        d1_delete(peer);
        return -1; 
    }

    // kopiere header til packet
    memcpy(packet, &header, sizeof(D1Header));

    // kopiere data til packet
    memcpy(packet + sizeof(D1Header), buffer, sz);

    D1Header *h = (D1Header*) packet;
    h->checksum = htons(generate_checksum_for_packet(packet, size));

    //sende pakke
    wc = sendto(
        peer->socket,
        packet,
        size, 0,
        (struct sockaddr *)&peer->addr,
        sizeof(struct sockaddr_in));
    if(wc == -1){
        perror("sendto");
        d1_delete(peer);
        free(packet);
        return -1;
    }
    printf("Packet containing '%s' was sent\n", buffer);

    //disconnect packets skal ikke motta ack
    if (strcmp(buffer, "disconnect") == 0)
    {
        free(packet);
        return wc;
    }
    

    char buf[1024];

    //kalle d1_wait_ack - hvis retur er -1 --> resende pakke og kalle d1_wait_ack igjen
    rc = d1_wait_ack(peer, buf, 1000);

    if (rc == -1){
        if (d1_send_data(peer, buffer, sz ) == -1){
                free(packet);
                printf("failed to send packet\n");
                return -1;
        }
    }
    
    free(packet);
    return wc;
}

/** Send an ACK for the given sequence number.
 */

void d1_send_ack( struct D1Peer* peer, int seqno )
{

    printf("\n\n----In d1_send_ack(): trying to send ack with ack number %i\n\n", seqno);
    D1Header header;
    int wc;

     //lage header
    uint16_t flags = 0;

    flags |= FLAG_ACK; //setter biten som representerer at det er en ACK-pakke

    if (seqno){ //settes ikke hvis next seqnr er 0
        flags |= ACKNO;
    }

    uint32_t size = sizeof(D1Header);

    //konvertere fra host byte order til network byte order
    header.flags = htons(flags);
    header.checksum = 0;
    header.size = htonl(size);

    header.checksum = generate_checksum_for_packet((char*)&header, sizeof(D1Header));

    //sende pakke
    wc = sendto(
        peer->socket,
        &header,
        size, 0,
        (struct sockaddr *)&peer->addr,
        sizeof(struct sockaddr_in));
    if(wc == -1){
        perror(" d1_send_ack() sendto");
        d1_delete(peer);
    }
   
}