#include "ESP8266WiFi.h"
#include <SPI.h>
#include "SdFat.h"
extern "C" {
#include "c_types.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "smartconfig.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "user_config.h"
}

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
#define CHANNEL_HOP_INTERVAL   5000

//os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static os_timer_t channelHop_timer;

//static void loop(os_event_t *events);
static void promisc_cb(uint8 *buf, uint16 len);

#define printmac(buf, i) Serial.print(buf[i+0],HEX); Serial.print(":"); Serial.print(buf[i+1],HEX); Serial.print(":"); Serial.print(buf[i+2],HEX); Serial.print(":"); Serial.print(buf[i+3],HEX); Serial.print(":"); Serial.print(buf[i+4],HEX); Serial.print(":"); Serial.print(buf[i+5],HEX);

/* ==============================================
   Promiscous callback structures, see ESP manual
   ============================================== */

struct RxControl {
  signed rssi: 8;
  unsigned rate: 4;
  unsigned is_group: 1;
  unsigned: 1;
  unsigned sig_mode: 2;
  unsigned legacy_length: 12;
  unsigned damatch0: 1;
  unsigned damatch1: 1;
  unsigned bssidmatch0: 1;
  unsigned bssidmatch1: 1;
  unsigned MCS: 7;
  unsigned CWB: 1;
  unsigned HT_length: 16;
  unsigned Smoothing: 1;
  unsigned Not_Sounding: 1;
  unsigned: 1;
  unsigned Aggregation: 1;
  unsigned STBC: 2;
  unsigned FEC_CODING: 1;
  unsigned SGI: 1;
  unsigned rxend_state: 8;
  unsigned ampdu_cnt: 8;
  unsigned channel: 4;
  unsigned: 12;
};

struct LenSeq {
  uint16_t length;
  uint16_t seq;
  uint8_t  address3[6];
};

struct sniffer_buf {
  struct RxControl rx_ctrl;
  uint8_t buf[36];
  uint16_t cnt;
  struct LenSeq lenseq[1];
};

struct sniffer_buf2 {
  struct RxControl rx_ctrl;
  uint8_t buf[112];
  uint16_t cnt;
  uint16_t len;
};

const uint8_t SD_CHIP_SELECT = 4;

const int8_t DISABLE_CHIP_SELECT = -1;
SdFat sd;

void PrintHex83(uint8_t *data, uint8_t start, uint8_t length) // prints 8-bit data in hex
{
  char tmp[length * 2 + 1];
  byte first;
  int j = 0;
  for (uint8_t i = 0; i < length; i++)
  {
    first = (data[i + start] >> 4) | 48;
    if (first > 57) tmp[j] = first + (byte)39;
    else tmp[j] = first ;
    j++;

    first = (data[i + start] & 0x0F) | 48;
    if (first > 57) tmp[j] = first + (byte)39;
    else tmp[j] = first;
    j++;
  }
  tmp[length * 2] = 0;
  Serial.print(tmp);
}
// Function for printing the MAC address i of the MAC header
void printMAC(uint8_t *buf, uint8 i)
{
  Serial.printf("\t%02X:%02X:%02X:%02X:%02X:%02X", buf[i + 0], buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5]);
}

void channelHop(void *arg)
{
  // 1 - 13 channel hopping
  // uint8 new_channel = wifi_get_channel() % 12 + 1;
  uint8 new_channel = 11;
  // Serial.print("** hop to ");
  // Serial.println(new_channel);
  wifi_set_channel(new_channel);
}

static void ICACHE_FLASH_ATTR
promisc_cb(uint8 *buf, uint16 len)
{
  uint8_t* buffi;
  int rssi;
  if ((len == 12)) {
    return; // Nothing to do for this package, see Espressif SDK documentation.
  }
  if (len == 12) {
    return;
  }
  else if (len == 128) {
    struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
    buffi = sniffer->buf;
    rssi  = sniffer->rx_ctrl.rssi;
  }
  else {
    struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
    buffi = sniffer->buf;
    rssi  = sniffer->rx_ctrl.rssi;
  }
  if ((bitRead(buffi[0], 3) == 1) && (bitRead(buffi[0], 2) == 0)) {
    Serial.printf("Channel %3d: Package Length %d", wifi_get_channel(), len);
    printMAC(buffi,  4); // Print address 1
    printMAC(buffi, 10); // Print address 2
    printMAC(buffi, 16); // Print address 3
    if ((bitRead(buffi[1], 7) == 1) && (bitRead(buffi[1], 6) == 1)) printMAC(buffi, 24); // Print address 4
    // if ((bitRead(buffi[0], 7) == 1) && (bitRead(buffi[0], 6) == 0)){
      Serial.print("\t");
      Serial.print(rssi);
      Serial.print("\t");
      Serial.print(bitRead(buffi[0], 3));
      Serial.print(bitRead(buffi[0], 2));
      Serial.print(bitRead(buffi[0], 7));
      Serial.print(bitRead(buffi[0], 6));
      Serial.print(bitRead(buffi[0], 5));
      Serial.print(bitRead(buffi[0], 4));
      Serial.printf("\n");
    // }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delayMicroseconds(100);

  Serial.println("*** Monitor mode test ***");

  Serial.print(" -> Promisc mode setup ... ");
  wifi_set_promiscuous_rx_cb(promisc_cb);
  wifi_promiscuous_enable(1);
  Serial.println("done.");
  //  char ap_mac[6] = {0x48, 0xd2, 0x24, 0x77, 0x88, 0x21};
  //  char ap_mac[6] = {0x16, 0x34, 0x56, 0x78, 0x90, 0xab};
  //  wifi_promiscuous_set_mac(ap_mac);
  Serial.print(" -> Timer setup ... ");
  os_timer_disarm(&channelHop_timer);
  os_timer_setfn(&channelHop_timer, (os_timer_func_t *) channelHop, NULL);
  os_timer_arm(&channelHop_timer, CHANNEL_HOP_INTERVAL, 1);
  Serial.println("done.\n");

  Serial.print(" -> Set opmode ... ");
  wifi_set_opmode( 0x1 );
  Serial.println("done.");

  //Start os task
  //system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);

  Serial.println(" -> Init finished!");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10);
}
