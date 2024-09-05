/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "d2_lookup.h"


/* Create the information that is required to use the server with the given
 * name and port. Use D1 functions for this. Store the relevant information
 * in the D2Client structure that is allocated on the heap.
 * Returns NULL in case of failure.
 */

D2Client* d2_client_create( const char* server_name, uint16_t server_port )
{
    int rc;
    D2Client *client = calloc(1, sizeof(D2Client));

    //D2Client inneholder en D1Peer
    //lager peer med d1_create_client() og legger inn server- og port-info med d1_get_peer_info

    D1Peer *peer = d1_create_client();
    rc = d1_get_peer_info(peer, server_name, server_port);

    if( rc == 0 ){
        printf( "Failed to get peer info for server with name %s and port %d\n", server_name, server_port );
        d1_delete(peer);
        free(client);
        return NULL;
    }

    //Setter D2Client client sin peer til å peke på den nye peer-en

    client->peer = peer;

    return client;
}


D2Client* d2_client_delete( D2Client* client )
{
    //maa frigjøre D1Peer inni client, deretter D2Client
    //bruker d1_delete()
    d1_delete(client->peer);
    free(client);

    return NULL;
}


int d2_send_request( D2Client* client, uint32_t id )
{
    printf("\n\nIn d2_send_request - trying to send packet request with id %u\n", id);
    int rc;

    //lager pakke og konverterer til network byte order
    PacketRequest *packet = calloc(1, sizeof(PacketRequest));
    packet->type = htons(TYPE_REQUEST);
    packet->id = htonl(id);

    rc = d1_send_data(client->peer, (char *)packet, sizeof(PacketRequest));

    if (rc < 0){
        printf("d2_send_request(): failed to send packet with id %u\n", id);
        free(packet);
        return 0; //return failure
    }

    printf("d2_send_request - request packet containing id %u was sent\n\n", id);

    free(packet);
    return 1; //return success
}

/* Wait for a PacketResponseSize packet from the server.
 * Returns the number of NetNodes that will follow in several PacketReponse
 * packet in case of success. Note that this is _not_ the number of
 * PacketResponse packets that will follow because of PacketReponse can
 * contain several NetNodes.
 *
 * Returns a value <= 0 in case of failure.
 */

int d2_recv_response_size( D2Client* client )
{
    int rc;
    char buffer[1024]; //maks stoerrelse mulig
    memset(buffer, 0, 1024); //initialiserer hele bufferet til 0
    
    rc = d1_recv_data(client->peer, buffer, 1024);
    if (rc < 0){
        printf("d2_recv_response_size(): An error occured when calling d1_recv_data()\n");
        return 0;
    }
    
    PacketHeader* packet_header = (PacketHeader*)buffer;

    if (ntohs(packet_header->type) != TYPE_RESPONSE_SIZE){
        printf("d2_recv_response_size() revieved a packet that is not of type TYPE_RESPONSE_SIZE!\n");
        return 0;
    }

    PacketResponseSize* packet_response_size = (PacketResponseSize*)buffer;

    printf("Recieved a PacketResponseType with size %u\n", ntohs(packet_response_size->size));

    return ntohs(packet_response_size->size);
    
}



int d2_recv_response( D2Client* client, char* buffer, size_t sz )
{
    int rc;
    rc = d1_recv_data(client->peer, buffer, sz);
    if (rc < 0){
        printf("d2_recv_response(): Something went wrong when calling d1_recv_data");
        return 0;
    }
    
    //endrer ikke feltene i headet til host byteorder da dette gjøres i test-filen
    return rc;
}



LocalTreeStore* d2_alloc_local_tree( int num_nodes )
{
    //allokerer plass til LocalTreestore med størrelsen på struct LocalTreeStore
    // + num_nodes*32 fordi én NetNode er 32 bytes.
    LocalTreeStore* tree_store = calloc(1, sizeof(LocalTreeStore)+num_nodes*32); 

    if (tree_store == NULL){
        printf("d2_alloc_local_tree(): Unable to dynamically allocate memory for LocalTreeStore.\n");
        return NULL;
    }
    
    printf("d2_alloc_local_tree(): Allocated dynamic memory for LocalTreeStore.\n");
    tree_store->number_of_nodes = 0; //initialiserer til 0, oekes hver gang ny node legges til.
    tree_store->array_length = num_nodes; //antall NetNodes det er plass til.
    return tree_store;
}

void  d2_free_local_tree( LocalTreeStore* nodes )
{
    free(nodes);
}

//hjelpemetode for å lage NetNodes av bufferet og legge det inn i treet:
int make_netnodes(LocalTreeStore* nodes_out, int node_idx, char* buffer, int buflen ){

    char* index = buffer;

    int num_nodes = 0; 
    
    char* end_buffer = buffer + buflen;

    while (index < end_buffer) {


        if (nodes_out->array_length <= node_idx + num_nodes){
            printf("Index out of bounds - not able to add node to tree!\n");
            return -1;
        }

        NetNode *node = (NetNode*)index;

        node->id = ntohl(node->id);
        node->value = ntohl(node->value);
        node->num_children = ntohl(node->num_children);

        /*
        printf("\nnode id:              %u\n", node->id);
        printf("node value:             %u\n", node->value);
        printf("node num children:      %u\n", node->num_children);
        */
        
        for (uint32_t i = 0; i < node->num_children; i++){
            node->child_id[i] = ntohl(node->child_id[i]);
        }
        
        size_t node_size = sizeof(NetNode) - (5 - node->num_children) * sizeof(uint32_t);
        index += node_size;

        OneNetNode* oneNetNode = (OneNetNode*)node;

        

        nodes_out->nodes[node_idx + num_nodes] = *oneNetNode;

        
        for (uint32_t i = node->num_children; i < 5; i++) {
            NetNode* node = (NetNode*) &nodes_out->nodes[node_idx + num_nodes];
            node->child_id[i] = 0;
        }
        
        num_nodes += 1;
    }

    return num_nodes;
}



//buffer inneholder kun payload - kun netnodes ikke header
//buflen er størrelsen på payloaden

int d2_add_to_local_tree( LocalTreeStore* nodes_out, int node_idx, char* buffer, int buflen )
{
    
    int num_nodes = make_netnodes(nodes_out, node_idx, buffer, buflen);

    if (num_nodes == -1){
        return -1;
    }

    nodes_out->number_of_nodes += num_nodes;
    return node_idx + num_nodes;
}

//hjelpefunksjon ut i fra dypden på treet
void print_depth(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("--");
    }
}


void print_nodes_rec(LocalTreeStore* tree, uint32_t index, int depth){

    OneNetNode node = tree->nodes[index];

    NetNode* netNode = (NetNode*)&node;

    print_depth(depth);
    printf("id %u value %u children %u\n", 
           netNode->id, 
           netNode->value, 
           netNode->num_children);

    if (netNode->num_children == 0){
        return;
    }
    // Printe barn rekursivt
    for (uint32_t i = 0; i < netNode->num_children; i++) {
        uint32_t child_id = netNode->child_id[i];
        print_nodes_rec(tree, child_id ,depth + 1);
    }
}

//for qsort()
int compare (const void * a, const void * b) {

    return ((NetNode*)a)->id - ((NetNode*)b)->id; 
}

void d2_print_tree( LocalTreeStore* nodes_out )
{
    //sorterer arrayet med noder - typecaster til NetNode s.a. compare fungerer
    NetNode* nodes = (NetNode*)nodes_out->nodes;

    qsort(nodes, nodes_out->number_of_nodes, 32, compare);
    
    print_nodes_rec(nodes_out, 0, 0);
   
}

