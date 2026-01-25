#!/usr/bin/env python3
"""
Acceptance Test with N-Bit 0 Duration Change
=============================================

This script runs acceptance tests with configurable packet override parameters:
1. First run: Normal test with default bit 0 duration
2. Second run: Packet override with specified mask, deltaP, and deltaN

Usage:
  python AcceptanceTestWithNBit0Change.py -m 4 -p 15 -n -15
"""

import subprocess
import sys
import os
import json
import serial
import time
import argparse


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


def run_test_with_bit0_change(com_port, loco_address, half_speed, override_mask, override_deltaP, override_deltaN):
    """
    Run acceptance test with packet override for zero bits.
    
    Args:
        com_port: Serial port name
        loco_address: Locomotive address
        half_speed: Speed value for half speed
        override_mask: Bit mask for which zero bits to override (None = don't override)
        override_deltaP: P-phase delta adjustment in microseconds
        override_deltaN: N-phase delta adjustment in microseconds
        
    Returns:
        0 on success, 1 on failure
    """
    print("=" * 70)
    if override_mask is None:
        print("DCC_tester Acceptance Test - Normal Run")
    else:
        print(f"DCC_tester Acceptance Test - Packet Override (mask=0x{override_mask:X}, deltaP={override_deltaP:+d}μs, deltaN={override_deltaN:+d}μs)")
    print("Half-Speed Reverse -> Broadcast Emergency Stop")
    print("=" * 70)
    print()
    
    try:
        # Connect to DCC_tester
        print(f"Connecting to {com_port}...")
        rpc = DCCTesterRPC(com_port)
        print("Connected!\n")
        
        # Initial cleanup: Stop command station
        print("Initial setup: Stopping command station (if running)...")
        response = rpc.send_rpc("command_station_stop", {})
        if response is None or response.get("status") != "ok":
            print(f"WARNING: Failed to stop command station: {response}")
        else:
            print("✓ Command station stopped\n")
        
        # Pre-step: Enable scope trigger on first bit
        print("Pre-step: Enabling scope trigger on first bit...")
        response = rpc.send_rpc("command_station_params", {"trigger_first_bit": True})
        if response is None or response.get("status") != "ok":
            print(f"WARNING: Failed to enable scope trigger: {response}")
        else:
            print("✓ Scope trigger enabled\n")
        
        # Step 1: Start command station in custom packet mode (loop=0)
        print("Step 1: Starting command station in custom packet mode...")
        response = rpc.send_rpc("command_station_start", {"loop": 0})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to start command station: {response}")
            rpc.close()
            return 1
        print(f"✓ Command station started (loop={response.get('loop', 0)})\n")
        
        time.sleep(0.5)
        
        # Step 2: Read motor off current as baseline
        print("Step 2: Reading motor off current as baseline...")
        response = rpc.send_rpc("get_current_feedback_ma", {})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to read current: {response}")
            rpc.close()
            return 1
        
        motor_off_current_ma = response.get("current_ma", 0)
        print(f"✓ Motor off current: {motor_off_current_ma} mA (baseline)\n")
        
        # Step 3: Create half-speed reverse packet
        print("Step 3: Creating half-speed reverse packet...")
        packet = make_speed_packet(loco_address, half_speed, forward=False)
        print(f"Packet for address {loco_address}, speed {half_speed} reverse:")
        print(f"  Bytes: {' '.join(f'0x{b:02X}' for b in packet)}")
        print(f"  Binary breakdown:")
        print(f"    Address:     0x{packet[0]:02X} ({packet[0]})")
        print(f"    Instruction: 0x{packet[1]:02X} (advanced operations speed)")
        print(f"    Speed:       0x{packet[2]:02X} (dir=reverse, speed={half_speed})")
        print(f"    Checksum:    0x{packet[3]:02X}\n")
        
        # Step 4: Load the packet
        print("Step 4: Loading packet into command station...")
        response = rpc.send_rpc("command_station_load_packet", {"bytes": packet})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to load packet: {response}")
            rpc.close()
            return 1
        print(f"✓ Packet loaded (length={response.get('length')} bytes)\n")
        
        # Step 5: Transmit the packet 3 times with 100ms delay
        print("Step 5: Transmitting packet 3 times with 100ms delay...")
        response = rpc.send_rpc("command_station_transmit_packet", 
                               {"count": 3, "delay_ms": 100})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to transmit packet: {response}")
            rpc.close()
            return 1
        
        # Step 6: motor run time
        time.sleep(0.5)        

        # Step 7: Read motor run current
        print("Step 7: Reading motor run current...")
        response = rpc.send_rpc("get_current_feedback_ma", {})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to read current: {response}")
            rpc.close()
            return 1
        
        motor_on_current_ma = response.get("current_ma", 0)
        print(f"✓ Motor run current: {motor_on_current_ma} mA\n")

        # Step 8: Create and load BROADCAST emergency stop packet (address 0)
        print(f"Step 8: Creating and loading BROADCAST emergency stop packet...")
        estop_packet = make_emergency_stop_packet(0)  # Address 0 = broadcast
        print(f"Broadcast emergency stop packet (address 0x00):")
        print(f"  Bytes: {' '.join(f'0x{b:02X}' for b in estop_packet)}")
        print(f"  Binary breakdown:")
        print(f"    Address:     0x{estop_packet[0]:02X} (0 = BROADCAST)")
        print(f"    Instruction: 0x{estop_packet[1]:02X} (advanced operations speed)")
        print(f"    Speed:       0x{estop_packet[2]:02X} (emergency stop)")
        print(f"    Checksum:    0x{estop_packet[3]:02X}\n")
        
        response = rpc.send_rpc("command_station_load_packet", {"bytes": estop_packet})
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to load emergency stop packet: {response}")
            rpc.close()
            return 1
        print(f"✓ Emergency stop packet loaded (length={response.get('length')} bytes)\n")

        # Step 8.5: Set packet override if specified (just before transmission)
        if override_mask is not None:
            print(f"Step 8.5: Setting packet override (mask=0x{override_mask:X}, deltaP={override_deltaP:+d}μs, deltaN={override_deltaN:+d}μs)...")
            response = rpc.send_rpc("command_station_packet_override", {
                "zerobit_override_mask": override_mask,
                "zerobit_deltaP": override_deltaP,
                "zerobit_deltaN": override_deltaN
            })
            if response is None or response.get("status") != "ok":
                print(f"ERROR: Failed to set packet override: {response}")
                rpc.close()
                return 1
            print(f"✓ Packet override set: mask=0x{override_mask:X}, P-phase {override_deltaP:+d}μs, N-phase {override_deltaN:+d}μs\n")

        # Step 9: Transmit the emergency stop packet
        print(f"Step 9: Transmitting emergency stop packet...")
        response = rpc.send_rpc("command_station_transmit_packet",
                               {"count": 1, "delay_ms": 100})
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to transmit emergency stop packet: {response}")
            rpc.close()
            return 1
        print(f"✓ Emergency stop packet transmission triggered")
        print(f"  Count: {response.get('count')}\n")
        
        print(f"Waiting 1 second for motor stop")
        time.sleep(1.0)
        
        # Step 9.5: Reset packet override if it was set
        if override_mask is not None:
            print("\nStep 9.5: Resetting packet override parameters to zero...")
            response = rpc.send_rpc("command_station_packet_reset_override", {})
            if response is None or response.get("status") != "ok":
                print(f"ERROR: Failed to reset packet override: {response}")
                rpc.close()
                return 1
            print("✓ Packet override reset to zero\n")
        
        # Step 10: Read motor stopped current
        print("\nStep 10: Reading motor stopped current...")
        response = rpc.send_rpc("get_current_feedback_ma", {})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to read current: {response}")
            rpc.close()
            return 1
        
        motor_stopped_current_ma = response.get("current_ma", 0)
        print(f"✓ Motor stopped current: {motor_stopped_current_ma} mA\n")
        
        # Step 11: Stop command station
        print("Step 11: Stopping command station...")
        response = rpc.send_rpc("command_station_stop", {})
        
        if response is None or response.get("status") != "ok":
            print(f"WARNING: Failed to stop command station: {response}")
        else:
            print(f"✓ Command station stopped\n")
        
        print("\n" + "=" * 70)
        print("✓ TEST COMPLETE")
        print("=" * 70)
        
        test_passed = (motor_on_current_ma > motor_off_current_ma and
                      motor_stopped_current_ma < motor_on_current_ma)
        
        if test_passed:
            print("✓ TEST PASS")
        else:
            print("✗ TEST FAIL")
        print("=" * 70)
        
        print(f"\nSent half-speed reverse packets to address {loco_address}")
        print(f"Speed value: {half_speed} (approximately half of max speed 127)")
        if override_mask is not None:
            print(f"Packet override applied: mask=0x{override_mask:X}, deltaP={override_deltaP:+d}μs, deltaN={override_deltaN:+d}μs")
        print(f"\nTest sequence completed:")
        print(f"  1. Started command station in custom packet mode")
        print(f"  2. Read motor off current: {motor_off_current_ma} mA (baseline)")
        print(f"  3. Created half-speed reverse packet")
        print(f"  4. Loaded packet into command station")
        print(f"  5. Transmitted 3 half-speed reverse packets to address {loco_address}")
        print(f"  6. Motor run time: 0.5 seconds")
        print(f"  7. Read motor run current: {motor_on_current_ma} mA")
        print(f"  8. Created and loaded BROADCAST emergency stop packet (address 0x00)")
        if override_mask is not None:
            print(f"  8.5 Set packet override (mask=0x{override_mask:X}, deltaP={override_deltaP:+d}μs, deltaN={override_deltaN:+d}μs)")
        print(f"  9. Transmitted emergency stop packet")
        if override_mask is not None:
            print(f"  9.5 Reset packet override parameters to zero")
        print(f" 10. Read motor stopped current: {motor_stopped_current_ma} mA")
        print(f" 11. Stopped command station")
        print(f"\nCurrent measurements:")
        print(f"  Motor off:     {motor_off_current_ma} mA (baseline)")
        print(f"  Motor running: {motor_on_current_ma} mA (delta: {motor_on_current_ma - motor_off_current_ma} mA)")
        print(f"  Motor stopped: {motor_stopped_current_ma} mA (delta: {motor_stopped_current_ma - motor_off_current_ma} mA)")
        if override_mask is not None:
            print(f"\nPacket override applied:")
            print(f"  Mask:   0x{override_mask:X}")
            print(f"  DeltaP: {override_deltaP:+d}μs")
            print(f"  DeltaN: {override_deltaN:+d}μs")
        print()
        
        # Close connection
        rpc.close()
        
        return 0 if test_passed else 1
        
    except serial.SerialException as e:
        print(f"\nERROR: Serial port error: {e}")
        print(f"Make sure {com_port} is the correct port and the device is connected.")
        return 1
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")
        return 1
    except Exception as e:
        print(f"\nERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


def main():
    """Main test orchestrator."""
    
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description='DCC Acceptance Test with configurable packet override parameters',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test with mask=4 (bit 2), deltaP=+20μs, deltaN=-20μs
  python AcceptanceTestWithNBit0Change.py --mask 4 --deltaP 20 --deltaN -20
  
  # Test with mask=7 (bits 0,1,2), deltaP=+15μs, deltaN=-15μs
  python AcceptanceTestWithNBit0Change.py -m 7 -p 15 -n -15
        """)
    
    parser.add_argument('-m', '--mask', type=int, required=True,
                        help='Zero bit override mask (which bits to modify)')
    parser.add_argument('-p', '--deltaP', type=int, required=True,
                        help='P-phase delta adjustment in microseconds (can be negative)')
    parser.add_argument('-n', '--deltaN', type=int, required=True,
                        help='N-phase delta adjustment in microseconds (can be negative)')
    parser.add_argument('--port', type=str, default='COM6',
                        help='Serial port (default: COM6)')
    parser.add_argument('--address', type=int, default=3,
                        help='Locomotive address (default: 3)')
    
    args = parser.parse_args()
    
    # Configuration from arguments
    COM_PORT = args.port
    LOCO_ADDRESS = args.address
    HALF_SPEED = 64  # Half of 127 (rounded up from 63.5)
    OVERRIDE_MASK = args.mask
    OVERRIDE_DELTAP = args.deltaP
    OVERRIDE_DELTAN = args.deltaN
    
    print("\n" + "=" * 70)
    print("DUAL ACCEPTANCE TEST")
    print("Test 1: Normal run (default bit 0 duration)")
    print(f"Test 2: Packet override (mask=0x{OVERRIDE_MASK:X}, deltaP={OVERRIDE_DELTAP:+d}μs, deltaN={OVERRIDE_DELTAN:+d}μs)")
    print("=" * 70)
    print()
    
    # Run Test 1: Normal test
    print("\n" + "#" * 70)
    print("# TEST 1: NORMAL RUN (Default Bit 0 Duration)")
    print("#" * 70 + "\n")
    
    result1 = run_test_with_bit0_change(COM_PORT, LOCO_ADDRESS, HALF_SPEED, None, None, None)
    
    if result1 != 0:
        print("\n" + "!" * 70)
        print("! TEST 1 FAILED - Aborting Test 2")
        print("!" * 70)
        return 1
    
    print("\n" + "✓" * 70)
    print("✓ TEST 1 PASSED - Proceeding to Test 2")
    print("✓" * 70)
    
    # Wait between tests
    time.sleep(2)
    
    # Run Test 2: Modified test with packet override
    print("\n" + "#" * 70)
    print(f"# TEST 2: PACKET OVERRIDE (mask=0x{OVERRIDE_MASK:X}, deltaP={OVERRIDE_DELTAP:+d}μs, deltaN={OVERRIDE_DELTAN:+d}μs)")
    print("#" * 70 + "\n")
    
    result2 = run_test_with_bit0_change(COM_PORT, LOCO_ADDRESS, HALF_SPEED, 
                                        OVERRIDE_MASK, OVERRIDE_DELTAP, OVERRIDE_DELTAN)
    
    # Final summary
    print("\n\n" + "=" * 70)
    print("FINAL SUMMARY")
    print("=" * 70)
    print(f"Test 1 (Normal):                {'PASS ✓' if result1 == 0 else 'FAIL ✗'}")
    print(f"Test 2 (Override mask=0x{OVERRIDE_MASK:X}): {'PASS ✓' if result2 == 0 else 'FAIL ✗'}")
    print("=" * 70)
    
    if result1 == 0 and result2 == 0:
        print("✓ ALL TESTS PASSED")
        return 0
    else:
        print("✗ SOME TESTS FAILED")
        return 1


if __name__ == "__main__":
    sys.exit(main())
