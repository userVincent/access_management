
# Secure Access Management Solution - Master Node

## Overview

The Master Node is a central component of the Secure Access Management Solution for lab environments. This system is designed to manage and control access to various devices in a laboratory setting through communication with multiple Slave Nodes.

### Master Node Functionality
The base functionality of the Master Node involves communicating with multiple Slave Nodes using the nRF24L01. It is responsible for determining whether a key has access to a device by consulting a local database and subsequently granting or denying access as appropriate.

For non-essential functionality, the Master Node supports Wi-Fi connectivity, enabling remote control, user authentication, and session management. This allows authorized users to manage devices, access levels, and the local database. Additionally, the Master Node offers logging capabilities with accurate timestamps and ensures regular log uploads to a remote database, handling local log deletions after successful transfers.

Beyond these basic functionalities, the Master Node encompasses several design philosophies, including robustness, scalability, security, and task management. The software is designed to gracefully handle errors and edge cases, ensuring reliable operation. It is capable of accommodating an increasing number of Slave Nodes and user interactions without compromising performance. Security measures are implemented to prevent unauthorized access and safeguard sensitive data. Lastly, the Master Node utilizes a multitasking architecture, such as FreeRTOS, to efficiently manage various tasks and processes.

For more detailed information, refer to the [Secure Access Management Solution for Lab Environments](SecureAccessManagementSolutionForLabEnvironments.pdf).

## Hardware and Software Requirements

1. **Hardware Requirements**: ESP32-S3 microcontroller.
2. **Software Requirements**: Refer to Espressif's official guide for [VS Code setup for ESP32](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/get-started/vscode-setup.html).

## Installation

1. Clone or download the Secure Access Management Solution source code.
2. Set up your development environment as per the [Espressif guide](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/get-started/vscode-setup.html).
3. Open the project in your development environment.
4. Connect your ESP32-S3 to your computer.
5. Compile and upload the code to your ESP32-S3.

## Reference

For detailed technical information, design philosophies, and step-by-step instructions, refer to the [Secure Access Management Solution for Lab Environments](SecureAccessManagementSolutionForLabEnvironments.pdf), specifically the Master Node sections.
