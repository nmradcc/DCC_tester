#!/usr/bin/env python3
"""
PacketAcceptanceTest Script
===========================

This script tests the inter-packet delay timing as described in NEM 671,
which specifies a minimum of 5ms between two data packets.

Test: Send motor start command, wait with configurable delay, then send
    emergency stop command while reading IO13/IO14 to verify response.

The inter_packet_delay_ms parameter can be adjusted for stress testing.
"""

import json
import serial
import time
import sys


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
        bytes_list: List of packet bytes (address + instruction)

    Returns:
        Checksum byte
    """
    checksum = 0
    for byte in bytes_list:
        checksum ^= byte
    return checksum


def make_speed_packet(address, speed, forward=True):
    """
    Create a DCC advanced operations speed packet (128-speed step mode).

    Args:
        address: Locomotive address (0-127 for short address)
        speed: Speed value (0-127, where 0=stop, 1=emergency stop, 2-127=speed steps)
        forward: True for forward, False for reverse

    Returns:
        List of packet bytes
    """
    # Advanced operations speed instruction: 0b00111111 (0x3F)
    instruction = 0x3F

    # Speed byte: bit 7 = direction (1=forward, 0=reverse), bits 6-0 = speed
    if forward:
        speed_byte = (1 << 7) | (speed & 0x7F)
    else:
        speed_byte = speed & 0x7F

    packet = [address, instruction, speed_byte]
    checksum = calculate_dcc_checksum(packet)
    packet.append(checksum)

    log(2, f"Packet for address {address}, speed {speed} {'forward' if forward else 'reverse'}:")
    log(2, f"  Bytes: {' '.join(f'0x{b:02X}' for b in packet)}")
    log(2, "  Binary breakdown:")
    log(2, f"    Address:     0x{packet[0]:02X} ({packet[0]})")
    log(2, f"    Instruction: 0x{packet[1]:02X} (advanced operations speed)")
    log(2, f"    Speed:       0x{packet[2]:02X} (dir={'forward' if forward else 'reverse'}, speed={speed})")
    log(2, f"    Checksum:    0x{packet[3]:02X}\n")

    return packet


def make_emergency_stop_packet(address):
    """
    Create a DCC emergency stop packet.

    Args:
        address: Locomotive address (0 for broadcast to all locomotives)

    Returns:
        List of packet bytes
    """
    # Advanced operations speed instruction: 0x3F
    # Emergency stop: speed = 1, direction = forward (bit 7 = 1)
    instruction = 0x3F
    speed_byte = (1 << 7) | 1  # 0x81

    packet = [address, instruction, speed_byte]
    checksum = calculate_dcc_checksum(packet)
    packet.append(checksum)

    log(2, "Emergency stop packet:")
    log(2, f"  Bytes: {' '.join(f'0x{b:02X}' for b in packet)}")
    log(2, "  Binary breakdown:")
    log(2, f"    Address:     0x{packet[0]:02X} ({packet[0]})")
    log(2, f"    Instruction: 0x{packet[1]:02X} (advanced operations speed)")
    log(2, f"    Speed:       0x{packet[2]:02X} (emergency stop)")
    log(2, f"    Checksum:    0x{packet[3]:02X}\n")

    return packet


def read_io13_io14(rpc):
    """
    Read IO13 and IO14 via a single RPC call.

    Returns:
        Tuple (io13_high, io14_high) or None on error
    """
    response = rpc.send_rpc("get_gpio_inputs", {})
    if response is None or response.get("status") != "ok":
        log(1, f"ERROR: Failed to read GPIO inputs: {response}")
        return None

    gpio_word = response.get("value")
    if gpio_word is None:
        log(1, f"ERROR: Missing GPIO value in response: {response}")
        return None

    io13_high = (gpio_word & (1 << 12)) != 0
    io14_high = (gpio_word & (1 << 13)) != 0

    log(2, f"GPIO inputs: 0x{gpio_word:04X} (IO13={'HIGH' if io13_high else 'LOW'}, IO14={'HIGH' if io14_high else 'LOW'})")
    return io13_high, io14_high


def run_packet_acceptance_test(rpc, loco_address, inter_packet_delay_ms=1000, logging_level=1):
    """
    Run the packet acceptance test.

    Args:
        rpc: DCCTesterRPC client instance
        loco_address: Locomotive address
        inter_packet_delay_ms: Delay between packets in milliseconds (default: 1000ms)

    Returns:
        Dictionary with test results including pass/fail status
    """
    # Fixed test parameters
    HALF_SPEED = 64  # Half of 127 (rounded up from 63.5)

    set_log_level(logging_level)

    log(2, "=" * 70)
    log(2, "DCC Packet Acceptance Test (NEM 671)")
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

        # Step 2: Read motor off IO status as baseline
        log(1, "Step 2: Reading motor off IO status as baseline...")
        io_state = read_io13_io14(rpc)
        if io_state is None:
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read IO13/IO14"}
        io13_high, io14_high = io_state
        motor_off_ok = io13_high and io14_high
        log(1, f"✓ Motor off IO state: {motor_off_ok} (IO13={'HIGH' if io13_high else 'LOW'}, IO14={'HIGH' if io14_high else 'LOW'})")

        # Step 3: Create motor start packet (half-speed reverse)
        log(1, f"Step 3: Creating motor start packet (speed {HALF_SPEED} reverse)...")
        start_packet = make_speed_packet(loco_address, HALF_SPEED, forward=False)

        # Step 4: Load and transmit the start packet
        log(1, "Step 4: Loading and transmitting motor start packet...")
        response = rpc.send_rpc("command_station_load_packet", {"bytes": start_packet, "replace": True})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to load packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to load packet"}

        response = rpc.send_rpc("command_station_transmit_packet",
                       {"delay_ms": 0})

        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to transmit packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to transmit packet"}

        # Step 5: Wait for inter-packet delay
        log(1, f"Step 5: Waiting {inter_packet_delay_ms} ms (inter-packet delay)...")
        time.sleep(inter_packet_delay_ms / 1000.0)
        log(2, "✓ Inter-packet delay complete\n")

        # Step 6: Read motor run IO status
        log(1, "Step 6: Reading motor run IO status...")
        io_state = read_io13_io14(rpc)
        if io_state is None:
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read IO13/IO14"}
        io13_high, io14_high = io_state
        motor_run_ok = (not io13_high) or (not io14_high)
        log(1, f"✓ Motor run IO state: {motor_run_ok} (IO13={'HIGH' if io13_high else 'LOW'}, IO14={'HIGH' if io14_high else 'LOW'})")

        # Step 7: Send emergency stop packet
        log(1, f"Step 7: Sending emergency stop packet to address {loco_address}...")
        estop_packet = make_emergency_stop_packet(loco_address)

        response = rpc.send_rpc("command_station_load_packet", {"bytes": estop_packet, "replace": True})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to load emergency stop packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to load emergency stop packet"}
        log(2, "✓ Emergency stop packet loaded\n")
        response = rpc.send_rpc("command_station_transmit_packet",
                       {"delay_ms": 0})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to transmit emergency stop packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to transmit emergency stop"}
        # Step 8: Wait 1 second for motor to stop
        log(2, "Step 8: Waiting 1 second for motor to stop...")
        time.sleep(1.0)

        # Step 9: Read motor stopped IO status
        log(1, "Step 9: Reading motor stopped IO status...")
        io_state = read_io13_io14(rpc)
        if io_state is None:
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read IO13/IO14"}
        io13_high, io14_high = io_state
        motor_stop_ok = io13_high and io14_high
        log(1, f"✓ Motor stopped IO state: {motor_stop_ok} (IO13={'HIGH' if io13_high else 'LOW'}, IO14={'HIGH' if io14_high else 'LOW'})")

        # Step 10: Stop command station
        log(1, "Step 10: Stopping command station")
        response = rpc.send_rpc("command_station_stop", {})

        if response is None or response.get("status") != "ok":
            log(1, f"WARNING: Failed to stop command station: {response}")
        else:
            log(2, "✓ Command station stopped\n")

        # Evaluate pass/fail
        test_pass = motor_off_ok and motor_run_ok and motor_stop_ok

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
        log(2, f"  Motor speed:           {HALF_SPEED} (reverse)")
        log(2, f"  Inter-packet delay:    {inter_packet_delay_ms} ms")
        log(2, "\nTest sequence completed:")
        log(2, "  1. Started command station in custom packet mode")
        log(2, f"  2. Read motor off IO state: {motor_off_ok}")
        log(2, f"  3. Created motor start packet (speed {HALF_SPEED} reverse)")
        log(2, f"  4. Transmitted motor start packet to address {loco_address}")
        log(2, f"  5. Waited {inter_packet_delay_ms} ms (inter-packet delay)")
        log(2, f"  6. Read motor run IO state: {motor_run_ok}")
        log(2, f"  7. Sent emergency stop packet to address {loco_address}")
        log(2, "  8. Waited 1 second for motor to stop")
        log(2, f"  9. Read motor stopped IO state: {motor_stop_ok}")
        log(2, "  10. Stopped command station")
        log(2, "\nIO state measurements:")
        log(2, f"  Motor off OK:  {motor_off_ok}")
        log(2, f"  Motor run OK:  {motor_run_ok}")
        log(2, f"  Motor stop OK: {motor_stop_ok}")
        log(2, "\nPass Criteria:")
        log(2, "  Off, Run, Stop states are all True")
        log(1, "")

        return {
            "status": "PASS" if test_pass else "FAIL",
            "inter_packet_delay_ms": inter_packet_delay_ms,
            "motor_off_ok": motor_off_ok,
            "motor_run_ok": motor_run_ok,
            "motor_stop_ok": motor_stop_ok
        }

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


