#ifndef _BLE_H
#define _BLE_H

extern bool read_message(void);
extern void ble_sleep(bool en);
extern void ble_rsp_wait(void);
extern void ble_init_hw(void);

#endif
