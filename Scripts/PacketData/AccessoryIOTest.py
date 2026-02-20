#!/usr/bin/env python3
"""
AccessoryIOTest Script
======================

This script tests inter-packet delay timing for accessory IO control.
"""

import json
import serial
import time
from datetime import datetime


LOG_LEVEL = 1  # 0 = none, 1 = minimum, 2 = verbose


def set_log_level(level):
    """Set global logging level (0=none, 1=minimum, 2=verbose)."""
    global LOG_LEVEL
    try:
        level_int = int(level)
    except (TypeError, ValueError):
        level_int = 1
    LOG_LEVEL = max(0, min(2, level_int))


def log(level, message):
    if LOG_LEVEL >= level:
        if LOG_LEVEL == 2:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] {message}")
        else:
            print(message)


class DCCTesterRPC:
    """RPC client for DCC_tester command station."""

    def __init__(self, port, baudrate=115200, timeout=2):
        """
        Initialize RPC client.

        Args:
            port: Serial port (e.g., 'COM3' on Windows or '/dev/ttyACM0' on Linux)
            baudrate: Serial baud rate (default: 115200)
            timeout: Serial timeout in seconds (default: 2)
        """
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.5)  # Allow time for connection to establish

    def send_rpc(self, method, params):
        """
        Send an RPC request and return the response.

        Args:
            method: RPC method name
            params: Dictionary of parameters

        Returns:
            Response dictionary
        """
        request = {
            "method": method,
            "params": params
        }

        request_json = json.dumps(request) + '\r\n'
        log(2, f"→ {request_json.strip()}")

        self.ser.write(request_json.encode('utf-8'))

        # Read response
        response_line = self.ser.readline().decode('utf-8').strip()
        log(2, f"← {response_line}")

        if response_line:
            return json.loads(response_line)
        return None

    def close(self):
        """Close serial connection."""
        self.ser.close()


def calculate_dcc_checksum(bytes_list):
    """
    Calculate DCC packet checksum (XOR of all bytes).

    Args:
        bytes_list: List of packet bytes

    Returns:
        Checksum byte
    """
    checksum = 0
    for byte in bytes_list:
        checksum ^= byte
    return checksum


def _validate_aux_params(address, aux_number):
    if not 1 <= aux_number <= 4:
        raise ValueError("aux_number must be between 1 and 4")
    if not 1 <= address <= 511:
        raise ValueError("address must be between 1 and 511 for basic accessory packets")


def _make_basic_accessory_packet(address, aux_number, activate):
    """
    Create a basic accessory decoder packet (NMRA S-9.2.1).

    address: accessory decoder address (1-511)
    aux_number: output 1-4
    activate: True for ON/closed, False for OFF/thrown
    """
    _validate_aux_params(address, aux_number)

    # NMRA basic accessory packet:
    # Byte 1: 10AAAAAA (A0-A5)
    # Byte 2: 1AAACDDD (A6-A8, C=activate, DDD=output 0-7)
    # Output 1-4 encoded as 0-3.
    addr = address - 1
    output = aux_number - 1
    byte1 = 0x80 | (addr & 0x3F)
    byte2 = 0x80 | (((addr >> 6) & 0x07) << 4) | ((1 if activate else 0) << 3) | (output & 0x07)
    checksum = calculate_dcc_checksum([byte1, byte2])
    packet = [byte1, byte2, checksum]

    log(2, f"Accessory packet for address {address}, aux {aux_number} ({'ON' if activate else 'OFF'}):")
    log(2, f"  Bytes: {' '.join(f'0x{b:02X}' for b in packet)}")
    log(2, "  Binary breakdown:")
    log(2, f"    Address bits:  {addr:09b} (address {address})")
    log(2, f"    Byte 1:        0x{byte1:02X}")
    log(2, f"    Byte 2:        0x{byte2:02X} (activate={'ON' if activate else 'OFF'}, output={aux_number})")
    log(2, f"    Checksum:      0x{checksum:02X}\n")

    return packet


def make_aux_on_packet(address, aux_number):
    """Create an Aux ON packet for a given auxiliary number."""
    return _make_basic_accessory_packet(address, aux_number, activate=True)


def make_aux_off_packet(address, aux_number):
    """Create an Aux OFF packet for a given auxiliary number."""
    return _make_basic_accessory_packet(address, aux_number, activate=False)


def read_aux_io_state(rpc, aux_number):
    """
    Read the IO state for a given auxiliary number.

    Aux 1 reads IO1, Aux 2 reads IO2, etc.
    Returns True if HIGH, False if LOW, or None on error.
    """
    response = rpc.send_rpc("get_gpio_inputs", {})
    if response is None or response.get("status") != "ok":
        log(1, f"ERROR: Failed to read GPIO inputs: {response}")
        return None

    gpio_word = response.get("value")
    if gpio_word is None:
        log(1, f"ERROR: Missing GPIO value in response: {response}")
        return None

    bit_index = aux_number - 1
    io_high = (gpio_word & (1 << bit_index)) != 0
    log(2, f"GPIO inputs: 0x{gpio_word:04X} (IO{aux_number}={'HIGH' if io_high else 'LOW'})")
    return io_high


def run_aux_io_test(rpc, loco_address, aux_number, inter_packet_delay_ms=1000, logging_level=1):
    """
    Run the Aux IO test.

    Args:
        rpc: DCCTesterRPC client instance
        loco_address: Locomotive address
        aux_number: Auxiliary number to control
        inter_packet_delay_ms: Delay between packets in milliseconds (default: 1000ms)

    Returns:
        Dictionary with test results including pass/fail status
    """
    set_log_level(logging_level)

    log(2, "=" * 70)
    log(2, "DCC Accessory IO Test")
    log(2, f"Aux number: {aux_number}")
    log(2, f"Inter-packet delay: {inter_packet_delay_ms} ms")
    log(2, "=" * 70)
    log(2, "")

    try:
        # Step 1: Start command station in custom packet mode (loop=0)
        log(1, "Step 1: Starting command station in custom packet mode")
        response = rpc.send_rpc("command_station_start", {"loop": 0})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to start command station: {response}")
            return {"status": "FAIL", "error": "Failed to start command station"}
        log(2, f"✓ Command station started (loop={response.get('loop', 0)})\n")

        time.sleep(0.5)

        # Step 2: Create Aux ON packet
        log(1, f"Step 2: Creating Aux ON packet for Aux {aux_number}...")
        aux_on_packet = make_aux_on_packet(loco_address, aux_number)

        # Step 3: Load and transmit the Aux ON packet
        log(1, "Step 3: Loading and transmitting Aux ON packet...")
        response = rpc.send_rpc("command_station_load_packet", {"bytes": aux_on_packet})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to load Aux ON packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to load Aux ON packet"}

        response = rpc.send_rpc("command_station_transmit_packet",
                               {"count": 1, "delay_ms": 0})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to transmit Aux ON packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to transmit Aux ON packet"}

        # Step 4: Read Aux IO state after ON
        log(1, f"Step 4: Reading IO{aux_number} after Aux ON transmit...")
        aux_on_state = read_aux_io_state(rpc, aux_number)
        if aux_on_state is None:
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read Aux IO state (ON)"}
        aux_on_ok = aux_on_state is True
        log(1, f"✓ Aux ON IO state: {aux_on_ok} (IO{aux_number}={'HIGH' if aux_on_state else 'LOW'})")

        # Step 5: Wait for inter-packet delay
        log(1, f"Step 5: Waiting {inter_packet_delay_ms} ms (inter-packet delay)...")
        time.sleep(inter_packet_delay_ms / 1000.0)
        log(2, "✓ Inter-packet delay complete\n")

        # Step 6: Create Aux OFF packet
        log(1, f"Step 6: Creating Aux OFF packet for Aux {aux_number}...")
        aux_off_packet = make_aux_off_packet(loco_address, aux_number)

        # Step 7: Load and transmit the Aux OFF packet
        log(1, "Step 7: Loading and transmitting Aux OFF packet...")
        response = rpc.send_rpc("command_station_load_packet", {"bytes": aux_off_packet})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to load Aux OFF packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to load Aux OFF packet"}

        response = rpc.send_rpc("command_station_transmit_packet",
                       {"count": 1, "delay_ms": 0})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to transmit Aux OFF packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to transmit Aux OFF packet"}

        # Step 8: Read Aux IO state after OFF
        log(1, f"Step 8: Reading IO{aux_number} after Aux OFF transmit...")
        aux_off_state = read_aux_io_state(rpc, aux_number)
        if aux_off_state is None:
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read Aux IO state (OFF)"}
        aux_off_ok = aux_off_state is False
        log(1, f"✓ Aux OFF IO state: {aux_off_ok} (IO{aux_number}={'HIGH' if aux_off_state else 'LOW'})")

        # Step 9: Stop command station
        log(1, "Step 9: Stopping command station")
        response = rpc.send_rpc("command_station_stop", {})

        if response is None or response.get("status") != "ok":
            log(1, f"WARNING: Failed to stop command station: {response}")
        else:
            log(2, "✓ Command station stopped\n")

        test_pass = aux_on_ok and aux_off_ok

        log(2, "\n" + "=" * 70)
        log(2, "✓ TEST COMPLETE")
        log(2, "=" * 70)
        if test_pass:
            log(2, "✓ TEST PASS")
        else:
            log(2, "✗ TEST FAIL")
        log(2, "=" * 70)
        log(2, "\nTest Parameters:")
        log(2, f"  Locomotive address:    {loco_address}")
        log(2, f"  Aux number:            {aux_number}")
        log(2, f"  Inter-packet delay:    {inter_packet_delay_ms} ms")
        log(2, "\nTest sequence completed:")
        log(2, "  1. Started command station in custom packet mode")
        log(2, f"  2. Created Aux ON packet for Aux {aux_number}")
        log(2, f"  3. Transmitted Aux ON packet to address {loco_address}")
        log(2, f"  4. Read IO{aux_number} after Aux ON: {aux_on_ok}")
        log(2, f"  5. Waited {inter_packet_delay_ms} ms (inter-packet delay)")
        log(2, f"  6. Created Aux OFF packet for Aux {aux_number}")
        log(2, f"  7. Transmitted Aux OFF packet to address {loco_address}")
        log(2, f"  8. Read IO{aux_number} after Aux OFF: {aux_off_ok}")
        log(2, "  9. Stopped command station")
        log(2, "\nIO state measurements:")
        log(2, f"  Aux ON IO match:  {aux_on_ok}")
        log(2, f"  Aux OFF IO match: {aux_off_ok}")
        log(2, "\nPass Criteria:")
        log(2, "  Aux ON read is HIGH and Aux OFF read is LOW")
        log(1, "")

        return {
            "status": "PASS" if test_pass else "FAIL",
            "inter_packet_delay_ms": inter_packet_delay_ms,
            "aux_on_ok": aux_on_ok,
            "aux_off_ok": aux_off_ok,
        }

    except NotImplementedError as e:
        log(1, f"\nERROR: {e}")
        return {"status": "FAIL", "error": str(e)}
    except serial.SerialException as e:
        log(1, f"\nERROR: Serial port error: {e}")
        return {"status": "FAIL", "error": f"Serial port error: {e}"}
    except KeyboardInterrupt:
        log(1, "\n\nTest interrupted by user.")
        return {"status": "FAIL", "error": "Test interrupted by user"}
    except Exception as e:
        log(1, f"\nERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return {"status": "FAIL", "error": f"Unexpected error: {e}"}
