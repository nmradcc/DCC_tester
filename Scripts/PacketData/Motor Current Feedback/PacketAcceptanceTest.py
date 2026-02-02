#!/usr/bin/env python3
"""
PacketAcceptanceTest Script
===========================

This script tests the inter-packet delay timing as described in NEM 671,
which specifies a minimum of 5ms between two data packets.

Test: Send motor start command, wait with configurable delay, then send
      emergency stop command while measuring current to verify motor response.

The inter_packet_delay_ms parameter can be adjusted for stress testing.
"""

import json
import serial
import time
import sys


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
        print(f"→ {request_json.strip()}")

        self.ser.write(request_json.encode('utf-8'))

        # Read response
        response_line = self.ser.readline().decode('utf-8').strip()
        print(f"← {response_line}")

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

    return packet


def run_packet_acceptance_test(rpc, loco_address, inter_packet_delay_ms=1000):
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

    print("=" * 70)
    print("DCC Packet Acceptance Test (NEM 671)")
    print(f"Inter-packet delay: {inter_packet_delay_ms} ms")
    print("=" * 70)
    print()

    try:

        # Step 1: Start command station in custom packet mode (loop=0)
        print("Step 1: Starting command station in custom packet mode...")
        response = rpc.send_rpc("command_station_start", {"loop": 0})

        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to start command station: {response}")
            return {"status": "FAIL", "error": "Failed to start command station"}
        print(f"✓ Command station started (loop={response.get('loop', 0)})\n")

        time.sleep(0.5)

        # Step 2: Read motor off current as baseline
        print("Step 2: Reading motor off current as baseline...")
        response = rpc.send_rpc("get_current_feedback_ma", {})

        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to read current: {response}")
            rpc.close()
            r
        motor_off_current_ma = response.get("current_ma", 0)
        print(f"✓ Motor off current: {motor_off_current_ma} mA (baseline)\n")

        # Step 3: Create motor start packet (half-speed reverse)
        print(f"Step 3: Creating motor start packet (speed {HALF_SPEED} reverse)...")
        start_packet = make_speed_packet(loco_address, HALF_SPEED, forward=False)
        print(f"Packet for address {loco_address}, speed {HALF_SPEED} reverse:")
        print(f"  Bytes: {' '.join(f'0x{b:02X}' for b in start_packet)}")
        print(f"  Binary breakdown:")
        print(f"    Address:     0x{start_packet[0]:02X} ({start_packet[0]})")
        print(f"    Instruction: 0x{start_packet[1]:02X} (advanced operations speed)")
        print(f"    Speed:       0x{start_packet[2]:02X} (dir=reverse, speed={HALF_SPEED})")
        print(f"    Checksum:    0x{start_packet[3]:02X}\n")

        # Step 4: Load and transmit the start packet
        print("Step 4: Loading and transmitting motor start packet...")
        response = rpc.send_rpc("command_station_load_packet", {"bytes": start_packet})

        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to load packet: {response}")
            rpc.close()
            r(f"✓ Packet loaded (length={response.get('length')} bytes)\n")

        response = rpc.send_rpc("command_station_transmit_packet",
                               {"count": 1, "delay_ms": 0})

        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to transmit packet: {response}")
            rpc.close()
            r(f"✓ Motor start packet transmitted\n")

        # Step 5: Wait for inter-packet delay
        print(f"Step 5: Waiting {inter_packet_delay_ms} ms (inter-packet delay)...")
        time.sleep(inter_packet_delay_ms / 1000.0)
        print(f"✓ Inter-packet delay complete\n")

        # Step 6: Read motor run current
        print("Step 6: Reading motor run current...")
        response = rpc.send_rpc("get_current_feedback_ma", {})

        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to read current: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read motor current"}
        motor_on_current_ma = response.get("current_ma", 0)
        print(f"✓ Motor run current: {motor_on_current_ma} mA\n")

        # Step 7: Send emergency stop packet
        print(f"Step 7: Sending emergency stop packet to address {loco_address}...")
        estop_packet = make_emergency_stop_packet(loco_address)
        print(f"Emergency stop packet:")
        print(f"  Bytes: {' '.join(f'0x{b:02X}' for b in estop_packet)}")
        print(f"  Binary breakdown:")
        print(f"    Address:     0x{estop_packet[0]:02X} ({estop_packet[0]})")
        print(f"    Instruction: 0x{estop_packet[1]:02X} (advanced operations speed)")
        print(f"    Speed:       0x{estop_packet[2]:02X} (emergency stop)")
        print(f"    Checksum:    0x{estop_packet[3]:02X}\n")

        response = rpc.send_rpc("command_station_load_packet", {"bytes": estop_packet})
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to load emergency stop packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to load emergency stop packet"}
        print(f"✓ Emergency stop packet loaded\n")
        response = rpc.send_rpc("command_station_transmit_packet",
                               {"count": 1, "delay_ms": 0})
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to transmit emergency stop packet: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to transmit emergency stop"}
        # Step 8: Wait 1 second for motor to stop
        print(f"Waiting 1 second for motor to stop...")
        time.sleep(1.0)

        # Step 9: Read motor stopped current
        print("Step 9: Reading motor stopped current...")
        response = rpc.send_rpc("get_current_feedback_ma", {})

        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to read current: {response}")
            rpc.close()
            return {"status": "FAIL", "error": "Failed to read stopped current"}

        motor_stopped_current_ma = response.get("current_ma", 0)
        print(f"✓ Motor stopped current: {motor_stopped_current_ma} mA\n")

        # Step 10: Stop command station
        print("Step 10: Stopping command station...")
        response = rpc.send_rpc("command_station_stop", {})

        if response is None or response.get("status") != "ok":
            print(f"WARNING: Failed to stop command station: {response}")
        else:
            print(f"✓ Command station stopped\n")

        # Evaluate pass/fail
        current_increase = motor_on_current_ma - motor_off_current_ma
        current_decrease = motor_on_current_ma - motor_stopped_current_ma

        # Pass criteria: motor current must increase by at least 1mA during run and decrease by at least 1mA after stop
        MIN_CURRENT_DELTA_MA = 1
        test_pass = (current_increase >= MIN_CURRENT_DELTA_MA and current_decrease >= MIN_CURRENT_DELTA_MA)

        print("\n" + "=" * 70)
        print("✓ TEST COMPLETE")
        print("=" * 70)
        if test_pass:
            print("✓ TEST PASS")
        else:
            print("✗ TEST FAIL")
        print("=" * 70)
        print(f"\nTest Parameters:")
        print(f"  Locomotive address:    {loco_address}")
        print(f"  Motor speed:           {HALF_SPEED} (reverse)")
        print(f"  Inter-packet delay:    {inter_packet_delay_ms} ms")
        print(f"\nTest sequence completed:")
        print(f"  1. Started command station in custom packet mode")
        print(f"  2. Read motor off current: {motor_off_current_ma} mA (baseline)")
        print(f"  3. Created motor start packet (speed {HALF_SPEED} reverse)")
        print(f"  4. Transmitted motor start packet to address {loco_address}")
        print(f"  5. Waited {inter_packet_delay_ms} ms (inter-packet delay)")
        print(f"  6. Read motor run current: {motor_on_current_ma} mA")
        print(f"  7. Sent emergency stop packet to address {loco_address}")
        print(f"  8. Waited 1 second for motor to stop")
        print(f"  9. Read motor stopped current: {motor_stopped_current_ma} mA")
        print(f"  10. Stopped command station")
        print(f"\nCurrent measurements:")
        print(f"  Motor off:     {motor_off_current_ma} mA (baseline)")
        print(f"  Motor running: {motor_on_current_ma} mA (delta: {current_increase:+d} mA)")
        print(f"  Motor stopped: {motor_stopped_current_ma} mA (delta from baseline: {motor_stopped_current_ma - motor_off_current_ma:+d} mA)")
        print(f"\nPass Criteria (minimum delta: {MIN_CURRENT_DELTA_MA} mA):")
        print(f"  Current increased during run: {current_increase >= MIN_CURRENT_DELTA_MA} ({current_increase:+d} mA >= {MIN_CURRENT_DELTA_MA} mA)")
        print(f"  Current decreased after stop: {current_decrease >= MIN_CURRENT_DELTA_MA} ({current_decrease:+d} mA >= {MIN_CURRENT_DELTA_MA} mA)")
        print()

        return {
            "status": "PASS" if test_pass else "FAIL",
            "inter_packet_delay_ms": inter_packet_delay_ms,
            "motor_off_current_ma": motor_off_current_ma,
            "motor_on_current_ma": motor_on_current_ma,
            "motor_stopped_current_ma": motor_stopped_current_ma,
            "current_increase": current_increase,
            "current_decrease": current_decrease
        }

    except serial.SerialException as e:
        print(f"\nERROR: Serial port error: {e}")
        return {"status": "FAIL", "error": f"Serial port error: {e}"}
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")
        return {"status": "FAIL", "error": "Test interrupted by user"}
    except Exception as e:
        print(f"\nERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return {"status": "FAIL", "error": f"Unexpected error: {e}"}


def main():
    """Main function for standalone execution."""

    # Configuration
    COM_PORT = "COM6"  # Change this to match your USB CDC ACM port
    LOCO_ADDRESS = 3   # Locomotive address
    INTER_PACKET_DELAY_MS = 5  # NEM 671 specifies minimum 5ms

    try:
        # Connect to DCC_tester
        print(f"Connecting to {COM_PORT}...")
        rpc = DCCTesterRPC(COM_PORT)
        print("Connected!\n")

        # Run the test
        result = run_packet_acceptance_test(
            rpc,
            LOCO_ADDRESS,
            INTER_PACKET_DELAY_MS
        )

        # Close connection
        rpc.close()

        # Return exit code based on result
        if result.get("status") == "PASS":
            return 0
        else:
            return 1

    except serial.SerialException as e:
        print(f"\nERROR: Serial port error: {e}")
        print(f"Make sure {COM_PORT} is the correct port and the device is connected.")
        return 1
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")
        return 1
    except Exception as e:
        print(f"\nERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
