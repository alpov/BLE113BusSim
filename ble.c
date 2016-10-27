#include <msp.h>
#include <string.h>
#include <stdio.h>
#include <driverlib.h>
#include "cmd_def.h"
#include "ble.h"

#define BLE_WAKE_UP		GPIO_PORT_P2, GPIO_PIN6
#define BLE_RESET		GPIO_PORT_P2, GPIO_PIN7

#define BLE_RX_BUFFER_LEN 256
static uint8_t ble_rx_buf[BLE_RX_BUFFER_LEN];
static uint16_t ble_rx_read_ptr = 0;
static volatile uint16_t ble_rx_write_ptr = 0;
#define ble_rx_count ((ble_rx_write_ptr - ble_rx_read_ptr) & (BLE_RX_BUFFER_LEN-1))

static bool received_wakeup = false;
static bool received_rsp = false;


/* UART Configuration Parameter. These are the configuration parameters to
 * make the eUSCI A UART module to operate with a 1M baud rate. These
 * values were calculated using the online calculator that TI provides
 * at:
 * http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
 */
const eUSCI_UART_Config uartConfig_BLE =
{
	EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
	3,                                       // BRDIV
	0,                                       // UCxBRF
	0,                                       // UCxBRS
	EUSCI_A_UART_NO_PARITY,                  // No Parity
	EUSCI_A_UART_LSB_FIRST,                  // LSB First
	EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
	EUSCI_A_UART_MODE,                       // UART mode
	EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
};


/* EUSCI A2 UART ISR */
void EUSCIA2_IRQHandler(void)
{
    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A2_BASE);

    if (status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG) {
        ble_rx_buf[ble_rx_write_ptr] = MAP_UART_receiveData(EUSCI_A2_BASE);
    	if (++ble_rx_write_ptr >= BLE_RX_BUFFER_LEN) ble_rx_write_ptr = 0;
    }
}


bool read_message(void)
{
    struct ble_header apihdr;
    uint8_t *apihdr_raw = (uint8_t *)(&apihdr);
    uint8_t data[256]; // enough for BLE

	if (ble_rx_count < 4) return false; // no data incl. header available

    // read header
	for (uint8_t i = 0; i < 4; i++) {
		apihdr_raw[i] = ble_rx_buf[ble_rx_read_ptr];
		if (++ble_rx_read_ptr >= BLE_RX_BUFFER_LEN) ble_rx_read_ptr = 0;
	}

    // read rest if needed
	for (uint8_t i = 0; i < apihdr.lolen; i++) {
		do {} while (ble_rx_read_ptr == ble_rx_write_ptr); // receive byte
		data[i] = ble_rx_buf[ble_rx_read_ptr];
		if (++ble_rx_read_ptr >= BLE_RX_BUFFER_LEN) ble_rx_read_ptr = 0;
	}

	// find message descriptor
	const struct ble_msg *apimsg = ble_get_msg_hdr(apihdr);
    if (!apimsg) return false;

    // execute message handler
    apimsg->handler(data);

    // if the message is a command response, set flag
    if ((apimsg->hdr.type_hilen & 0x80) == ble_msg_type_rsp) received_rsp = true;

    return true;
}


void write_message(uint8 len1, uint8* data1, uint16 len2, uint8* data2)
{
	uint8_t len = len1 + len2;
	MAP_UART_transmitData(EUSCI_A2_BASE, len);

	while (len1) {
		MAP_UART_transmitData(EUSCI_A2_BASE, *data1++);
		len1--;
	}

	while (len2) {
		MAP_UART_transmitData(EUSCI_A2_BASE, *data2++);
		len2--;
	}
}


void ble_sleep(bool en)
{
	static bool en_old = true;

	// no change?
	if (en == en_old) return;

	if (en) {
	    MAP_GPIO_setOutputLowOnPin(BLE_WAKE_UP); // wake up low
	} else {
		received_wakeup = false;
		MAP_GPIO_setOutputHighOnPin(BLE_WAKE_UP); // wake up high
	    do read_message(); while (!received_wakeup); // wait for wakeup ack
	}
	en_old = en;
}


void ble_rsp_wait(void)
{
	received_rsp = false;
	do read_message(); while (!received_rsp); // wait for ble_rsp_*
}


void ble_evt_hardware_io_port_status(const struct ble_msg_hardware_io_port_status_evt_t *msg)
{
	received_wakeup = true;
}


void ble_init_hw(void)
{
    /* Selecting P1.2 and P1.3 in UART mode */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3, GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    /* Configuring UART Module */
    MAP_UART_initModule(EUSCI_A2_BASE, &uartConfig_BLE);

    /* Enable UART module */
    MAP_UART_enableModule(EUSCI_A2_BASE);

    /* Enabling interrupts */
    MAP_UART_enableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
    MAP_Interrupt_enableInterrupt(INT_EUSCIA2);

    // setup BGLib write handler
    bglib_output = write_message;

    // wakeup BLE module
    MAP_GPIO_setAsOutputPin(BLE_WAKE_UP);
    ble_sleep(false);

    // hardware reset for BLE module
    MAP_GPIO_setAsOutputPin(BLE_RESET);
    MAP_GPIO_setOutputLowOnPin(BLE_RESET);
    for (int i = 0; i < 480000; i++) {} // wait
    MAP_GPIO_setOutputHighOnPin(BLE_RESET);
    do {} while (!read_message()); // process response
}

