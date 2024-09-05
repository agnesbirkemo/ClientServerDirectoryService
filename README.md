

Jeg har kjørt koden med valgrind som spesisfisert i oppgaveteksten og får ingen minnelekasjer: *valgrind --leak-check=full --track-origins=yes   --show-leak-kinds=all -s*

## D1 Client

### d1_create_client()
Allokerer dynamisk minne til D1Peer-structen. Bruker den innebygde socket()-metoden for å opprette en socket for klienten. Addressen og neste sekvensnummer initialiseres til 0.

### d1_delete()
Lukker socketen og frigjør plass for D1Peer som sendes inn dersom peer ikke er NULL.

### d1_get_peer_info
Her fylles addr-memberet til D1Peer som sendes inn. Bruker getaddrinfo for å finne sockaddr_in som er UDP og å IPv4. Passer på at hele lenkelisten som returneres av getaddrinfo frigjøres til slutt med freeaddrinfo. 

### d1_recv_data()
Bruker argumentet int sz som maks størrelse som kan leses til bufferet.
Antar at grunnen til at det sendes inn 1000 bytes i sz-argumentet i d1_test_client for å 
være sikker på at det er plass til hele pakken i bufferet. Metoden returnerer størrelsen på payloaden, og setter bufferet til å kun inneholde payloaden uten headeren. Dersom den mottar en disconnect-pakke sendes det ikke noen ACK (slik det står i d1_udp.h).

**Lager noen hjelpemetoder for at ikke for mye funksjonalitet skal være samlet på ett sted**

* **int correct_size_in_header()**
sjekker om størrelsen oppgitt i headeren til den mottatte pakken stemmer overens med antall bytes som ble lest inn i bufferet.

* **int generate_checksum_for_packet**
Bruker samme hjelpemetode for å generere checksum når det sendes pakker og for å sjekke at checksummen stemmer etter mottatt pakke. Sender med hele pakken inkludert checksummen til pakken som er mottatt. Dersom checksummen i headeren stemmer, skal resultatet bli 0 ut i fra XOR-logikk.
Metoden setter sammen to og to bytes, og foretar XOR-beregning på de 16 bitsene. Det som kan være en ulempe med en slik tilnærming er at checksummen som returneres settes til å være 0 i begynnelsen av metoden. Dersom checksummen av en eller annen grunn ikke oppdateres etter XOR-beregningene, kan 0 returneres selv om checksummen er feil. Dette skal dog ikke skje i metoden ettersom den eneste måten den ikke vil gå inn i for-løkken for å beregne XOR er dersom størrelsen er 0. Da er det noe feil med størrelsen, som sjekkes for i correct_size_in_header(). Dermed mener jeg denne måten å sjekke cheksum er tilstrekkelig.



* **int flag_data_packet(), int flag_ACK(), int flag_sequence_number(), int flag_ack_number()**
Hjelpemetoder for å sjekke bitsene som er satt i flags i Headeren ved hjelp av makroen fra header-filen. Ulik logikk ut i fra hvordan jeg bruker de i metodene som kaller de. Det ville nok vært mer ryddig dersom disse metodene fungerte på samme måte/hadde samme logikk.



### d1_send_data()
Dersom den sender en disconnect-pakke venter den ikke på ACK. Det ser ut som det kanskje sendes en ACK fra serveren selv om det er en disconnect-pakke fra klienten. Ettersom det står at disconnect-pakker og ACK-pakker ikke skal ackes fra motsatt side i header-filen (d1_udp.h), velger jeg å ikke vente på en ACK i dette tilfellet. Slik som d1_recv_data(), bruker også denne metoden generate_checksum_for_packet(). Den lager pakken, nuller ut checksum-feltet i headeren og genererer checksum. Deretter legges checksumen inn i headerens checksum-felt. Dersom det ikke er en disconnect-pakke, venter den på en ack fra serveren.

**Lager noen hjelpemetoder for at ikke for mye funksjonalitet skal være samlet på ett sted:**

* **int d1_wait_ack()**
Kalles på av d1_send_data etter den har sendt en pakke. 
Bruker select og timeout slik at den kun venter/blokkerer i 1 sekund.
Dersom noe går galt, timeouten går ut eller ack-nummeret er feil, returneres -1.
I såfall er det opp til d1_send_data å sende pakken på nytt og kalle på d1_wait_ack igjen.

* **uint16_t generate_checksum_for_packet()** genererer checksum basert på flags, øvre og nedre del av size fra headeren samt dataen/payloaden.  Denne checksummen legges så inn i checksum-feltet i headeren før pakken sendes av gårde, men dette gjøres i d1_send_data().


## D2 Client

### d2_client_create()
Bruker d1_create_client for å opprette en D1Peer og legge den inn i D2 client som er dynamisk allokert.

### d2_client_delete
Frigjør først D1Peer-structen som er i D2Client-structen, deretter selve D2Client-structen.

### d2_send_request()
Bruker d1_send_data() for å sende en PacketRequest-pakke. Returnerer 0 hvis sendingen feiler, ellers 1.

### d2_recv_response_size()
Kaller d1_recv_data for å motta pakke. Sjekker at typen er TYPE_RESPONSE_SIZE. Hvis den er det, returneres size. Hvis noe går galt returneres 0.

### d2_recv_response()
Kaller også d1_recv_data for å motta pakke. Den legger pakken (inkludert PacketResponse-header) i bufferet ettersom testfilen henter ut payloaden selv. Den konverterer ikke headeren til host byte order ettersom dette gjøres i testfilen d2_test_client. Returnerer 0 hvis noe går galt, ellers antall bytes mottatt.

### d2_alloc_local_tree() og struct LocalTreeStore
Her står det at LocalTreeStore skal endres. Jeg har valgt å opprette en ny struct OneNetNode som holder på et member som er 32 bytes (altså like stor som størrelsen på én NetNode). Dette gjorde jeg for å unngå sirkulær include (altså at de to headerfilene includer hverandre), som jeg måtte gjort hvis jeg skulle include NetNode-structen. Deretter la jeg til et "flexible array member" nederst i LocalTreeStore, der arrayet er av typen OneNetNode. Dette gjør at man kan legge til et variabelt antall elementer i arrayet, der hver indeks har plass til 32 bytes, altså kan man lagre så mange NetNodes som er ønskelig. For å vite hvor stort arrayet er, la jeg også til array_length, som sier hvor mange plasser det er i arrayen nodes. Slik ser det ut:

struct OneNetNode {
    uint8_t node[32]; 
};

struct LocalTreeStore
{
    int number_of_nodes;
    int array_length;
    OneNetNode nodes[]; 
};


Metoden d2_alloc_local_tree() allokerer plasse til LocalTreestore med størrelsen på struct LocalTreeStore + num_nodes*32 fordi én NetNode er 32 bytes. Den fleksible array memberen "nodes" får dermed lengde lik argumentet num_nodes som sendes inn i metoden. Initialiserer number_of_nodes i LocalTreeStore-en til å være 0. Det er metoden som legger inn noder som har ansvar for å øke number_of_nodes når den legger inn en node. Hvis den feiler, returneres NULL, ellers returneres treet.


### d2_free_local_tree()
Kaller free() på treet som sendes inn. Ettersom LocalTreeStore ikke inneholder noe som er dynamisk allokert, trengs ikke innholdet å frigjøres før selve treet frigjøres.


### d2_add_to_local_tree()
Legger noder fra en pakke/et buffer inn i det lokale treet. Metoden kaller hjelpemetoden make_netnodes. make_netnodes kunne nok fint blitt implementert inni d2_add_to_local_tree(), og hadde da ikke trengt å være en separat hjelpemetode. 

Men slik jeg har implementert det, tar make_netnodes inn de samme argumentene som d2_add_to_local_tree(). Den går over bufferet, lager NetNodes og fyller de inn med informasjonen fra bufferet. Deretter legger den NetNodene til i treet som sendes inn. Returverdien til make_netnode() er antall noder som ble lagt til.

d2_add_to_local_tree() bruker returverdien fra make_netnodes til å øke numbr_of_nodes i treet med tilsvarende antall. Hvis returverdien av make_netnodes er -1 betyr det at den prøvde å legge til en node utenfor arrayet, da returnerer metoden -1. Til slutt returneres neste ledige indeks, altså indeksen som ble sendt inn + antall noder som ble lagt til.

### d2_print_tree()
Metoden bruker først qsort for å sortere arrayet i treet etter id-en på nodene. For å bruke qsort lagde jeg sammenligningsmetoden compare som sammenligner id-ene til to og to noder. Dermed blir indeksen og id-en til noden den samme.

Etter arret med noder er sortert, kaller jeg en rekursiv print-metode print_nodes_rec(). Den tar inn treet, indeksen/id-en på noden og dybden på noden. Først printer den infoen om noden som sendes inn med "--" dybde antall ganger før informasjonen printes. Deretter gjør den rekursive kall på alle nodens barn (hvis den har barn) der dybden økes med 1 for hver "generasjon". Dermed blir utskriften på samme format som beskrevet i d2_lookup.h.

