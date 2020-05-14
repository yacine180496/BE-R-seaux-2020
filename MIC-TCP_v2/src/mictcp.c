#include <mictcp.h>
#include <api/mictcp_core.h>
#define MAX_SENDINGS 1000
/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

/* Variable Globale */

mic_tcp_sock sock;
mic_tcp_sock_addr addr_sock_dest;
int PE = 0 ; 
int PA = 0; 
int numero_packet = 0 ; 

int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

   if((result = initialize_components(sm)) == -1){
       return -1;
   }
   else{
        sock.fd = 1;
        sock.state = CONNECTED;
        set_loss_rate(0);
        return sock.fd;
   }
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   if (sock.fd == socket){
       memcpy((char *)&sock.addr,(char *)&addr, sizeof(mic_tcp_sock_addr));
       return 0;
   }
   else {
       return -1;
   }
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
     int result = -1;
    if (socket == 1) {
        sock.addr = *addr;
	    sock.state = CONNECTED;
	    result = 0;
	}
	return result; 

}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect (int socket, mic_tcp_sock_addr addr) {
	printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    int result = -1;
    if(socket == 1) {
        sock.addr = addr;
	    sock.state = CONNECTED;
	    result = 0;
	}
	return result; 
}


/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_pdu pdu_emis ; 
    mic_tcp_pdu ack ; 
    int nb_sent = 0 ; 
    unsigned long timeout = 100 ; //100 ms
    int size_PDU ;
    int ack_recu= 0;

    if( (sock.fd == mic_sock) && (sock.state = CONNECTED)) {

        /* Construction du PDU a emettre */

        // Header
        pdu_emis.header.source_port = sock.addr.port;
        pdu_emis.header.dest_port = addr_sock_dest.port;
        pdu_emis.header.seq_num = PE;
        pdu_emis.header.syn = 0;
        pdu_emis.header.ack = 0;
        pdu_emis.header.fin = 0;

        // Payload
        pdu_emis.payload.data = mesg;
        pdu_emis.payload.size = mesg_size;

        // Envoi du PDU
        size_PDU =  IP_send(pdu_emis, sock.addr);
        numero_packet++;
        nb_sent++;

        // WAIT_FOR_ACK
        sock.state = WAIT_FOR_ACK;

        /* Malloc pour l'ACK */
        ack.payload.size = 2*sizeof(short)+2*sizeof(int)+3*sizeof(char);
        ack.payload.data = malloc(ack.payload.size);

        // Prochaine_trame_a_Emettre(PE)
        PE = (PE+1) % 2 ; 

        while(!ack_recu) {
            if ((IP_recv(&(ack),&addr_sock_dest, timeout) >= 0) && (ack.header.ack == 1) && (ack.header.ack_num == PE))  {
                ack_recu = 1 ; 
            } 
            else { //timer fini ou numéro !=PE
                // il faut renvoyer le PDU
                if (nb_sent < MAX_SENDINGS) {
                    size_PDU = IP_send(pdu_emis, addr_sock_dest);
                    printf("Renvoi du packet : %d, tentative ° : %d.\n",numero_packet,nb_sent);
                    nb_sent++;
                }
                else {
                    printf("Erreur : nombre de renvoi dépasse/n");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    else {
        printf("Erreur au niveau du numero de socket ou connexion non etablie\n");
        exit(EXIT_FAILURE);
    } 
    
    sock.state = CONNECTED;
    return size_PDU;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size) {
    mic_tcp_payload p;
    int taille_lu =-1;

	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

 	p.data = mesg;
	p.size = max_mesg_size;

    if (sock.fd == socket && sock.state == CONNECTED){

    /* Attente d'un PDU */
    sock.state = WAIT_FOR_PDU;

    /* Recuperation d'un PDU dans le buffer de reception */
    taille_lu = app_buffer_get(p);

    sock.state = CONNECTED;
  }
  
  return taille_lu;
}


/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return  close(socket); 
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr) {
    mic_tcp_pdu ack;

    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

    if (pdu.header.seq_num == PA) { // n°seq == PA
        /**Acceptation de DT**/
        /* Ajout de la charge utile du PDU recu dans le buffer de reception */
        app_buffer_put(pdu.payload);

        /* Incrémentation de PA */
        PA = (PA +1) % 2;
    }
    // sinon, n°seq != PA, rejet de la DT => PA reste le même

    /* Construction d'un ACK */
    // Header
    ack.header.source_port = sock.addr.port;
    ack.header.dest_port = addr.port;
    ack.header.ack_num = PA;
    ack.header.syn = 0;
    ack.header.ack = 1;
    ack.header.fin = 0;

    ack.payload.size = 0; // on n'envoie pas de DU

    /* Envoi de l'ACK */
    IP_send(ack, addr);
}

