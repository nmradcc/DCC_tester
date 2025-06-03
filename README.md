# DCC

<img src="https://github.com/ZIMO-Elektronik/DCC/raw/master/data/images/logo.gif" align="right"/>

DCC is an acronym for [Digital Command Control](https://en.wikipedia.org/wiki/Digital_Command_Control), a standardized protocol for controlling digital model railways. This library contains code to test DCC decoders for compliance to the NMRA standards. Test software runs on an STMicrosystems STM32H5xx Nucleo development board with an additional custom hardware daughterboard.


<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#getting-started">Getting Started</a></li>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
        <li><a href="#build">Build</a></li>
      </ul>
  </ol>
</details>

## Protocol
The DCC protocol is defined by various standards published by the [National Model Railroad Association (NMRA)](https://www.nmra.org/) and the [RailCommunity](https://www.vhdm.at/). The standards are mostly consistent and match the English and German standards in the table below.
| NMRA (English)                                                                                                                                                                            | RailCommunity (German)                                                                                                   |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| [S-9.1 Electrical Standards for Digital Command Control](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.1_electrical_standards_for_digital_command_control_2021.pdf) | [RCN-210 DCC - Protokoll Bit - Übertragung](https://normen.railcommunity.de/RCN-210.pdf)                                 |
| [S-9.2 Communications Standards For Digital Command Control, All Scales](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-92-2004-07.pdf)                                | [RCN-211 DCC - Protokoll Paketstruktur, Adressbereiche und globale Befehle](https://normen.railcommunity.de/RCN-211.pdf) |
| [S-9.2.1 DCC Extended Packet Formats](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.2.1_dcc_extended_packet_formats.pdf)                                            | [RCN-212 DCC - Protokoll Betriebsbefehle für Fahrzeugdecoder](https://normen.railcommunity.de/RCN-212.pdf)               |
| [S-9.2.1 DCC Extended Packet Formats](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.2.1_dcc_extended_packet_formats.pdf)                                            | [RCN-213 DCC - Protokoll Betriebsbefehle für Zubehördecoder](https://normen.railcommunity.de/RCN-213.pdf)                |
| [S-9.2.1 DCC Extended Packet Formats](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.2.1_dcc_extended_packet_formats.pdf)                                            | [RCN-214 DCC - Protokoll Konfigurationsbefehle](https://normen.railcommunity.de/RCN-214.pdf)                             |
| [S-9.2.3 Service Mode For Digital Command Control, All Scales](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/S-9.2.3_2012_07.pdf)                                       | [RCN-216 DCC - Protokoll Programmierumgebung](https://normen.railcommunity.de/RCN-216.pdf)                               |
| [S-9.3.2 Communications Standard for Digital Command Control Basic Decoder Transmission](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/S-9.3.2_2012_12_10.pdf)          | [RCN-217 RailCom DCC-Rückmeldeprotokol](https://normen.railcommunity.de/RCN-217.pdf)                                     |
| [S-9.2.1.1 Advanced Extended Packet Formats](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.2.1.1_advanced_extended_packet_formats.pdf)                              | [RCN-218 DCC - Protokoll DCC-A - Automatische Anmeldung](https://normen.railcommunity.de/RCN-218.pdf)                    |
| [S-9.2.2 Configuration Variables For Digital Command Control, All Scales](https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.2.2_decoder_cvs_2012.07.pdf)                | [RCN-225 DCC - Protokoll Konfigurationsvariablen](https://normen.railcommunity.de/RCN-225.pdf)                           |

## Getting Started
### Prerequisites
- [Visual Studio Code](https://code.visualstudio.com/download)
- [STM32Cube for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=stmicroelectronics.stm32-vscode-extension) Can be installed from the VS Code marketplace
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)

### Installation

This repository uses [Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) to bring in dependent components.

Note: If you download the ZIP file provided by GitHub UI, you will not get the contents of the submodules. (The ZIP file is also not a valid Git repository)

To clone using HTTPS:
```
git clone --recursive https://github.com/nmradcc/DCC_tester.git ./DCC_tester
cd ./DCC_tester
```

This library is meant to be consumed with CMake,

### Build

If the VSC STM32Cube extension is installed correctly it should pickup the top level CMakeLists.txt file when VSC is launched (or the projevt folder is opened) and automatically configure the project. You can build and debug by clicking the appropriate icon. 

