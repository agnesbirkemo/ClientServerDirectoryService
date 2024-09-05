/* ======================================================================
 * YOU CAN MODIFY THIS FILE.
 * ====================================================================== */

#ifndef D2_LOOKUP_MOD_H
#define D2_LOOKUP_MOD_H

#include "d1_udp.h"

struct D2Client
{
    D1Peer* peer;
};

typedef struct D2Client D2Client;


//for aa unngaa gjensidig include mellom d2_lookup.h og d2_lookup_mod.h
//lager jeg en struct med samme stoerrelse som netnode, maa typecaste 
//i d2_lookup
struct OneNetNode {
    uint8_t node[32]; 
};
typedef struct OneNetNode OneNetNode;


struct LocalTreeStore
{
    int number_of_nodes;
    int array_length;
    OneNetNode nodes[]; 
};

typedef struct LocalTreeStore LocalTreeStore;

/*
* Egendefinerte hjelpemetoder som skal inkluderes i d2_lookup.c
*/

/*
make_netnodes går over bufferet, lager NetNodes og fyller de inn med informasjonen 
fra bufferet. Deretter legger den NetNodene til i treet som sendes inn. 
Returverdien til make_netnode() er antall noder som ble lagt til.*/
int make_netnodes(LocalTreeStore* nodes_out, int node_idx, char* buffer, int buflen );

/*
 rekursiv print-metode som tar inn treet, 
 indeksen/id-en på noden og dybden på noden. Først printer den 
 infoen om noden som sendes inn med "--" * dybde antall ganger 
 før informasjonen printes. Deretter gjør den rekursive kall på 
 alle nodens barn (hvis den har barn) der dybden økes med 1 for 
 hver "generasjon"/hvert lag i treet.
*/
void print_nodes_rec(LocalTreeStore* tree, uint32_t index, int depth);

/*For å bruke qsort lagde jeg sammenligningsmetoden compare som 
 sammenligner id-ene til to og to noder. */
int compare (const void * a, const void * b);




#endif /* D2_LOOKUP_MOD_H */

