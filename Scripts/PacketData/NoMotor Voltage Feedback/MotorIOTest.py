#!/usr/bin/env python3
"""
Motor IO Test
=============

- Configures IO15 as an output with default HIGH
- Scans IO13/IO14 as fast as possible
- Reads IO13 and IO14 inputs
- Sets IO15 HIGH only when both IO13 and IO14 are HIGH

Usage:
  python MotorIOTest.py <COM_PORT>

Example:
  python MotorIOTest.py COM3
  python MotorIOTest.py /dev/ttyACM0
"""

import json
import sys
import time
import serial


class DCCTesterRPC:
    """RPC client for DCC_tester command station."""

    def __init__(self, port, baudrate=115200, timeout=2):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.5)
        self.ser.reset_input_buffer()

    def send_rpc(self, method, params, verbose=True):
        request = {"method": method, "params": params}
        request_json = json.dumps(request) + "\r\n"
        if verbose:
            print(f"→ {request_json.strip()}")
        self.ser.write(request_json.encode("utf-8"))
        response_line = self.ser.readline().decode("utf-8").strip()
        if verbose:
            print(f"← {response_line}")
        if response_line:
            try:
                return json.loads(response_line)
            except json.JSONDecodeError:
                print(f"Error: Invalid JSON response: {response_line}")
                return None
        return None

    def close(self):
        self.ser.close()


def main():
    if len(sys.argv) < 2:
        print("Usage: python MotorIOTest.py <COM_PORT>")
        print("Example: python MotorIOTest.py COM3")
        print("         python MotorIOTest.py /dev/ttyACM0")
        return 1

    com_port = sys.argv[1]

    print("=" * 70)
    print("Motor IO Test")
    print("=" * 70)
    print(f"Connecting to {com_port}...")

    try:
        rpc = DCCTesterRPC(com_port)
    except serial.SerialException as e:
        print(f"ERROR: Could not open {com_port}: {e}")
        return 1

        
    except KeyboardInterrupt:
        print("\nExiting...")
    except Exception as e:
        print(f"\nERROR: {e}")
        rpc.close()
        return 1

    rpc.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
