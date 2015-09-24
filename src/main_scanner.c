// BGAPI を用いたサンプルコード
// BLE アドバタイズパケットをスキャンするプログラム
// by bunji2

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <windows.h>
#include <time.h>

#include "cmd_def.h"
#include "uart.h"

//#define DEBUG

#define CLARG_PORT 1
#define CLARG_ACTION 2

#define UART_TIMEOUT 1000

/*
#define MAX_DEVICES 64
int found_devices_count = 0;
bd_addr found_devices[MAX_DEVICES];
*/

enum actions {
  action_none,
  action_scan,
//  action_connect,
  action_info,
};
enum actions action = action_none;

typedef enum {
  state_disconnected,
//  state_connecting,
//  state_connected,
  state_finish,
  state_last
} states;

states state = state_disconnected;

const char *state_names[state_last] = {
  "disconnected",
//  "connecting",
//  "connected",
  "finish"
};

bd_addr connect_addr;

void usage(char *exe)
{
  printf("%s <list|COMx> <info|scan>\n", exe);
}

void change_state(states new_state)
{
#ifdef DEBUG
  printf("DEBUG: State changed: %s --> %s\n",
    state_names[state], state_names[new_state]);
#endif
  state = new_state;
}

void print_bdaddr(bd_addr bdaddr)
{
  printf("%02x:%02x:%02x:%02x:%02x:%02x",
      bdaddr.addr[5],
      bdaddr.addr[4],
      bdaddr.addr[3],
      bdaddr.addr[2],
      bdaddr.addr[1],
      bdaddr.addr[0]);
}

#ifdef DEBUG
void print_raw_packet(struct ble_header *hdr, unsigned char *data)
{
  int i;
  printf("Incoming packet: ");
  for (i = 0; i < sizeof(*hdr); i++) {
    printf("%02x ", ((unsigned char *)hdr)[i]);
  }
  for (i = 0; i < hdr->lolen; i++) {
    printf("%02x ", data[i]);
  }
  printf("\n");
}
#endif

void print_hex(uint8 *data, uint8 len) {
  uint8 i;
  
  for (i = 0; i < len; i++) {
    printf("%02x", data[i]);
  }
}

void output(uint8 len1, uint8* data1, uint16 len2, uint8* data2)
{
  if (uart_tx(len1, data1) || uart_tx(len2, data2)) {
    printf("ERROR: Writing to serial port failed\n");
    exit(1);
  }
}

int read_message(int timeout_ms)
{
  unsigned char data[256]; // enough for BLE
  struct ble_header hdr;
  int r;
  const struct ble_msg *msg;

  r = uart_rx(sizeof(hdr), (unsigned char *)&hdr, timeout_ms);
  if (!r) {
    return -1; // timeout
  }
  else if (r < 0) {
    printf("ERROR: Reading header failed. Error code:%d\n", r);
    return 1;
  }

  if (hdr.lolen) {
    r = uart_rx(hdr.lolen, data, timeout_ms);
    if (r <= 0) {
      printf("ERROR: Reading data failed. Error code:%d\n", r);
      return 1;
    }
  }

  msg = (const struct ble_msg *)ble_get_msg_hdr(hdr);

#ifdef DEBUG
  print_raw_packet(&hdr, data);
#endif

  if (!msg) {
    printf("ERROR: Unknown message received\n");
    exit(1);
  }

  msg->handler(data);

  return 0;
}

typedef struct {
  uint8 flags;
  uint8 ibeacon; // 0:no, 1:yes
  char name[256]; // adtype=0x08,0x09
  uint8 services[256]; // adtype=0x02-0x07
  uint8 services_len;
  uint8 services_type;
  uint8 uuid[16];
  uint16 major;
  uint16 minor;
  int8 txpower; // adtype=0x0a
} adv_pkt;

adv_pkt ap;

// ibeaconパケットのパース
void parse_packet_ibeacon(uint8 *p, uint8 len) {
  int i;
//printf("#parse_packet_ibeacon\n");
  if ((p[0]+p[1]*256)!=0x4C) {
//printf("#not Apple(0x004C)\n");
    return; // not Apple(0x004C)
  }
  if (p[2] != 0x02) {
//printf("#not iBecon(0x02)\n");
    return; // not iBecon(0x02)
  }
  if (p[3] < 0x15) {
//printf("#not enough size\n");
    return; // not enough size;
  }
  ap.ibeacon=1;
  for (i=0; i<16; i++) {
    ap.uuid[i]=p[4+i];
  }
  ap.major = p[20]*256+p[21];
  ap.minor = p[22]*256+p[23];
  ap.txpower = (int8)p[24];
}

// アドバタイズパケットのパース
void parse_packet(uint8*data, uint8 datalen) {
  int i;
  uint8 type;
  uint8 len;
  
//printf("#parse_packet datalen=%d\n", datalen);
  for (i = 0; i < datalen; ) {
//printf("p.data[i]=%02x\n", data[i]);
    len = data[i++];
//printf("len=%d\n", len);
    if (!len) {
      continue;
    }
    if (i + len > datalen) {
      break; // not enough data
    }
    type = data[i++];
//printf("type=%02x len=%d\n", type, len);
    switch (type) {
    case 0x01: // AD Type == Flags
      ap.flags = data[i];
      break;
    case 0x02: // AD Type == Incomplete List of 16-bit Service Class UUIDs
    case 0x03: // AD Type == Complete List of 16-bit Service Class UUIDs
    case 0x04: // AD Type == Incomplete List of 32-bit Service Class UUIDs
    case 0x05: // AD Type == Complete List of 32-bit Service Class UUIDs
    case 0x06: // AD Type == Incomplete List of 128-bit Service Class UUIDs
    case 0x07: // AD Type == Complete List of 128-bit Service Class UUIDs
      if (len-1 < 255) {
        ap.services_type = type;
        ap.services_len = len-1;
        memcpy(ap.services, data +i, len - 1);
      }
      break;
    case 0x08: // AD Type == Shortened Local Name
    case 0x09: // AD Type == Complete Local Name
      if (len-1 < 255) {
        memcpy(ap.name, data + i, len - 1);
      }
      //printf("#name:%s\n", ap.name);
      break;
    case 0x0a: // AD Type == Tx Power Level
      ap.txpower = (int8)data[i];
      break;
    case 0xff: // AD Type == Manufacturer Specific Data
      parse_packet_ibeacon(data+i, len-1);
      break;
    }

    i += len - 1;
  }
}

void print_now() {
/*
  char buf[80];
  time_t tp = time(NULL);
  strftime(buf, 80, "%Y/%m/%d %H:%M:%S", localtime(&tp));
  printf("%s", buf);
*/
  char szTime[25] = { 0 };
  SYSTEMTIME st;
/*
  GetSystemTime(&st);
  // wHourを９時間足して、日本時間にする
  wsprintf(szTime, "%04d/%02d/%02d %02d:%02d:%02d.%03d",
       st.wYear, st.wMonth, st.wDay+(()),
       st.wHour + 9, st.wMinute, st.wSecond, st.wMilliseconds);
  // wHour==15だと、wHour+9>=24 になることがあり、いけてません。
*/
  GetLocalTime(&st);
  wsprintf(szTime, "%04d/%02d/%02d %02d:%02d:%02d.%03d",
       st.wYear, st.wMonth, st.wDay,
       st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  printf("%s", szTime);
}

void print_flags() {
  printf(",Flags:%u", ap.flags);
}

void print_services() {
  int i;
  
//  printf(",Services_type:%u", ap.services_type);
  printf(",Services:");
  switch (ap.services_type) {
    case 0x02: // AD Type == Incomplete List of 16-bit Service Class UUIDs
    case 0x03: // AD Type == Complete List of 16-bit Service Class UUIDs
      printf("[");
      for(i=0; i<ap.services_len; i+=2) {
        printf("0x%02x%02x,", ap.services[i+1], ap.services[i]);
      }
      printf("]");
      break;
    case 0x04: // AD Type == Incomplete List of 32-bit Service Class UUIDs
    case 0x05: // AD Type == Complete List of 32-bit Service Class UUIDs
      printf("[");
      for(i=0; i<ap.services_len; i+=4) {
        printf("0x%02x%02x%02x%02x,", ap.services[i+3], ap.services[i+2],
          ap.services[i+1], ap.services[i]);
      }
      printf("]");
      break;
    
    case 0x06: // AD Type == Incomplete List of 128-bit Service Class UUIDs
    case 0x07: // AD Type == Complete List of 128-bit Service Class UUIDs
      printf("[");
      for(i=0; i<ap.services_len; i+=16) {
        print_hex(ap.services+i, 16);
        printf(",");
      }
      printf("]");
    default:
      print_hex(ap.services, ap.services_len);
  }

}

// [1] スキャンしたアドバタイズパケットの取得
void ble_evt_gap_scan_response(
  const struct ble_msg_gap_scan_response_evt_t *msg
){

/*

  if (found_devices_count >= MAX_DEVICES) {
    change_state(state_finish);
  }

  // Check if this device already found
  for (i = 0; i < found_devices_count; i++) {
    if (!cmp_bdaddr(msg->sender, found_devices[i])) return;
  }
  found_devices_count++;
  memcpy(found_devices[i].addr, msg->sender.addr, sizeof(bd_addr));
*/

  // パケットデータ全体のダンプ
//  printf("#packet:");
//  print_hex(msg->data.data, msg->data.len);
//  printf("\n");
  
  // Parse data
  memset(&ap, 0, sizeof(adv_pkt));

  parse_packet(msg->data.data, msg->data.len);

  // 時刻
  print_now();
  // アドレス
  printf(",");
  print_bdaddr(msg->sender);
  // RSSI
  printf(",RSSI:%d", msg->rssi);
  print_flags();
  if (ap.name[0] != 0) {
    printf(",Name:%s", ap.name);
  }
  if (ap.ibeacon) {
    printf(",iBeacon");
    printf (",UUID:");
    print_hex(ap.uuid, 16);
    printf (",major:%d,minor:%d,txpower:%d", ap.major, ap.minor, ap.txpower);
  }
  if (ap.services_len != 0) {
    print_services();
  }
#ifdef DEBUG
  // パケットデータ全体のダンプ
  printf(",packet:");
  print_hex(msg->data.data, msg->data.len);
#endif
  printf("\n");
}

// [2] デバイスの情報表示の取得
void ble_rsp_system_get_info(
  const struct ble_msg_system_get_info_rsp_t *msg
){
//  printf("#ble_rsp_system_get_info\n");
  printf("major=%u, minor=%u, ", msg->major, msg->minor);
  printf("patch=%u, ", msg->patch);
  printf("build=%u, ", msg->build);
  printf("ll_version=%u, ", msg->ll_version);
  printf("protocol_version=%u, ", msg->protocol_version);
  printf("hw=%u\n", msg->hw);
  if (action == action_info) {
    change_state(state_finish);
  }
}


// ここから、このコードでは使用していない関数
void ble_evt_connection_status(
  const struct ble_msg_connection_status_evt_t *msg
){
  printf("#ble_evt_connection_status [%s]\n", state_names[state]);
}

void ble_evt_attclient_group_found(
  const struct ble_msg_attclient_group_found_evt_t *msg
){
  uint16 uuid;
  
  printf("#ble_evt_attclient_group_found [%s]\n", state_names[state]);

  if (msg->uuid.len == 0) return;
  uuid = (msg->uuid.data[1] << 8) | msg->uuid.data[0];

  printf("service=0x%04x, handles=%d-%d\n", uuid, msg->start, msg->end);

}

void ble_evt_attclient_procedure_completed(
  const struct ble_msg_attclient_procedure_completed_evt_t *msg
){
  printf("#ble_evt_attclient_procedure_completed [%s]\n",
    state_names[state]);

}

void ble_evt_attclient_find_information_found(
  const struct ble_msg_attclient_find_information_found_evt_t *msg
){
  printf("#ble_evt_attclient_find_information_found [%s]\n",
    state_names[state]);

}

void ble_evt_attclient_attribute_value(
  const struct ble_msg_attclient_attribute_value_evt_t *msg
){
  printf("#ble_evt_attclient_attribute_value [%s]\n",
    state_names[state]);
}

void ble_evt_connection_disconnected(
  const struct ble_msg_connection_disconnected_evt_t *msg
) {
  printf("Connection terminated, trying to reconnect\n");
}

// ここまで、このコードでは使用していない関数

// Ctrl-C 押下でスキャンを終了させる
static void handler(int sig)
{
  if (sig == SIGINT) {
    printf("Ctrl-C!\n");
    change_state(state_finish);
    if (action == action_scan) {
      ble_cmd_gap_end_procedure();
    }
  }
}

int main(int argc, char *argv[]) {
  char *uart_port = "";

  // シグナルハンドラの登録
  if (signal(SIGINT, handler) == SIG_ERR)
    return 1;
  
  // 引数が不足しているかのチェック
  if (argc <= CLARG_PORT) {
    usage(argv[0]);
    return 1;
  }

  // COM ポート引数のチェック
  if (argc > CLARG_PORT) {
    if (strcmp(argv[CLARG_PORT], "list") == 0) {
      uart_list_devices(); // デバイスのリスト表示
      return 1;
    }
    else {
      uart_port = argv[CLARG_PORT];
    }
  }

  // アクション引数のチェック
  if (argc > CLARG_ACTION) {
    int i;
    for (i = 0; i < strlen(argv[CLARG_ACTION]); i++) {
      // 小文字にしておく
      argv[CLARG_ACTION][i] = tolower(argv[CLARG_ACTION][i]);
    }

    if (strcmp(argv[CLARG_ACTION], "scan") == 0) {
      action = action_scan;    // スキャン
    } else if (strcmp(argv[CLARG_ACTION], "info") == 0) {
      action = action_info;    // デバイス情報の表示
    } else {
      
    }
  }
  if (action == action_none) {
    usage(argv[0]);
    return 1;
  }

  bglib_output = output;

  // COMポートのオープン
  if (uart_open(uart_port)) {
    printf("ERROR: Unable to open serial port\n");
    return 1;
  }

  // BLED のリセット
  ble_cmd_system_reset(0);
  uart_close();
  do {
//    usleep(500000); // 0.5s
    Sleep(500); // 0.5s
  } while (uart_open(uart_port));

  // アクションの実行
  if (action == action_scan) {

    // [1] スキャンの開始
    ble_cmd_gap_discover(gap_discover_observation);

  } else if (action == action_info) {

    // [2] デバイスの情報表示の要求
    ble_cmd_system_get_info();
  } else {
    return 1;
  }

  // メッセージループ
  while (state != state_finish) {
    if (read_message(UART_TIMEOUT) > 0) break;
  }

  // COMポートのクローズ
  uart_close();

  return 0;
}
