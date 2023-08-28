/*
 * Copyright (c) 201, RISE SICS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "net/queuebuf.h"
/* Log configuration */
#include "sys/log.h"
#include "sys/rtimer.h"
#include <string.h>
#include <stdio.h>
#define LOG_MODULE "RPL BR"
#define LOG_LEVEL LOG_LEVEL_INFO

/* UDP configuration */
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

static struct simple_udp_connection udp_conn;
static uint16_t seq_num_recv = 0;
/* Declare and auto-start this file's process */
PROCESS(contiki_ng_br, "Contiki-NG Border Router");
PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&contiki_ng_br, &udp_server_process);

static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  //getting the sequence number and the send_time from the payload
  char * seq_num_str = strtok((char *) data, "|");
  char * sent_time_str = strtok(NULL, "|");
  //cast the strings to integers
  int seq_num, sent_time;
  int delay; 
  sscanf(seq_num_str, "%d", &seq_num);
  sscanf(sent_time_str, "%d", &sent_time);

  rtimer_clock_t t_recv = RTIMER_NOW();

  delay = t_recv - sent_time;
  delay = delay/1000; // convert from µs to ms
  LOG_INFO("\n");
  LOG_INFO("Received %d bytes from %d, seq_num_recv = %d, seq_num_send = %d, delay = %d, send_time = %d, receive time = %ld\n",datalen, sender_addr->u8[15], seq_num_recv, seq_num, delay, sent_time, t_recv);
  seq_num_recv ++;
  char replyData[64];
  snprintf(replyData, sizeof(replyData), "%d", delay);
  LOG_INFO("Reply with %s\n", replyData);
  simple_udp_sendto(&udp_conn, replyData, 64, sender_addr);

  //LOG_INFO_6ADDR(sender_addr);
  //LOG_INFO("\n");
  //LOG_INFO("\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(contiki_ng_br, ev, data)
{
  PROCESS_BEGIN();

#if BORDER_ROUTER_CONF_WEBSERVER
  PROCESS_NAME(webserver_nogui_process);
  process_start(&webserver_nogui_process, NULL);
#endif /* BORDER_ROUTER_CONF_WEBSERVER */

  LOG_INFO("Contiki-NG Border Router started\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL, UDP_CLIENT_PORT , udp_rx_callback);

  PROCESS_END();
}