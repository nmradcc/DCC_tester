#!/usr/bin/env python3
"""
TestBaselineCurrent Script
==========================

Continuously reads and logs baseline current every 100 ms.
Stop with Ctrl+C.

Configuration:
  - SystemConfig.txt (serial port, logging level, file logging)
"""

import json
import os
import sys
import time
from datetime import datetime

import serial

script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)
import System


LOG_LEVEL = 1
SAMPLE_INTERVAL_MS = 100


def set_log_level(level):
    global LOG_LEVEL
    try:
        level_int = int(level)
    except (TypeError, ValueError):
        level_int = 1
    LOG_LEVEL = max(0, min(2, level_int))


def log(level, message):
    if LOG_LEVEL >= level:
        if LOG_LEVEL >= 2:
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{ts}] {message}")
        else:
            print(message)


class DCCTesterRPC:
    def __init__(self, port, baudrate=115200, timeout=2):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.5)

    def send_rpc(self, method, params, quiet=False):
        request = {"method": method, "params": params}
        request_json = json.dumps(request) + "\r\n"
        if not quiet:
            log(2, f"-> {request_json.strip()}")
        self.ser.write(request_json.encode("utf-8"))

        response_line = self.ser.readline().decode("utf-8").strip()
        if not quiet:
            log(2, f"<- {response_line}")
        if response_line:
            return json.loads(response_line)
        return None

    def close(self):
        self.ser.close()


def read_current_ma(rpc):
    response = rpc.send_rpc(
        "get_current_feedback_ma",
        {"num_samples": 1, "sample_delay_ms": 1},
        quiet=True,
    )
    if response is None or response.get("status") != "ok":
        return None
    return response.get("current_ma")


def main():
    print("=" * 70)
    print("Baseline Current Monitor")
    print("=" * 70)

    sys_cfg = System.get_config()
    port = sys_cfg.serial_port

    file_logging_started = False
    if getattr(sys_cfg, "save_logs", False):
        if hasattr(System, "_logging_active"):
            System._logging_active = True
        System.start_logging()
        file_logging_started = True

    set_log_level(sys_cfg.logging_level)

    if file_logging_started:
        log(1, "File logging enabled from SystemConfig.txt")

    log(1, "")
    log(1, "Configuration Summary:")
    log(1, f"  Serial port:        {port}")
    log(1, f"  Sample interval:    {SAMPLE_INTERVAL_MS} ms")
    log(1, f"  Logging level:      {sys_cfg.logging_level}")
    log(1, "")

    rpc = None
    sample_index = 0

    try:
        rpc = DCCTesterRPC(port)
        log(1, "Connected to DCC_tester")
        log(1, "Sampling baseline current. Press Ctrl+C to stop.")

        while True:
            sample_start = time.monotonic()
            current_ma = read_current_ma(rpc)
            sample_index += 1

            if current_ma is None:
                log(1, f"#{sample_index:06d} baseline current: read failed")
            else:
                log(1, f"#{sample_index:06d} baseline current: {float(current_ma):.2f} mA")

            elapsed_ms = (time.monotonic() - sample_start) * 1000.0
            sleep_ms = max(0.0, SAMPLE_INTERVAL_MS - elapsed_ms)
            time.sleep(sleep_ms / 1000.0)

    except serial.SerialException as exc:
        log(1, f"ERROR: Serial port error: {exc}")
        log(1, f"Check serial_port in SystemConfig.txt (current: {port}).")
        return 1
    except KeyboardInterrupt:
        log(1, "\nStopped by user.")
        return 0
    except Exception as exc:
        log(1, f"ERROR: {exc}")
        return 1
    finally:
        if rpc is not None:
            rpc.close()
        if file_logging_started:
            System.stop_logging(close_file=True)


if __name__ == "__main__":
    sys.exit(main())
