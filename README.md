# BLE113BusSim
Testing framework for Bluegiga BLE113 with MSP432. Provides a method to test Bluetooth Low Energy throughput (Bluegiga BLE113) on MSP432 Launchpad boards (TI MSP-EXP432P401R).

### Required hardware
* CC-Debugger http://www.ti.com/tool/cc-debugger
* MSP-EXP432P401R Launchpads http://www.ti.com/tool/msp-exp432p401r
* Bluegiga BLE113 modules

## Required tools and software
* SmartRF Flash Programmer v1 http://www.ti.com/tool/flash-programmer (v1.12.8)
* Bluegiga Bluetooth Smart Software Stack https://www.silabs.com/products/wireless/bluetooth/bluetooth-smart-modules/Pages/bluegiga-bluetooth-smart-software-stack.aspx (v1.4.2)
* TI Code Composer Studio http://processors.wiki.ti.com/index.php/Download_CCS (v6.2.0.00050)
* MSP432 Driver Library http://www.ti.com/tool/mspdriverlib (v.3.21.00.05)

### Random notes from BLE development
* BLE113 uses UART communication. To achieve the highest possible throughput, use at least 1MBaud.
* Sleep mode needs to be used; otherwise the power consumption is large. Wakeup from sleep is acknowledged with an event, handler needs to wait for it.
* Always wait for response after issuing command to BLE113.
* Connections are very stable after being established, but the connection setup itself is not reliable – gives "Connection Failed to be Established (0x023E)". This is documented in Bluegiga API manual. Solution is to send an acknowledged attribute write after connection; this will either proceed or give 0x023E error. In that case, do a reconnect.
* Support for 8 slaves needs to be enabled in both bgproj (config.xml) and CCS project. Minimum connection interval is 20ms for config of up to 8 slaves; this lowers the maximum achievable throughput.
* To use 20ms connection interval with all 8 slaves, it is necessary to start with longer interval (e.g. 100ms), connect all of them, and then issue an update to 20ms. Otherwise the 7th or 8th slave will connect very late or would not connect at all. Connection interval is not updated right after acknowledgement, it takes several communication intervals.
* With 20ms connection interval, the theoretical maximum write throughput for one node is 20B\*(1/(2\*0.02s))=500B/s. This is because the write packet is limited to 20B and each packet is acknowledged. Large values like 21.1kbit/s from our testing are only achievable with optimal payload distribution – maximum packet length to all slaves simultaneously.
* Nice reading to start with BLE:
  * https://bluegiga.zendesk.com/entries/25053373--REFERENCE-BLE-master-slave-GATT-client-server-and-data-RX-TX-basics
  * https://bluegiga.zendesk.com/entries/22412436--REFERENCE-What-is-the-difference-between-BGScript-BGAPI-and-BGLib
  * https://bluegiga.zendesk.com/entries/23173106--REFERENCE-BLE-module-low-power-and-sleep-modes
  * https://bluegiga.zendesk.com/entries/45890933--REFERENCE-BGAPI-BGLib-Implementation-on-BLE-devices
* Module ble.c provides a basic library for MSP432 to support Bluegiga BLE112/113 modules in UART packet mode. Other support files are from Bluegiga bglib SDK.
* Loading the firmware is possible with CC Debugger. Always use Bluegiga BLE SW Update Tool, not the TI Flash Programmer, as the later one will destroy the license key in BLE113.
* BLE is really suited for very slow communication with negligible throughput. It will perform best for e.g. a sensor, which provides up to 20B of data each ~1 second.
