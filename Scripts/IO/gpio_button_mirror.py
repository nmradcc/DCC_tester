#!/usr/bin/env python3
"""
GPIO Button Mirror Script
=========================

This script demonstrates GPIO control via RPC:
- Configures IO14 as an output with initial state high
- Continuously monitors the push button (IO16/BUTTON_USER)
- When button is pressed, reads IO13 input state
- Sets IO14 output to match IO13 input state

Hardware connections:
- IO16: Built-in push button (BUTTON_USER on Nucleo board)
- IO13: Input to be mirrored
- IO14: Output that mirrors IO13 when button is pressed
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
        # Flush any initial output
        self.ser.reset_input_buffer()
        
    def send_rpc(self, method, params):
        """
        Send an RPC request and return the response.
        
        Args:
            method: RPC method name
            params: Dictionary of parameters
            
        Returns:
            Response dictionary or None on error
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
            try:
                return json.loads(response_line)
            except json.JSONDecodeError:
                print(f"Error: Invalid JSON response: {response_line}")
                return None
        return None
    
    def close(self):
        """Close serial connection."""
        self.ser.close()


def main():
    """Main function."""
    # Check command line arguments
    if len(sys.argv) < 2:
        print("Usage: python gpio_button_mirror.py <COM_PORT>")
        print("Example: python gpio_button_mirror.py COM3")
        print("         python gpio_button_mirror.py /dev/ttyACM0")
        return 1
    
    com_port = sys.argv[1]
    
    print("=" * 70)
    print("GPIO Button Mirror Test")
    print("=" * 70)
    print(f"Connecting to {com_port}...")
    
    try:
        rpc = DCCTesterRPC(com_port)
    except serial.SerialException as e:
        print(f"ERROR: Could not open {com_port}: {e}")
        return 1
    
    try:
        # Step 1: Configure IO14 as output with initial state HIGH
        print("\nStep 1: Configuring IO14 as output (initial state HIGH)...")
        response = rpc.send_rpc("configure_gpio_output", {"pin": 14, "state": 1})
        
        if response is None or response.get("status") != "ok":
            print(f"ERROR: Failed to configure IO14 as output: {response}")
            rpc.close()
            return 1
        
        print("✓ IO14 configured as output (initial state: HIGH)\n")
        
        # Step 2: Monitor button and mirror IO13 to IO14
        print("Step 2: Monitoring push button (IO16)...")
        print("Press the button to copy IO13 state to IO14")
        print("Press Ctrl+C to exit\n")
        
        button_was_pressed = False
        
        while True:
            # Read button state (IO16)
            response = rpc.send_rpc("get_gpio_input", {"pin": 16})
            
            if response is None or response.get("status") != "ok":
                print(f"WARNING: Failed to read button state: {response}")
                time.sleep(0.1)
                continue
            
            button_pressed = response.get("value", 0) == 1
            
            # Detect button press (rising edge)
            if button_pressed and not button_was_pressed:
                print("\n→ Button PRESSED!")
                
                # Read IO13 input state
                response = rpc.send_rpc("get_gpio_input", {"pin": 13})
                
                if response is None or response.get("status") != "ok":
                    print(f"ERROR: Failed to read IO13: {response}")
                else:
                    io13_state = response.get("value", 0)
                    print(f"  IO13 state: {io13_state}")
                    
                    # Set IO14 to match IO13
                    response = rpc.send_rpc("set_gpio_output", {"pin": 14, "state": io13_state})
                    
                    if response is None or response.get("status") != "ok":
                        print(f"ERROR: Failed to set IO14: {response}")
                    else:
                        print(f"  ✓ IO14 set to: {io13_state}")
            
            # Detect button release
            if not button_pressed and button_was_pressed:
                print("→ Button RELEASED\n")
            
            button_was_pressed = button_pressed
            
            # Small delay to avoid excessive polling
            time.sleep(0.05)
    
    except KeyboardInterrupt:
        print("\n\nExiting...")
    except Exception as e:
        print(f"\nERROR: {e}")
        rpc.close()
        return 1
    
    rpc.close()
    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
