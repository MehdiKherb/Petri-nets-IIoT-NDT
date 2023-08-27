/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
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
#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "mqtt-prop.h"
#include "net/ipv6/uip.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/ipv6/uip-udp-packet.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "client.h"
#include "sys/node-id.h"

#include "sys/rtimer.h"

#include <string.h>
#include <strings.h>
#include <stdarg.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "mqtt-client"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif
/*---------------------------------------------------------------------------*/
/* Controls whether the example will work in IBM Watson IoT platform mode */
#ifdef MQTT_CLIENT_CONF_WITH_IBM_WATSON
#define MQTT_CLIENT_WITH_IBM_WATSON MQTT_CLIENT_CONF_WITH_IBM_WATSON
#else
#define MQTT_CLIENT_WITH_IBM_WATSON 0
#endif
/*---------------------------------------------------------------------------*/
/* MQTT broker address. Ignored in Watson mode */
#ifdef MQTT_CLIENT_CONF_BROKER_IP_ADDR
#define MQTT_CLIENT_BROKER_IP_ADDR MQTT_CLIENT_CONF_BROKER_IP_ADDR
#else
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
#endif
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*
 * MQTT Org ID.
 *
 * If it equals "quickstart", the client will connect without authentication.
 * In all other cases, the client will connect with authentication mode.
 *
 * In Watson mode, the username will be "use-token-auth". In non-Watson mode
 * the username will be MQTT_CLIENT_USERNAME.
 *
 * In all cases, the password will be MQTT_CLIENT_AUTH_TOKEN.
 */
/*#ifdef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_ORG_ID MQTT_CLIENT_CONF_ORG_ID
#else
#define MQTT_CLIENT_ORG_ID "quickstart"
#endif*/
/*---------------------------------------------------------------------------*/
/* MQTT token */
#ifdef MQTT_CLIENT_CONF_AUTH_TOKEN
#define MQTT_CLIENT_AUTH_TOKEN MQTT_CLIENT_CONF_AUTH_TOKEN
#else
#define MQTT_CLIENT_AUTH_TOKEN "AUTHTOKEN"
#endif
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_IBM_WATSON
/* With IBM Watson support */
static const char *broker_ip = "0064:ff9b:0000:0000:0000:0000:b8ac:7cbd";
#define MQTT_CLIENT_USERNAME "use-token-auth"

#else /* MQTT_CLIENT_WITH_IBM_WATSON */
/* Without IBM Watson support. To be used with other brokers, e.g. Mosquitto */
static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

#ifdef MQTT_CLIENT_CONF_USERNAME
#define MQTT_CLIENT_USERNAME MQTT_CLIENT_CONF_USERNAME
#else
#define MQTT_CLIENT_USERNAME "use-token-auth"
#endif

#endif /* MQTT_CLIENT_WITH_IBM_WATSON */
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_STATUS_LED
#define MQTT_CLIENT_STATUS_LED MQTT_CLIENT_CONF_STATUS_LED
#else
#define MQTT_CLIENT_STATUS_LED LEDS_GREEN
#endif
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_WITH_EXTENSIONS
#define MQTT_CLIENT_WITH_EXTENSIONS MQTT_CLIENT_CONF_WITH_EXTENSIONS
#else
#define MQTT_CLIENT_WITH_EXTENSIONS 0
#endif
/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)

/*---------------------------------------------------------------------------*/
/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_NEWCONFIG       6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_CLIENT_ID_LEN        32
//#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_CLIENT_ID           "mqtt-client"
#define DEFAULT_EVENT_TYPE_ID       "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE  "+"
#define DEFAULT_BROKER_PORT         1883
//#define DEFAULT_PUBLISH_INTERVAL    (5 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER    60
#define DEFAULT_RSSI_MEAS_INTERVAL  (CLOCK_SECOND * 30)
/*---------------------------------------------------------------------------*/
//#define MQTT_CLIENT_SENSOR_NONE     (void *)0xFFFFFFFF
/*---------------------------------------------------------------------------*/
/* Payload length of ICMPv6 echo requests used to measure RSSI with def rt */
#define ECHO_REQ_PAYLOAD_LEN   20
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_client_process);
PROCESS(udp_process, "UDP process");
AUTOSTART_PROCESSES(&mqtt_client_process, &udp_process);
/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config {
  //char org_id[CONFIG_ORG_ID_LEN];
  //char type_id[CONFIG_TYPE_ID_LEN];
  //char client_id[CONFIG_CLIENT_ID_LEN];
  char auth_token[CONFIG_AUTH_TOKEN_LEN];
  //char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  //char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  int def_rt_ping_interval;
  uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    64
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 *
 * d:quickstart:status:EUI64 is 32 bytes long
 * iot-2/evt/status/fmt/json is 25 bytes
 * We also need space for the null termination
 */
#define BUFFER_SIZE 64
//static char client_id[BUFFER_SIZE];
//static char pub_topic[BUFFER_SIZE];
//static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
// features to be updated
//static int bufferState = QUEUEBUF_NUM;
static int EEDelay = 0;
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
//#define QUICKSTART "quickstart"
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
//static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
//static struct uip_icmp6_echo_reply_notification echo_reply_notification;
//static struct etimer echo_request_timer;
//static int def_rt_rssi = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
/*#if MQTT_CLIENT_WITH_EXTENSIONS
extern const mqtt_client_extension_t *mqtt_client_extensions[];
extern const uint8_t mqtt_client_extension_count;
#else
static const mqtt_client_extension_t *mqtt_client_extensions[] = { NULL };
static const uint8_t mqtt_client_extension_count = 0;
#endif*/
/*---------------------------------------------------------------------------*/
/* MQTTv5 */
/*#if MQTT_5
static uint8_t PUB_TOPIC_ALIAS;

struct mqtt_prop_list *publish_props;
*/
/* Control whether or not to perform authentication (MQTTv5) */
/*#define MQTT_5_AUTH_EN 0
#if MQTT_5_AUTH_EN
struct mqtt_prop_list *auth_props;
#endif
#endif*/
/*---------------------------------------------------------------------------*/
PROCESS(mqtt_client_process, "MQTT Client");
/*---------------------------------------------------------------------------*/
static bool
have_connectivity(void)
{
  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
     uip_ds6_defrt_choose() == NULL) {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------*/
/*static int
ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}*/
/*---------------------------------------------------------------------------*/
/*
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data,
                   uint16_t datalen)
{
  if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
    def_rt_rssi = (int16_t)uipbuf_get_attr(UIPBUF_ATTR_RSSI);
  }
}*/
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    LOG_DBG("Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  case MQTT_EVENT_CONNECTION_REFUSED_ERROR: {
    LOG_DBG("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    process_poll(&mqtt_client_process);
    break;
  }
  case MQTT_EVENT_PUBLISH: {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      LOG_DBG("Application received publish for topic '%s'. Payload "
              "size is %i bytes.\n", msg_ptr->topic, msg_ptr->payload_chunk_length);
    }

/*    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic),
                msg_ptr->payload_chunk, msg_ptr->payload_chunk_length);
*/
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
    break;
  }
  case MQTT_EVENT_SUBACK: {
#if MQTT_31
    LOG_DBG("Application is subscribed to topic successfully\n");
#else
    struct mqtt_suback_event *suback_event = (struct mqtt_suback_event *)data;

    if(suback_event->success) {
      LOG_DBG("Application is subscribed to topic successfully\n");
    } else {
      LOG_DBG("Application failed to subscribe to topic (ret code %x)\n", suback_event->return_code);
    }
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
#endif
    break;
  }
  case MQTT_EVENT_UNSUBACK: {
    LOG_DBG("Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {
    LOG_DBG("Publishing complete.\n");
    break;
  }
#if MQTT_5_AUTH_EN
  case MQTT_EVENT_AUTH: {
    LOG_DBG("Continuing auth.\n");
    struct mqtt_prop_auth_event *auth_event = (struct mqtt_prop_auth_event *)data;
    break;
  }
#endif
  default:
    LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}


static int
init_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));

  //memcpy(conf.client_id, DEFAULT_CLIENT_ID, strlen(DEFAULT_CLIENT_ID));
  //memcpy(conf.org_id, MQTT_CLIENT_ORG_ID, strlen(MQTT_CLIENT_ORG_ID));
  //memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  memcpy(conf.auth_token, MQTT_CLIENT_AUTH_TOKEN,
           strlen(MQTT_CLIENT_AUTH_TOKEN));
  //memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID,
    //     strlen(DEFAULT_EVENT_TYPE_ID));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  //memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  //conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;

  return 1;
}
/*---------------------------------------------------------------------------*/
/*static void
subscribe(void)
{*/
  /* Publish MQTT topic in IBM quickstart format */
  /*mqtt_status_t status;

#if MQTT_5
  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0,
                          MQTT_NL_OFF, MQTT_RAP_OFF, MQTT_RET_H_SEND_ALL,
                          MQTT_PROP_LIST_NONE);
#else
  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);
#endif

  LOG_DBG("Subscribing!\n");
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    LOG_ERR("Tried to subscribe but command queue was full!\n");
  }
}*/
/*---------------------------------------------------------------------------*/
/*static char* formatToDitto(char* featurePath, char* value){
  char msg[APP_BUFFER_SIZE];
  char* msg_ptr;
  msg_ptr = msg;
  snprintf(msg_ptr, APP_BUFFER_SIZE, 
    "{\"topic\" : \"org.eclipse.ditto/demo-device-001/things/twin/commands/modify\" , \"headers\" :{}, \"path\" : \"%s\", \"value\" : %s}", featurePath, value)
  return msg;
}
*/

/*---------------------------------------------------------------------------*/
static void
publish(char* topic)
{
  int len;
  buf_ptr=app_buffer;

  //bufferState = (int) (QUEUEBUF_NUM - queuebuf_numfree());

  len = snprintf(buf_ptr, 512, 
    //'{" topic " : " org.eclipse.ditto/demo-device-001/things/twin/commands/modify " , " headers " :{}, " path " : " /features/bufferState/properties/remainingPackets " , " value " : 3}'); 
    //"{\"topic\" : \"org.eclipse.ditto/demo-device-001/things/twin/commands/modify\" , \"headers\" :{}, \"path\" : \"/features/bufferState/properties/remainingPackets\" , \"value\" : 3}");
    "{\"dataRate\" : %d, \"AverageDelay\" : %d}", (int) DATA_SEND_INTERVAL, EEDelay);

  if(len < 0 || len >= APP_BUFFER_SIZE) {
    LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", APP_BUFFER_SIZE,
            len);
    return;
  }

  mqtt_publish(&conn, NULL, topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

  LOG_DBG("Publish!\n");
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  static struct mqtt_string username,password;
  static struct mqtt_credentials *our_credentials = &conn.credentials;
  username = our_credentials->username;
  password = our_credentials->password;
  

  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
              15*CLOCK_SECOND,
               MQTT_CLEAN_SESSION_ON);
  LOG_DBG("Connecting to %s:%d with Credentials : username = %s, password = %s\n",conn.server_host, conn.server_port,username.string, password.string);
  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
  leds_off(MQTT_CLIENT_STATUS_LED);
}
/*---------------------------------------------------------------------------*/
/*static void
ping_parent(void)
{
  if(have_connectivity()) {
    uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0,
                   ECHO_REQ_PAYLOAD_LEN);
  } else {
    LOG_WARN("ping_parent() is called while we don't have connectivity\n");
  }
}*/
/*---------------------------------------------------------------------------*/
static void
state_machine(void)
{
  switch(state) {
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */

    mqtt_register(&conn, &mqtt_client_process, "Contiki_MQTT_CLIENT", mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

    /*
     * If we are not using the quickstart service (thus we are an IBM
     * registered device), we need to provide user name and password
     */
    
       mqtt_set_username_password(&conn, MQTT_CLIENT_USERNAME,conf.auth_token);
    
    /* _register() will set auto_reconnect. We don't want that. */
    conn.auto_reconnect = 0;
    connect_attempt = 1;

#if MQTT_5
    mqtt_prop_create_list(&publish_props);

    /* this will be sent with every publish packet */
    (void)mqtt_prop_register(&publish_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_PUBLISH,
                             MQTT_VHDR_PROP_USER_PROP,
                             "Contiki", "v4.5+");

    mqtt_prop_print_list(publish_props, MQTT_VHDR_PROP_ANY);
#endif

    state = STATE_REGISTERED;
    static struct mqtt_string client_id;
    client_id=conn.client_id;
    LOG_DBG("Init MQTT version %d, client_id = %s\n", MQTT_PROTOCOL_VERSION,client_id.string);
    /* Continue */
  case STATE_REGISTERED:
    //if(have_connectivity()) {
      /* Registered and with a public IP. Connect */
      LOG_DBG("Registered. Connect attempt %u\n",connect_attempt);
      //ping_parent();
      connect_to_broker();
      LOG_DBG("Client Connecting to broker : %s : %d\n",conf.broker_ip, conf.broker_port);
    /*} else {
      leds_on(MQTT_CLIENT_STATUS_LED);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
      LOG_DBG("Failed to connect \n");
    }*/
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
    break;
  case STATE_CONNECTING:
    //leds_on(MQTT_CLIENT_STATUS_LED);
    //ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    LOG_DBG("Connecting (%u)\n", connect_attempt);
    break;
  case STATE_CONNECTED:
    /* Don't subscribe unless we are a registered device */
    //if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) == 0) {
    //  LOG_DBG("Using 'quickstart': Skipping subscribe\n");
      state = STATE_PUBLISHING;
    //}
    /* Continue */
  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if(timer_expired(&connection_life)) {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
      /* Connected. Publish */
      /*if(state == STATE_CONNECTED) {
        subscribe();
        state = STATE_PUBLISHING;
      } else {*/
        leds_on(MQTT_CLIENT_STATUS_LED);
        ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        LOG_DBG("Publishing\n");
        publish("telemetry");
      //}
      etimer_set(&publish_periodic_timer, conf.pub_interval);
      /* Return here so we don't end up rescheduling the timer */
      return;
    } else {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      LOG_DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
              conn.out_queue_full);
    }
    break;
  case STATE_DISCONNECTED:
    LOG_DBG("Disconnected\n");
    if(connect_attempt < RECONNECT_ATTEMPTS ||
       RECONNECT_ATTEMPTS == RETRY_FOREVER) {
      /* Disconnect and backoff */
      clock_time_t interval;
#if MQTT_5
      mqtt_disconnect(&conn, MQTT_PROP_LIST_NONE);
#else
      mqtt_disconnect(&conn);
#endif
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;
    } else {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;
  case STATE_CONFIG_ERROR:
    /* Idle away. The only way out is a new config */
    LOG_ERR("Bad configuration.\n");
    return;
  case STATE_ERROR:
  default:
    //leds_on(MQTT_CLIENT_STATUS_LED);
    /*
     * 'default' should never happen.
     *
     * If we enter here it's because of some error. Stop timers. The only thing
     * that can bring us out is a new config event
     */
    LOG_ERR("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/

static char payload[PAYLOAD_SIZE];

void populate_payload(){
  for (uint8_t i=0; i < PAYLOAD_SIZE; i++){
    payload[i] = 'a';
  }
}

//udp callback function for received packets 
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  sscanf((char *) data, "%d", &EEDelay);
  //char * delay_str = (char *)data ;
  //sscanf(delay_str, "%d", &EEDelay);
  LOG_INFO("Received %d bytes, delay = %d , from %d\n", datalen, EEDelay, sender_addr->u8[15] );
}
/*---------------------------------------------------------------------------*/
//static void send_udp_packet(struct simple_udp_connection udp_conn,unsigned char *data, uint16_t data_len, uip_ipaddr_t *dest_ipaddr){
   /* Write the data to the UIP buffer */
//  memcpy(uip_appdata, data, data_len);

  /* Set the length of the data in the UIP buffer */
//  uip_len = data_len;
  //LOG_INFO("uip_len = %d\n", uip_len);
  /* Send the UDP packet */
//  simple_udp_sendto(&udp_conn,uip_appdata, uip_len, dest_ipaddr);
//}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_client_process, ev, data)
{
  

  PROCESS_BEGIN();

  printf("MQTT Client Process\n");

  if(init_config() != 1) {
    PROCESS_EXIT();
  }

  /* Main loop */
  while(1) {

    PROCESS_YIELD();

    if(ev == button_hal_release_event &&
       ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO) {
      if(state == STATE_ERROR) {
        connect_attempt = 1;
        state = STATE_REGISTERED;
      }
    }

    if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
       ev == PROCESS_EVENT_POLL ||
       (ev == button_hal_release_event &&
        ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO)) {
      state_machine();
    }
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_process, ev, data)
{
  PROCESS_BEGIN();

  //UDP connection
  static struct simple_udp_connection udp_conn;
  static struct etimer data_timer;
  uip_ipaddr_t dst;
  //timer for delay calculation
  rtimer_clock_t t_send = RTIMER_NOW();
  static uint32_t seq_num=0;


  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, udp_rx_callback);

  //wait for connecting to the broker ..
  etimer_set(&data_timer, (60*CLOCK_SECOND));
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&data_timer));

  //populate_payload();
  etimer_set(&data_timer, DATA_SEND_INTERVAL);
  
  LOG_INFO("Started UDP communication, QUEUE_BUF_SIZE = %d \n", QUEUEBUF_NUM);
  if(node_id != 1){ // if its not the root node

      while(1){

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&data_timer));
        if(have_connectivity()){
          //LOG_INFO("Node have connectivity \n");
          //if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst)){
            NETSTACK_ROUTING.get_root_ipaddr(&dst);
            t_send = RTIMER_NOW();
            snprintf(payload, PAYLOAD_SIZE, "%d|%ld", seq_num, t_send);
            LOG_INFO("Sending %d bytes packet to %d, seq_num = %d, current_time = %ld\n", (int) sizeof(payload) ,dst.u8[15], seq_num, t_send);
            //LOG_INFO("Packetbuf_datalen = %d | Packetbuf_remaininglen = %d\n", packetbuf_datalen(), packetbuf_remaininglen());
            //LOG_INFO("UIP_len = %d\n", uip_len);
            
            //LOG_INFO_6ADDR(&dst);
            //LOG_INFO("\n");
            //send_udp_packet(udp_conn, payload, PAYLOAD_SIZE, &dst);
            //LOG_INFO("Queuebuf free packets before send= %d\n",(int)queuebuf_numfree());

            simple_udp_sendto(&udp_conn, payload, PAYLOAD_SIZE, &dst);

            seq_num++;
            //LOG_INFO("Queuebuf free packets after send= %d\n",(int)queuebuf_numfree());
            //bufferState = (int)queuebuf_numfree();
          //}
          //else{
            //LOG_INFO("Not reachable yet\n");
          //}
        }
        else{
          LOG_INFO("Node does not have connectivity\n");
        }
        etimer_set(&data_timer, DATA_SEND_INTERVAL);
    }
  }
  PROCESS_END();

}
/*---------------------------------------------------------------------------*/