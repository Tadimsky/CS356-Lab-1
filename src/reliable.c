
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include <stdbool.h>
#include "rlib.h"

#define debug(...)   fprintf(stderr, __VA_ARGS__)
#define DATA_PACKET_SIZE 12
#define ACK_PACKET_SIZE 8

#define ACK_START 1
#define SEQ_START 1

/*
 Structure we use for tracking packets to be sent
 */
struct node {
    
    packet_t * pkt;
    int time_sent; //update everytime we resend
    uint16_t request_attempts;
    
    bool ack_received;  //mark true when sender receives ACK from receiver
    
    struct node * next;
    struct node ** prev;

};
typedef struct node node_t;

node_t * node_create(packet_t * new_packet) {
    node_t *n;
    n = xmalloc (sizeof (*n));
    
    n -> pkt = new_packet;
    n -> request_attempts = 0;
    
    return n;
}


/* Returns a packet with seqno = 0 (acts as a NULL packet)
 */
packet_t null_packet () {
    packet_t p;
    p.seqno = 0;
    return p;
}


struct reliable_state {
    rel_t *next;			/* Linked list for traversing all connections */
    rel_t **prev;
    
    conn_t *c;			/* This is the connection object */
    
    /* Add your own data fields below this */

    // The final data structure we want.
    node_t * received_data_linked_list;
    
    // Pointer to slot where the most recent data was received.
    node_t * last_data_received;
    
//    // Data structure with all sent packets
//    node_t * sent_data_linked_list;
    
//    // Pointer to the last sent packet node
//    node_t * last_data_sent;
    
    int window_size;
    
    // All packets with sequence number lower than ackno have been recieved by the SENDER
    // last frame received
    uint32_t ackno;

    // the last ack received from the receiver
    uint32_t last_ack_received;
    
    // The next seqno the receiver is expecting. The lowest order number in the current window.
    uint32_t seqno;
    
    //Array of size window that holds incomming packets so that they can be added to our linked list in order.
    packet_t* receive_ordering_buffer;

    //Array of size window that holds sent packets. Remove from the buffer when ACK comes back.
    packet_t* send_ordering_buffer;

};
rel_t *rel_list;


/* Creates a new reliable protocol session, returns NULL on failure.
 * Exactly one of c and ss should be NULL.  (ss is NULL when called
 * from rlib.c, while c is NULL when this function is called from
 * rel_demux.) */
rel_t * rel_create (conn_t *c, const struct sockaddr_storage *ss,
            const struct config_common *cc)
{
    rel_t *r;
    r = xmalloc (sizeof (*r));
    memset (r, 0, sizeof (*r));
    
    if (!c) {
        c = conn_create (r, ss);
        if (!c) {
            free (r);
            return NULL;
        }
    }
    
    r->c = c;
    r->next = rel_list;
    r->prev = &rel_list;
    if (rel_list)
        rel_list->prev = &r->next;
    rel_list = r;
    
    /* Do any other initialization you need here */
    
    /* TODO */
    
    r->window_size = cc->window;
    
    packet_t * receive_buff = xmalloc(sizeof(packet_t) * cc->window);
    r->receive_ordering_buffer = receive_buff;
//    uint32_t * send_buff = xmalloc(sizeof(uint32_t) * cc->window);
//    r->send_ordering_buffer = send_buff;
    r->send_ordering_buffer = receive_buff;
    r->seqno = SEQ_START;
    r->ackno = ACK_START;
    
    packet_t new_packet = null_packet();
    node_t * first_node = node_create(&new_packet);
    r->received_data_linked_list = first_node;
    r->last_data_received = first_node;
//    r->sent_data_linked_list = first_node;
//    r->last_data_sent = first_node;

    debug("this should be 0: %d \n", r->last_data_received->pkt->seqno);
    
    return r;
}

void rel_destroy (rel_t *r)
{
    debug("---Entering rel_destroy---\n");
    if (r->next)
        r->next->prev = r->prev;
    *r->prev = r->next;
    conn_destroy (r->c);
    
    /* Free any other allocated memory here */
    
    /* TODO */
    rel_t* current = r;
    while (current != NULL) {
        rel_t* next = r->next;
        free(current);
        current = next;
    }
    
    /*
     Todo:
     
     Delete:
     node_t * received_data_linked_list;
     node_t * last_data;
     packet_t* receive_ordering_buffer;
     */
    
    
    
}


/* This function only gets called when the process is running as a
 * server and must handle connections from multiple clients.  You have
 * to look up the rel_t structure based on the address in the
 * sockaddr_storage passed in.  If this is a new connection (sequence
 * number 1), you will need to allocate a new conn_t using rel_create
 * ().  (Pass rel_create NULL for the conn_t, so it will know to
 * allocate a new connection.)
 */
void rel_demux (const struct config_common *cc,
           const struct sockaddr_storage *ss,
           packet_t *pkt, size_t len)
{
    /* LAB ASSIGNMENT SAYS NOT TO TOUCH rel_demux() */
}


/* This function is called when the 0th index of the receive_ordering_buffer contains a packet, not NULL.
 * When this occurs, we move the 0th packet into the received_data_linked_list, move all elements of 
 * receive_ordering_buffer forward by one index, and update the next expected seqno in reliable_state.
 */
void shift_receive_buffer (rel_t *r) {
    debug("---Entering shift_receive_buffer---\n");
    node_t * new_node = node_create(&r -> receive_ordering_buffer[0]);
    
    if (r->last_data_received != NULL) {
    	r -> last_data_received -> next = new_node;
    }
    r -> last_data_received = new_node;

    
    
    debug("last data received seqno = %d \n", r->last_data_received->pkt->seqno);
    
    // SEND ACK(new_node+1);
    // increment ack no
    // r->ackno = r->ackno + 1;
    
    
    
    int i;
    for (i = 0; i < r -> window_size - 2; i--) {
        r -> receive_ordering_buffer[i] = r -> receive_ordering_buffer[i+1];
    }
    
    r -> receive_ordering_buffer[r -> window_size - 1] = null_packet();
    
    // if after shifting the buffer we now have the next packet available too, shift the buffer again
    if (r -> receive_ordering_buffer[0].seqno != null_packet().seqno) {
        shift_receive_buffer(r);
    }
    return;
}



void shift_send_buffer (rel_t *r) {
    debug("---Entering shift_send_buffer---\n");
    for (int i = 0; i < r->window_size - 1; i++) {
      r->send_ordering_buffer[i] = r->send_ordering_buffer[i + 1];      
    }
    r->send_ordering_buffer[r->window_size - 1] = null_packet();    
}


/*
 Examine incomming packets. If the packet is an ACK then remove the corresponding packet from our send buffer.  
 If it is data, see if we have already received that data (if it is a lower seq number than the lowest of our current frame, or if it is already in our recieved buffer).  
 If it has already been recieved, do nothing with it but send the ACK. If it has not been recieved, put it in its ordered place in the buffer and send the ACK.

 n is the actual size where pkt -> len is what it should be.  
 A discrepancy would indicate some bits were or the packet is not properly formed. Thus we will discard it.
 */
void rel_recvpkt (rel_t *r, packet_t *pkt, size_t n)
{
    debug("---Entering rel_recvpkt---\n");
	pkt->len = ntohs(pkt->len);
	pkt->ackno = ntohl(pkt->ackno);
	pkt->seqno = ntohl(pkt->seqno);

    if (n != pkt->len) {
        //we have not received the full packet or it's an error packet
        //ignore it and wait for the packet to come in its entirety
        debug("size_t n is different from pkt->len; this is an error packet, return");
        return;
    }
    
    debug("received packet of len: %d\n", pkt->len);
    
    if (pkt->len < DATA_PACKET_SIZE) {
        //pkt is an ACK
        debug("Received an ACK of: %d\n", pkt->ackno);
        debug("\tReceiver received packet with seqno: %d\n", pkt->ackno - 1);
        debug("\tFirst packet in send window: %d", ntohl(r->send_ordering_buffer[0].seqno));

        // the ackno that was sent to us should be one larger than the last ack received on the sender side
        if (pkt->ackno != r->last_ack_received + 1) {
          debug("FATAL ERROR: ackno is not in order");
          die():
        }

        uint32_t ackno = pkt->ackno;

        // if the ackno is for the first packet in the send window (which it should be)
        // the ack number is the number of the packet the receiver is waiting for.
        // one less than this is the packet that was just received.
        if (ackno != ntohl(r->send_ordering_buffer[0].seqno) + 1) {
          debug("FATAL ERROR: ackno does not correspond to the first packet in the send window\n");
          die();
        }

        // if we get to this point, then all seems good and we wil remove the packet that was acked from the beginning of the array
        // increment last_ack_received and then shift the buffer

        r->send_ordering_buffer[0] = null_packet();        
        r->last_ack_received = ackno;

        shift_send_buffer(r);
    } 
    else {
        //pkt is a data PACKET
        debug("received Data packet\n");
        debug("packet contents: %s\n", pkt->data);
        int offset = (pkt -> seqno) - (r -> ackno);
        
        // offset tells where in the receive_ordering_buffer this packet falls
        debug("packet in window slot: %d \n", offset);
        r -> receive_ordering_buffer[offset] = *pkt;
        if ((r -> receive_ordering_buffer[0]).seqno != null_packet().seqno) {
            shift_receive_buffer(r);
        }
        rel_output(r);
        
    }
    
    return;
}


/*
 Take information from standard input and create packets.  Will call some means of sending to the appropriate receiver.
 */
void
rel_read (rel_t *r)
{
    debug("---Entering rel_read---\n");
	char buffer[500];
	// drain the console
	while (true) {
        
        // this may need to be r->seqno - 1
      if (r->seqno - r->last_ack_received > r->window_size) {
        // cannot fit any new packets into the buffer
        return;
      }

		  int bytes_read = conn_input(r->c, buffer, 500);

		if (bytes_read == 0) {
			return;
		}

		packet_t pkt;
		int packet_size = DATA_PACKET_SIZE;
		if (bytes_read > 0) {
			memcpy(pkt.data, buffer, bytes_read);
			packet_size += bytes_read;

		}
		pkt.seqno = htonl(r->seqno); // set the sequence number
		r->seqno = r->seqno + 1;
		pkt.len = htons(packet_size);
		pkt.ackno = htonl(r->ackno); // the sequence number of the last packet received + 1
		pkt.cksum = cksum((void*)&pkt, packet_size);		

    // this packet seqno into the sender buffer and keep here until we receive the ack back from receiver
    r->send_ordering_buffer[ntohl(pkt.seqno) - r->last_ack_received] = pkt;
        /*
         THIS FUNCTION SHOULD ALSO BE SENDING PACKETS UNTIL THE send_ordering_buffer IS FULL
         */
    
    conn_sendpkt(r->c, &pkt, packet_size);

		if (bytes_read == -1) {
			return;
		}
	}
    return;

}


void send_ack(rel_t *r) {
    debug("---Entering send_ack---\n");
	packet_t pkt;
	pkt.len = htons(ACK_PACKET_SIZE);
	r->ackno = r->ackno + 1;
	pkt.ackno = htonl(r->ackno);
	pkt.cksum = cksum((void*)&pkt, ACK_PACKET_SIZE);

	conn_sendpkt(r->c, &pkt, ACK_PACKET_SIZE);
    return;
}

void
rel_output (rel_t *r)
{
	debug("---Entering rel_output---\n");
    debug("DEBUG: Data Packet contents: %s\n", r -> last_data_received -> pkt -> data);
    if (conn_bufspace(r -> c) > (r -> last_data_received -> pkt -> len)) {
        conn_output(r -> c, r -> last_data_received -> pkt -> data, r -> last_data_received -> pkt -> len);
        send_ack(r);
    }
    
    return;
}

void
rel_timer ()
{
    /* Retransmit any packets that need to be retransmitted */
    
    /* TODO */
    
    
}


