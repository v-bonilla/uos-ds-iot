/** Broadcast PingPong */ 

#include "contiki.h" 
#include "lib/random.h" 
#include "net/rime.h" 
#include "etimer.h" 
#include <stdio.h> 

#define BROADCAST_PORT 129 

#define SEND_INTERVAL (CLOCK_SECOND*2) 

static struct broadcast_conn broadcast;

PROCESS(pingpong, "Ping Pong Process");
AUTOSTART_PROCESSES(&pingpong);

 /*
 * Broadcast callback
 * This method gets called in case that a broadcast is received
 */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) 
{ 
    printf("Broadcast received from %d.%d: '%s'\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr()); 
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

PROCESS_THREAD(pingpong, ev, data)
{
    static struct etimer et;
    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    PROCESS_BEGIN();
    broadcast_open(&broadcast, 2863, &broadcast_call);
    while (1)
    {
        etimer_set(&et, SEND_INTERVAL);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        packetbuf_copyfrom("40", 2);
        broadcast_send(&broadcast);
    }
    PROCESS_END();
}