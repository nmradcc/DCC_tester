#!/usr/bin/env python3
"""
Acceptance Test with Bit 0 Duration Change
===========================================

This script runs AcceptanceTestPacketAllStop.py twice:
1. First run: Normal test with default bit 0 duration
2. Second run: Change bit 0 duration to 200μs just before emergency stop
"""

import subprocess
import sys
import os
import json
import serial
import time


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


def run_test_with_bit0_change(com_port, loco_address, half_speed, override_delta):
    """
    Run acceptance test with packet override for second zero bit.
    
    Args:
        com_port: Serial port name
        loco_address: Locomotive address
        half_speed: Speed value for half speed
        override_delta: Delta P-phase adjustment in microseconds (None = don't override)
        
    Returns:
        0 on success, 1 on failure
    """
    print("=" * 70)
    if override_delta is None:
        print("DCC_tester Acceptance Test - Normal Run")
    else:
        print(f"DCC_tester Acceptance Test - Packet Override (+{override_delta}μs on second zero bit)")
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
        if override_delta is not None:
            print(f"Step 8.5: Setting packet override for second zero bit (mask=0x02, deltaP=+{override_delta}μs, deltaN=-{override_delta}μs)...")
            # Bit position 2 (0-indexed as bit 1) is the second zero bit after preamble
            # Binary: 0b10 = 0x02
            response = rpc.send_rpc("command_station_packet_override", {
                "zerobit_override_mask": 4,
                "zerobit_deltaP": override_delta,
                "zerobit_deltaN": -override_delta
            })
            if response is None or response.get("status") != "ok":
                print(f"ERROR: Failed to set packet override: {response}")
                rpc.close()
                return 1
            print(f"✓ Packet override set: second zero bit will have P-phase +{override_delta}μs, N-phase -{override_delta}μs\n")

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
        if override_delta is not None:
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
        if override_delta is not None:
            print(f"Packet override applied: Second zero bit P-phase +{override_delta}μs before emergency stop")
        print(f"\nTest sequence completed:")
        print(f"  1. Started command station in custom packet mode")
        print(f"  2. Read motor off current: {motor_off_current_ma} mA (baseline)")
        print(f"  3. Created half-speed reverse packet")
        print(f"  4. Loaded packet into command station")
        print(f"  5. Transmitted 3 half-speed reverse packets to address {loco_address}")
        print(f"  6. Motor run time: 0.5 seconds")
        print(f"  7. Read motor run current: {motor_on_current_ma} mA")
        print(f"  8. Created and loaded BROADCAST emergency stop packet (address 0x00)")
        if override_delta is not None:
            print(f"  8.5 Set packet override for second zero bit (mask=0x02, deltaP=+{override_delta}μs)")
        print(f"  9. Transmitted emergency stop packet")
        if override_delta is not None:
            print(f"  9.5 Reset packet override parameters to zero")
        print(f" 10. Read motor stopped current: {motor_stopped_current_ma} mA")
        print(f" 11. Stopped command station")
        print(f"\nCurrent measurements:")
        print(f"  Motor off:     {motor_off_current_ma} mA (baseline)")
        print(f"  Motor running: {motor_on_current_ma} mA (delta: {motor_on_current_ma - motor_off_current_ma} mA)")
        print(f"  Motor stopped: {motor_stopped_current_ma} mA (delta: {motor_stopped_current_ma - motor_off_current_ma} mA)")
        if override_delta is not None:
            print(f"\nPacket override applied:")
            print(f"  Second zero bit P-phase increased by {override_delta}μs (mask: 0x02)")
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
    
    # Configuration
    COM_PORT = "COM6"  # Change this to match your USB CDC ACM port
    LOCO_ADDRESS = 3   # Locomotive address for speed test
    HALF_SPEED = 64    # Half of 127 (rounded up from 63.5)
    
    print("\n" + "=" * 70)
    print("DUAL ACCEPTANCE TEST")
    print("Test 1: Normal run (default bit 0 duration)")
    print("Test 2: Packet override (second zero bit P-phase +20μs)")
    print("=" * 70)
    print()
    
    # Run Test 1: Normal test
    print("\n" + "#" * 70)
    print("# TEST 1: NORMAL RUN (Default Bit 0 Duration)")
    print("#" * 70 + "\n")
    
    result1 = run_test_with_bit0_change(COM_PORT, LOCO_ADDRESS, HALF_SPEED, None)
    
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
    print("# TEST 2: PACKET OVERRIDE (Second Zero Bit P-phase +20μs)")
    print("#" * 70 + "\n")
    
    result2 = run_test_with_bit0_change(COM_PORT, LOCO_ADDRESS, HALF_SPEED, +20)
    
    # Final summary
    print("\n\n" + "=" * 70)
    print("FINAL SUMMARY")
    print("=" * 70)
    print(f"Test 1 (Normal):        {'PASS ✓' if result1 == 0 else 'FAIL ✗'}")
    print(f"Test 2 (Override+20μs): {'PASS ✓' if result2 == 0 else 'FAIL ✗'}")
    print("=" * 70)
    
    if result1 == 0 and result2 == 0:
        print("✓ ALL TESTS PASSED")
        return 0
    else:
        print("✗ SOME TESTS FAILED")
        return 1


if __name__ == "__main__":
    sys.exit(main())
