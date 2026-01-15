#!/usr/bin/env python3
"""
AcceptanceTestPacket Script
===========================

This script tests custom packet injection via RPC calls to send
DCC speed command packets using the DCC_tester command station.

Test: Send 3 half-speed reverse packets with 100ms delay between them
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


def main():
    """Main test function."""
    
    # Configuration
    COM_PORT = "COM6"  # Change this to match your USB CDC ACM port
    LOCO_ADDRESS = 3   # Locomotive address
    HALF_SPEED = 64    # Half of 127 (rounded up from 63.5)
    NUM_IDLE_PACKETS = 3  # Number of idle packets (equivalent delay at ~2.7ms each)
    IDLE_PACKET_TIME_MS = 2.7  # Time per idle packet in milliseconds
    
    print("=" * 70)
    print("DCC_tester Acceptance Test")
    print("Half-Speed Reverse -> Idle Delay -> Emergency Stop")
    print("=" * 70)
    print()
    
    try:
        # Connect to DCC_tester
        print(f"Connecting to {COM_PORT}...")
        rpc = DCCTesterRPC(COM_PORT)
        print("Connected!\n")
        # Pre-step: Enable scope trigger on first bit
        print("Pre-step: Enabling scope trigger on first bit...")
        response = rpc.send_rpc("command_station_params", {"trigger_first_bit": True})
        if response.get("status") != "ok":
            print(f"WARNING: Failed to enable scope trigger: {response}")
        else:
            print("\u2713 Scope trigger enabled\n")
        
        # Step 1: Start command station in custom packet mode (loop=0)
        print("Step 1: Starting command station in custom packet mode...")
        response = rpc.send_rpc("command_station_start", {"loop": 0})
        
        if response.get("status") != "ok":
            print(f"ERROR: Failed to start command station: {response}")
            return 1
        print(f"✓ Command station started (loop={response.get('loop', 0)})\n")
        
        time.sleep(0.5)
        
        # Step 2: Create half-speed reverse packet
        print("Step 2: Creating half-speed reverse packet...")
        packet = make_speed_packet(LOCO_ADDRESS, HALF_SPEED, forward=False)
        print(f"Packet for address {LOCO_ADDRESS}, speed {HALF_SPEED} reverse:")
        print(f"  Bytes: {' '.join(f'0x{b:02X}' for b in packet)}")
        print(f"  Binary breakdown:")
        print(f"    Address:     0x{packet[0]:02X} ({packet[0]})")
        print(f"    Instruction: 0x{packet[1]:02X} (advanced operations speed)")
        print(f"    Speed:       0x{packet[2]:02X} (dir=reverse, speed={HALF_SPEED})")
        print(f"    Checksum:    0x{packet[3]:02X}\n")
        
        # Step 3: Load the packet
        print("Step 3: Loading packet into command station...")
        response = rpc.send_rpc("command_station_load_packet", {"bytes": packet})
        
        if response.get("status") != "ok":
            print(f"ERROR: Failed to load packet: {response}")
            return 1
        print(f"✓ Packet loaded (length={response.get('length')} bytes)\n")
        
        # Step 4: Transmit the packet 3 times with 100ms delay
        print("Step 4: Transmitting packet 3 times with 100ms delay...")
        response = rpc.send_rpc("command_station_transmit_packet", 
                               {"count": 3, "delay_ms": 100})
        
        if response.get("status") != "ok":
            print(f"ERROR: Failed to transmit packet: {response}")
            return 1
#        print(f"✓ Packet transmission triggered")
#        print(f"  Count: {response.get('count')}")
#        print(f"  Delay: {response.get('delay_ms')} ms\n")
        
        # Wait for transmissions to complete
#        wait_time = (response.get('count', 3) * response.get('delay_ms', 100)) / 1000 + 0.5
#        print(f"Waiting {wait_time:.1f} seconds for transmissions to complete...")
#        time.sleep(wait_time)
        
        # Step 5: Idle delay (simulating idle packets without actually sending them)
        idle_delay_ms = NUM_IDLE_PACKETS * IDLE_PACKET_TIME_MS
        idle_delay_s = idle_delay_ms / 1000
#        print(f"\nStep 5: Idle delay ({NUM_IDLE_PACKETS} idle packets @ {IDLE_PACKET_TIME_MS}ms each)...")
#        print(f"  Total delay: {idle_delay_ms:.1f} ms ({idle_delay_s:.4f} seconds)")
#        time.sleep(idle_delay_s)
#        print(f"✓ Idle delay complete\n")
        
        # Step 6: Send emergency stop packet
#        print(f"Step 6: Sending emergency stop packet...")
        estop_packet = make_emergency_stop_packet(LOCO_ADDRESS)
        print(f"Emergency stop packet for address {LOCO_ADDRESS}:")
        print(f"  Bytes: {' '.join(f'0x{b:02X}' for b in estop_packet)}")
        print(f"  Binary breakdown:")
        print(f"    Address:     0x{estop_packet[0]:02X} ({estop_packet[0]})")
        print(f"    Instruction: 0x{estop_packet[1]:02X} (advanced operations speed)")
        print(f"    Speed:       0x{estop_packet[2]:02X} (emergency stop)")
        print(f"    Checksum:    0x{estop_packet[3]:02X}\n")
        
        response = rpc.send_rpc("command_station_load_packet", {"bytes": estop_packet})
        if response.get("status") != "ok":
            print(f"ERROR: Failed to load emergency stop packet: {response}")
            return 1
        print(f"✓ Emergency stop packet loaded (length={response.get('length')} bytes)\n")
        
        response = rpc.send_rpc("command_station_transmit_packet",
                               {"count": 1, "delay_ms": 100})
        if response.get("status") != "ok":
            print(f"ERROR: Failed to transmit emergency stop packet: {response}")
            return 1
        print(f"✓ Emergency stop packet transmission triggered")
        print(f"  Count: {response.get('count')}\n")
        
        wait_time = 0.5  # Brief wait for final transmission
        print(f"Waiting {wait_time:.1f} seconds for transmission to complete...")
        time.sleep(wait_time)
        
        print("\n" + "=" * 70)
        print("✓ TEST COMPLETE")
        print("=" * 70)
        print(f"\nSent {response.get('count')} half-speed reverse packets to address {LOCO_ADDRESS}")
        print(f"Speed value: {HALF_SPEED} (approximately half of max speed 127)")
        print(f"\nTest sequence completed:")
        print(f"  1. Sent 3 half-speed reverse packets to address {LOCO_ADDRESS}")
        print(f"  2. Idle delay of {idle_delay_ms:.1f} ms ({NUM_IDLE_PACKETS} idle packet equivalents)")
        print(f"  3. Sent 1 emergency stop packet to address {LOCO_ADDRESS}")
        print()
        
        # Close connection
        rpc.close()
        return 0
        
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
