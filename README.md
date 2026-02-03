# DCC

<img src="https://github.com/nmradcc/DCC_tester/raw/main/data/images/logo.gif" align="right"/>

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
- [git](https://git-scm.com/downloads)
- [Visual Studio Code](https://code.visualstudio.com/download)
- [STM32Cube for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=stmicroelectronics.stm32-vscode-extension) Can be installed from the VS Code Marketplace
- [Serial Monitor for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode.vscode-serial-monitor) Can be installed from the VS Code Marketplace
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)
NOTE: Please install the STM32CubeMX program with the "Install for all users" option. NOT the "for only me". The for "all user option" insures the STM32CubeMX is available in the correct location to allow the auto build script to work correctly.

### Installation

To clone using HTTPS:
```
git clone https://github.com/nmradcc/DCC_tester.git ./DCC_tester
cd ./DCC_tester
```
After cloning it is important to initialy generate the dependent project driver code before building. (these driver directories are not archived by default!)
Generate the code by launching the STM32CubeMX utility, open the DCC_testor.ioc file and press the GENERATE CODE button.

After code generation you can launch VSC and build.

NOTE: The code is NOT buildable or runable after cloning! You must do an initial GENERATE CODE with the CubeMX utility!
You can also do the generate code step from the command line by using the Windows PowerShell generate_project.ps1 script.
In addition to generating code the PowerShell script also builds the entire project!


### Build

If the VSC STM32Cube extensions are installed correctly it should pickup the top level CMakeLists.txt file when VSC is launched (or the project folder is opened) and automatically configure the project. You can build and debug by clicking the appropriate icon.

**Caution:** STM32CubeMX is a great tool for code generation. With it you can add/delete drivers and it will automatically generate driver initialization code, which can get quite complicated, if done manually.
Its worth maintaining compatability going forward.  
**DO NOT MODIFY** generated files unless you add code only in the "USER" specified code blocks. If you do not adhear to this rule, the next time you regenerate the code your changes will be lost!

Command line builds can be accomplished via the generate_project.ps1 PowerShell script.

Script are developed using Python running on the host PC.
See the how_to_run_scripts.txt in the Scripts folder for more information.
Script development and debug can also be accomplished using the same Visual Studio Code tool.

### Run

The debug USB interface creates a virtual serial serial uart.
You can connect to the UART using the Visual Studio Code serial monitor or you can use an external program like Putty or Termite.
The Command Line Interface (CLI) is available via the virtual com port.
Through the CLI you can start/stop modules like the command station, set variables/parameters etc.
See the CLI help menu for available commands.

More extensive control is offered via an RPC JSON interface implemented on the NUCLEO aux user USB interface.
Plugging in the user USB interface to a host computer will offer up an additional virtual com port.
Through this com port you can exicute any series of commands (scripts) by using simple JSON formatted instructions.
See RPC_TEST_MESSAGES.txt file in the Doc directory for a complete list of presently implemented commands.
Again you can use a simple terminal program, cut and paste individual commands directly from the document into the terminal send window.
Higher level control can be accomplished via Python scripts that can aggregate RPC commands.
See Script folder for more information.

Have fun!


