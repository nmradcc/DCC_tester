#!/usr/bin/env python3
"""
SystemRPCTest.py
- Sends one JSON-RPC echo request
- Prints PASS/FAIL and exits with code 0/1
"""

import json
import sys
import time
from pathlib import Path

import serial


SCRIPT_DIR = Path(__file__).resolve().parent
CONFIG_FILE = SCRIPT_DIR / "SystemConfig.txt"


def load_serial_port(default_port: str = "COM6") -> str:
    if not CONFIG_FILE.exists():
        return default_port

    try:
        with CONFIG_FILE.open("r", encoding="utf-8") as config:
            for raw_line in config:
                line = raw_line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                if key.strip() == "serial_port":
                    port = value.strip()
                    return port if port else default_port
    except Exception:
        pass

    return default_port


def main() -> int:
    port = load_serial_port()
    baudrate = 115200

    status_request_obj = {
        "method": "system_usb_status",
        "params": {},
    }
    status_request_line = json.dumps(status_request_obj) + "\r\n"

    echo_request_obj = {
        "method": "echo",
        "params": {"message": "system_test"},
    }
    echo_request_line = json.dumps(echo_request_obj) + "\r\n"

    print("=" * 60)
    print("DCC_tester RPC Echo System Test")
    print("=" * 60)
    print(f"Port: {port}")
    print(f"Baud: {baudrate}")
    print()

    try:
        with serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=2,
            write_timeout=5,
        ) as ser:
            time.sleep(0.3)
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            print("Preflight: Query USB status")
            print(f"-> {status_request_line.strip()}")
            ser.write(status_request_line.encode("utf-8"))
            ser.flush()

            response_line = ser.readline().decode("utf-8", errors="replace").strip()
            print(f"<- {response_line}")

            if not response_line:
                print("\nFAIL: No response to system_usb_status.")
                return 1

            try:
                status_response = json.loads(response_line)
            except json.JSONDecodeError:
                print("\nFAIL: system_usb_status response is not valid JSON.")
                return 1

            if status_response.get("status") != "ok":
                print("\nFAIL: system_usb_status returned error.")
                return 1

            usb_status = status_response.get("usb", {})
            device_configured = bool(usb_status.get("device_configured", False))
            cdc_active = bool(usb_status.get("cdc_active", False))

            print(f"USB configured: {device_configured}")
            print(f"CDC active:     {cdc_active}")

            if not device_configured or not cdc_active:
                print("\nFAIL: USB preflight not ready (device not configured or CDC inactive).")
                return 1

            print("\nPreflight OK: Running echo test")
            print(f"-> {echo_request_line.strip()}")
            ser.write(echo_request_line.encode("utf-8"))
            ser.flush()

            response_line = ser.readline().decode("utf-8", errors="replace").strip()
            print(f"<- {response_line}")

            if not response_line:
                print("\nFAIL: No response to echo request.")
                return 1

            try:
                response = json.loads(response_line)
            except json.JSONDecodeError:
                print("\nFAIL: Echo response is not valid JSON.")
                return 1

            if response.get("status") == "ok" and "echo" in response:
                print("\nPASS: RPC echo successful.")
                return 0

            print("\nFAIL: Unexpected RPC response payload.")
            return 1

    except serial.SerialException as exc:
        print(f"\nFAIL: Serial error: {exc}")
        return 1
    except Exception as exc:
        print(f"\nFAIL: Unexpected error: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
