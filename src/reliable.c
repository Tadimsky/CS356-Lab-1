
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

/*
 Structure we use for tracking packets to be sent
 */
struct node {
    
    packet_t * pack;
    
    int time_sent; //update everytime we resend
    uint16_t request_attempts;
    
    bool ack_received;  //mark true when sender receives ACK from receiver
    //then we can remove from linked list
    
    struct node * next;
    struct node ** prev;
    
};

typedef struct node node_t;


struct reliable_state {
    /* aka rel_t */
    rel_t *next;			/* Linked list for traversing all connections */
    rel_t **prev;
    
    conn_t *c;			/* This is the connection object */
    
    /* Add your own data fields below this */

    // The final data structure we want.
    node_t * received_data_linked_list;
    
    // Pointer to slot where the most recent data was added.
    node_t * last_data;
    
    int window_size;
    // All packets with sequence number lower than ackno have been recieved by the SENDER
    uint32_t ackno;
    
    // The next seqno the receiver is expecting. The lowest order number in the current window.
    uint32_t seqno;
    
    //Array of size window that holds incomming packets so that they can be added to our linked list in order.
    packet_t* receive_ordering_buffer;
    
};
rel_t *rel_list;


node_t *node_create(packet_t * new_packet) {
    node_t *n;
    n = xmalloc (sizeof (*n));
    
    n -> pack = new_packet;
    n -> request_attempts = 0;
    n -> ack_received = false;
    
    return n;
}

/* Returns a packet with seqno = 0 (acts as a NULL packet)
 */
//packet_t * null_packet () {
//    packet_t * p;
//    p = xmalloc(sizeof(*p));
//    memset (p, 0, sizeof (*p));
//    p -> seqno = 0;
//    return p;
//}
packet_t null_packet () {
    packet_t p;
//    p = xmalloc(sizeof(*p));
//    memset (p, 0, sizeof (*p));
    p.seqno = 0;
    return p;
}


/* Creates a new reliable protocol session, returns NULL on failure.
 * Exactly one of c and ss should be NULL.  (ss is NULL when called
 * from rlib.c, while c is NULL when this function is called from
 * rel_demux.) */
rel_t *
rel_create (conn_t *c, const struct sockaddr_storage *ss,
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
    
    /*
    node_t *node;
    node = node_create(NULL);
    r->current_node = node;
    //current_node is initialized as an empty node
    */
    
    r->window_size = cc->window;
    
    packet_t buff[cc->window];
    r->receive_ordering_buffer = buff;
    r->seqno = 1;
    r->ackno = 1;

    return r;
}

void
rel_destroy (rel_t *r)
{
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
void
rel_demux (const struct config_common *cc,
           const struct sockaddr_storage *ss,
           packet_t *pkt, size_t len)
{
    /* LAB ASSIGNMENT SAYS NOT TO TOUCH rel_demux() */
}

/* This function is called when the 0th index of the receive_ordering_buffer contains a packet, not NULL.
 * When this occurs, we move the 0th packet into the received_data_linked_list, move all elements of 
 * receive_ordering_buffer forward by one index, and update the next expected seqno in reliable_state.
 */
void
shift_receive_buffer (rel_t *r) {
    
    node_t * new_node = node_create(&r -> receive_ordering_buffer[0]);
    
    r -> last_data -> next = new_node;
    r -> last_data = new_node;
    
    //SEND ACK(new_node+1);
    
    for (int i = 0; i < r -> window_size - 2; i--) {
        r -> receive_ordering_buffer[i] = r -> receive_ordering_buffer[i+1];
    }
    
    r -> receive_ordering_buffer[r -> window_size - 1] = null_packet();
    
    (r -> seqno)++;
    
    // if after shifting the buffer we now have the next packet available too, shift the buffer again
    if (r -> receive_ordering_buffer[0].seqno != null_packet().seqno) {
        shift_receive_buffer(r);
    }
}


/*
 Examine incomming packets. If the packet is an ACK then remove the corresponding packet from our send buffer.  
 If it is data, see if we have already received that data (if it is a lower seq number than the lowest of our current frame, or if it is already in our recieved buffer).  
 If it has already been recieved, do nothing with it but send the ACK. If it has not been recieved, put it in its ordered place in the buffer and send the ACK.

 n is the actual size where pkt -> len is what it should be.  
 A discrepancy would indicate some bits were or the packet is not properly formed. Thus we will discard it.
 */
void
rel_recvpkt (rel_t *r, packet_t *pkt, size_t n)
{
    /* TODO */
    
    if (n != pkt->len) {
        //we have not received the full packet or it's an error packet
        //ignore it and wait for the packet to come in its entirety
        debug("size_t n is different from pkt->len; this is an error packet, return");
        return;
    }
    
    
    if (pkt->len < 12) {
        //pkt is an ACK
        
        //handle sender buffer appropriately and return
        
        
    } else {
        //pkt is a data PACKET
        
        int offset = (pkt -> seqno) - (r -> ackno);
        // offset tells where in the receive_ordering_buffer this packet falls
        
        r -> receive_ordering_buffer[offset] = *pkt;
        
        if ((r -> receive_ordering_buffer[0]).seqno != null_packet().seqno) {
            shift_receive_buffer(r);
        }
        
    }
    
    return;
}


/*
 Take information from standard input and create packets.  Will call some means of sending to the appropriate receiver.
 */
void
rel_read (rel_t *s)
{
	char buffer[500];
	// drain the console
	while (true) {
		int bytes_read = conn_input(s->c, buffer, 500);
		debug("Read: %d\n", bytes_read); // no more

		if (bytes_read == 0) {
			return;
		}

		packet_t pkt;
		int packet_size = 12;
		if (bytes_read > 0) {
			memcpy(pkt.data, buffer, bytes_read);
			packet_size += bytes_read;

		}
		pkt.seqno = htonl(s->seqno); // set the sequence number
		s->seqno = s->seqno + 1;
		pkt.len = htons(packet_size);
		pkt.ackno = htonl(s->ackno); // the sequence number of the last packet received + 1
		pkt.cksum = cksum((void*)&pkt, packet_size);

		conn_sendpkt(s->c, &pkt, packet_size);
		if (bytes_read == -1) {
			return;
		}
	}

}

void
rel_output (rel_t *r)
{
    /* RECEIVER SIDE */
    /* TODO */
    
    
    
    
    
}

void
rel_timer ()
{
    /* Retransmit any packets that need to be retransmitted */
    
    /* TODO */
    
    
}


