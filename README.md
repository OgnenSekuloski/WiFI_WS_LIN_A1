## Overview

This project is my bachelor's thesis at the Faculty of Electrical Engineering and Information Technologies, UKIM in Skopje. It was suggested as a project from Magna Auteca. It implements a system for remote control and testing of a linear automotive actuator using a PSoC 6 microcontroller.

The main goal is to extend an existing LIN-based actuator control solution (originally operated via physical buttons) into a wireless, web-controlled system. The result is a portable setup where the actuator can be controlled from any device with a web browser, without requiring additional software.

The system combines embedded development, wireless networking, web technologies and automotive communication into a single integrated solution. It is designed not only to be functional, but also easy to demonstrate and use in real-world scenarios.

---

## Key Features

- Web-based actuator control through a standard browser interface  
- Wi-Fi connectivity with SoftAP (setup) and STA (operation) modes  
- Automatic reconnection to previously saved networks  
- mDNS support for access via hostname instead of IP  
- UART-based LIN communication with full frame handling  
- Non-blocking architecture based on a state machine  
- Persistent Wi-Fi storage using internal flash memory  
- OLED display for runtime system information  

---

## System Architecture

The system is divided into two main layers:

- Network layer – responsible for Wi-Fi connection and HTTP communication  
- Control layer – responsible for LIN communication and actuator logic  

High-level data flow:

```

Web Browser
↓ HTTP (GET / POST / SSE)
PSoC 6 Web Server
↓ Command interface
LIN State Machine
↓ UART
LIN Transceiver
↓ LIN Bus
Actuator

```

The browser sends commands to the PSoC, which translates them into LIN frames. The actuator responds with status information, which is then propagated back to the user interface.

---

## Hardware

The system is built using the following components:

- PSoC 62S2 Wi-Fi BT Pioneer Kit  
- LIN Transceiver (TLE7259-2GE)  
- Linear automotive actuator  
- OLED display (SSD1306 over I2C)  

The PSoC acts as the central controller, handling both networking and actuator communication.

---

## Software Stack

- ModusToolbox  
- FreeRTOS  
- lwIP (network stack)  
- cy_wcm (Wi-Fi connection manager)  
- cy_http_server (HTTP server)  
- PDL / HAL drivers  

This project is based on Infineon’s Wi-Fi web server example, which was significantly adapted and extended.

Original template repository:  
https://github.com/Infineon/mtb-example-wifi-web-server

The original example demonstrates Wi-Fi provisioning and basic web control of an LED. In this project, that functionality has been replaced with a full actuator control system over LIN.

---

## Web Interface

The web interface is the primary interaction point for the user.

### Modes of operation

SoftAP mode:
- Device creates its own Wi-Fi network  
- Used for initial configuration  
- User selects and enters Wi-Fi credentials  

STA mode:
- Device connects to an existing Wi-Fi network  
- Hosts the actuator control interface  
- Used during normal operation  

### Communication model

- HTTP GET – loading pages and establishing connections  
- HTTP POST – sending commands to the actuator  
- Server-Sent Events (SSE) – streaming real-time status updates  

---

## LIN Communication

Communication with the actuator is implemented using LIN over UART.

- Baudrate: 19200  
- Frame structure:
```

[Break] [0x55] [PID] [Data (8 bytes)] [Checksum]

```

Typical commands:
- 0xF0 – Open  
- 0x00 – Close  
- 0xF1 / 0x01 – Calibration  

Status frames are requested separately and parsed to determine actuator state and calibration progress.

---

## State Machine Design

The system uses a non-blocking state machine to ensure stable operation.

Main states include:
- IDLE  
- CALIBRATING  
- OPENING  
- CLOSING  
- READY  
- ERROR  

LIN communication is executed periodically (approximately every 100 ms), allowing continuous updates and proper coexistence with the networking stack.

This replaces the original blocking implementation and enables responsive behavior.

---

## Testing and Debugging

Testing was performed in multiple stages:

- UART logging (115200 baud) for monitoring system behavior  
- Simulation mode for testing without hardware:
```

#define LIN_ACTUATOR_ENABLE_SIMULATION (1u)

```
- Oscilloscope measurements to validate LIN signal integrity  

### Important fix

An issue with unstable LIN communication was traced to FreeRTOS power-saving behavior.

The fix was:
```

#define configUSE_TICKLESS_IDLE 0

```

Disabling tickless idle removed unintended disturbances on the UART line and stabilized communication.

---

## Project Background

This project builds upon an earlier implementation where actuator control was performed using physical buttons.

The current system introduces:
- wireless control  
- web-based interface  
- modular and non-blocking architecture  

This transforms the original prototype into a more practical and demonstrable system.

---

## Future Improvements

Possible future extensions include:

- Custom PCB design to replace prototype wiring  
- REST API layer for better extensibility  
- Support for multiple actuators  
- Mobile application interface  
- Remote (internet-based) access  

---

## Author

Ognen Sekuloski  
Faculty of Electrical Engineering and Information Technologies (FEEIT)  
Ss. Cyril and Methodius University – Skopje  
