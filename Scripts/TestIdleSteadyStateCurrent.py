#!/usr/bin/env python3
"""
TestIdleSteadyStateCurrent Script
================================

Starts the DCC command station (which automatically emits idle packets via the
DCC library when no user packet is queued) and measures steady-state current
for the configured duration, then reports average current, max absolute delta
from the average, and the worst-case reading.

Configuration:
  - SystemConfig.txt                          (serial port, logging level, file logging)
  - TestIdleSteadyStateCurrentConfig.txt      (duration, sample interval)
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
TEST_DURATION_SECONDS = 30.0
SAMPLE_INTERVAL_MS = 100
CONFIG_FILE_NAME = "TestIdleSteadyStateCurrentConfig.txt"


def _parse_int(value, key, minimum=None):
    try:
        parsed = int(str(value).strip(), 0)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid integer value for '{key}': {value}") from exc
    if minimum is not None and parsed < minimum:
        raise ValueError(f"'{key}' must be >= {minimum}, got {parsed}")
    return parsed


def _parse_float(value, key, minimum=None):
    try:
        parsed = float(str(value).strip())
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Invalid numeric value for '{key}': {value}") from exc
    if minimum is not None and parsed < minimum:
        raise ValueError(f"'{key}' must be >= {minimum}, got {parsed}")
    return parsed


def load_test_config(config_path):
    config = {
        "test_duration_seconds": TEST_DURATION_SECONDS,
        "sample_interval_ms": SAMPLE_INTERVAL_MS,
    }

    if not os.path.exists(config_path):
        return config

    raw_config = {}
    with open(config_path, "r", encoding="utf-8") as cfg:
        for raw in cfg:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            if "=" not in line:
                raise ValueError(f"Invalid config line (expected key=value): {raw.strip()}")
            key, value = line.split("=", 1)
            raw_config[key.strip()] = value.strip()

    if "test_duration_seconds" in raw_config:
        config["test_duration_seconds"] = _parse_float(
            raw_config["test_duration_seconds"], "test_duration_seconds", minimum=1.0
        )
    if "sample_interval_ms" in raw_config:
        config["sample_interval_ms"] = _parse_int(
            raw_config["sample_interval_ms"], "sample_interval_ms", minimum=1
        )

    return config


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
    response = rpc.send_rpc("get_current_feedback_ma", {}, quiet=True)
    if response is None or response.get("status") != "ok":
        return None
    try:
        return float(response.get("current_ma"))
    except (TypeError, ValueError):
        return None


def main():
    print("=" * 70)
    print("Idle Packet Steady-State Current Test")
    print("=" * 70)

    sys_cfg = System.get_config()
    port = sys_cfg.serial_port
    config_path = os.path.join(script_dir, CONFIG_FILE_NAME)
    try:
        test_cfg = load_test_config(config_path)
    except ValueError as exc:
        print(f"ERROR: {exc}")
        print(f"Please update {CONFIG_FILE_NAME} with valid values.")
        return 1

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
    log(1, f"  Test duration:      {test_cfg['test_duration_seconds']:.1f} s")
    log(1, f"  Sample interval:    {test_cfg['sample_interval_ms']} ms")
    log(1, f"  Logging level:      {sys_cfg.logging_level}")
    log(1, "")

    rpc = None
    samples = []
    read_failures = 0

    try:
        rpc = DCCTesterRPC(port)
        log(1, "Connected to DCC_tester")

        response = rpc.send_rpc("command_station_start", {"loop": 0})
        if response is None or response.get("status") != "ok":
            raise RuntimeError(f"Failed to start command station: {response}")

        log(1, "Command station started (DCC library emits idle packets automatically)")
        log(1, "Running steady-state current sampling...")
        start = time.monotonic()
        sample_index = 0

        while (time.monotonic() - start) < test_cfg["test_duration_seconds"]:
            cycle_start = time.monotonic()

            current_ma = read_current_ma(rpc)
            sample_index += 1
            if current_ma is None:
                read_failures += 1
                log(1, f"#{sample_index:04d} current: read failed")
            else:
                samples.append(current_ma)
                log(2, f"#{sample_index:04d} current: {current_ma:.2f} mA")

            elapsed_ms = (time.monotonic() - cycle_start) * 1000.0
            sleep_ms = max(0.0, test_cfg["sample_interval_ms"] - elapsed_ms)
            time.sleep(sleep_ms / 1000.0)

        if not samples:
            raise RuntimeError("No valid current samples collected")

        avg_ma = sum(samples) / len(samples)
        worst_case_ma = max(samples, key=lambda x: abs(x - avg_ma))
        worst_case_delta_ma = worst_case_ma - avg_ma
        max_delta_ma = abs(worst_case_delta_ma)

        log(1, "")
        log(1, "=" * 70)
        log(1, "Steady-State Current Results")
        log(1, "=" * 70)
        log(1, f"  Duration:            {test_cfg['test_duration_seconds']:.1f} s")
        log(1, f"  Valid samples:       {len(samples)}")
        log(1, f"  Read failures:       {read_failures}")
        log(1, f"  Average current:     {avg_ma:.2f} mA")
        log(1, f"  Max delta (abs):     {max_delta_ma:.2f} mA")
        log(1, f"  Worst-case reading:  {worst_case_ma:.2f} mA")
        log(1, f"  Worst-case delta:    {worst_case_delta_ma:+.2f} mA")
        log(1, "=" * 70)
        return 0

    except serial.SerialException as exc:
        log(1, f"ERROR: Serial port error: {exc}")
        log(1, f"Check serial_port in SystemConfig.txt (current: {port}).")
        return 1
    except KeyboardInterrupt:
        log(1, "\nStopped by user.")
        return 1
    except Exception as exc:
        log(1, f"ERROR: {exc}")
        return 1
    finally:
        if rpc is not None:
            try:
                rpc.send_rpc("command_station_stop", {}, quiet=True)
            except Exception:
                pass
            rpc.close()
        if file_logging_started:
            System.stop_logging(close_file=True)


if __name__ == "__main__":
    sys.exit(main())
