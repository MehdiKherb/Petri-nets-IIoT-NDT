/*
 * Copyright (c) 2012, Texas Instruments Incorporated - http://www.ti.com/
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
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Enable TCP */
#define UIP_CONF_TCP 1
#define MQTT_CLIENT_CONF_LOG_LEVEL LOG_LEVEL_DBG
//#define DEBUG_MQTT 1
//#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_MAC LOG_LEVEL_DBG
/* Change to 1 to use with the IBM Watson IoT platform */
#define MQTT_CLIENT_CONF_WITH_IBM_WATSON 0

/*
 * The IPv6 address of the MQTT broker to connect to.
 * Ignored if MQTT_CLIENT_CONF_WITH_IBM_WATSON is 1
 */
#define MQTT_CLIENT_CONF_BROKER_IP_ADDR  "fd00::1" //"0064:ff9b:0000:0000:0000:0000:224f:37dc" // "64:ff9b::34.79.55.220"


/* data rate & payload size*/
#define DATA_SEND_INTERVAL (0.07*CLOCK_SECOND)
#define PAYLOAD_SIZE 80 /* on MAC level we get 98, without counting the packetbuf header*/

/* MQTT publishing rate */
#define DEFAULT_PUBLISH_INTERVAL    (DATA_SEND_INTERVAL*20)

#define NETWORK_CONVERGENCE_TIME (300*CLOCK_SECOND)

/*===================if CSMA is used==================*/
/* CSMA configurations */
//#define CSMA_CONF_MAX_FRAME_RETRIES 2

/* the Backoff Exponent (BE) defines the delay between retransmissions in the presence of collisions in CSMA */

//#define CSMA_CONF_MIN_BE 1
//#define CSMA_CONF_MAX_BE 3

/* macMaxCSMABackoffs: Maximum number of backoffs in case of channel busy/collision.*/
//#define CSMA_CONF_MAX_BACKOFF 0

/*====================================================*/

/*===================if TSCH is used==================*/
//#define TSCH_CONF_MAC_MAX_FRAME_RETRIES 0

/* Number of Packets Queue buffer */
#define QUEUEBUF_CONF_NUM 8

/* UDP configuration */
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
/*
 * The Organisation ID.
 *
 * When in Watson mode, the example will default to Org ID "quickstart" and
 * will connect using non-authenticated mode. If you want to use registered
 * devices, set your Org ID here and then make sure you set the correct token
 * through MQTT_CLIENT_CONF_AUTH_TOKEN.
 */
/*
#ifndef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_CONF_ORG_ID "quickstart"
#endif
*/
/*
 * The MQTT username.
 *
 * Ignored in Watson mode: In this mode the username is always "use-token-auth"
 */
#define MQTT_CLIENT_CONF_USERNAME "demo-device-001-auth@org.eclipse.ditto"

/*
 * The MQTT auth token (password) used when connecting to the MQTT broker.
 *
 * Used with as well as without Watson.
 *
 * Transported in cleartext!
 */
#define MQTT_CLIENT_CONF_AUTH_TOKEN "my-password"
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */
