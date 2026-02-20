#!/usr/bin/env python3
"""
RunSetCommandStationParameters Script
=====================================

This script sets command station parameters via RPC.
Configuration is read from RunSetCommandStationParametersConfig.txt.
"""

import json
import os
import sys
import time
import serial


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
        if LOG_LEVEL == 2:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] {message}")
        else:
            print(message)


class DCCTesterRPC:
    """RPC client for DCC_tester command station."""

    def __init__(self, port, baudrate=115200, timeout=2):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.5)  # Allow time for connection to establish

    def send_rpc(self, method, params):
        request = {
            "method": method,
            "params": params,
        }
        request_json = json.dumps(request) + "\r\n"
        log(2, f"→ {request_json.strip()}")
        self.ser.write(request_json.encode("utf-8"))

        response_line = self.ser.readline().decode("utf-8").strip()
        log(2, f"← {response_line}")
        if response_line:
            return json.loads(response_line)
        return None

    def close(self):
        self.ser.close()


def _parse_bool(value, key):
    if isinstance(value, bool):
        return value
    if value is None:
        raise ValueError(f"Missing boolean value for '{key}'")
    normalized = str(value).strip().lower()
    if normalized in {"y", "yes", "true", "1"}:
        return True
    if normalized in {"n", "no", "false", "0"}:
        return False
    raise ValueError(f"Invalid boolean value for '{key}': {value}")


def _parse_int(value, key):
    if value is None or str(value).strip() == "":
        raise ValueError(f"Missing integer value for '{key}'")
    try:
        return int(str(value).strip())
    except ValueError as exc:
        raise ValueError(f"Invalid integer value for '{key}': {value}") from exc


def load_config(config_path):
    """Load configuration from a simple key=value text file."""
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Configuration file not found: {config_path}")

    config = {}
    with open(config_path, "r", encoding="utf-8") as config_file:
        for raw_line in config_file:
            line = raw_line.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            if "=" not in line:
                raise ValueError(f"Invalid config line (expected key=value): {raw_line.strip()}")
            key, value = line.split("=", 1)
            config[key.strip()] = value.strip()

    required_keys = {
        "logging_level",
        "serial_port",
        "preamble_bits",
        "bit1_duration",
        "bit0_duration",
        "bidi_enable",
        "trigger_first_bit",
    }

    missing = sorted(required_keys - set(config.keys()))
    if missing:
        raise ValueError(f"Missing required config keys: {', '.join(missing)}")

    return {
        "logging_level": _parse_int(config.get("logging_level"), "logging_level"),
        "serial_port": config.get("serial_port"),
        "preamble_bits": _parse_int(config.get("preamble_bits"), "preamble_bits"),
        "bit1_duration": _parse_int(config.get("bit1_duration"), "bit1_duration"),
        "bit0_duration": _parse_int(config.get("bit0_duration"), "bit0_duration"),
        "bidi_enable": _parse_bool(config.get("bidi_enable"), "bidi_enable"),
        "trigger_first_bit": _parse_bool(config.get("trigger_first_bit"), "trigger_first_bit"),
    }


def main():
    print("=" * 70)
    print("DCC Command Station Parameter Setter")
    print("=" * 70)
    print()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "RunSetCommandStationParametersConfig.txt")

    try:
        config = load_config(config_path)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}")
        print("Please update RunSetCommandStationParametersConfig.txt with valid values.")
        return 1

    set_log_level(config["logging_level"])

    log(1, "")
    log(1, "=" * 70)
    log(1, "Configuration Summary:")
    log(1, "=" * 70)
    log(1, "System Parameters:")
    log(1, f"  Serial port:        {config['serial_port']}")
    log(1, f"  Logging level:      {config['logging_level']}")
    log(1, "")
    log(1, "Command Station Parameters:")
    log(1, f"  Preamble bits:      {config['preamble_bits']}")
    log(1, f"  Bit1 duration:      {config['bit1_duration']} us")
    log(1, f"  Bit0 duration:      {config['bit0_duration']} us")
    log(1, f"  BiDi enable:        {config['bidi_enable']}")
    log(1, f"  Trigger first bit:  {config['trigger_first_bit']}")
    log(1, "=" * 70)
    log(1, "")

    try:
        log(2, f"Connecting to {config['serial_port']}...")
        rpc = DCCTesterRPC(config["serial_port"])
        log(2, "✓ Connected!\n")

        params = {
            "preamble_bits": config["preamble_bits"],
            "bit1_duration": config["bit1_duration"],
            "bit0_duration": config["bit0_duration"],
            "bidi_enable": config["bidi_enable"],
            "trigger_first_bit": config["trigger_first_bit"],
        }

        response = rpc.send_rpc("command_station_params", params)
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set parameters: {response}")
            rpc.close()
            return 1

        log(1, "✓ Command station parameters updated")

        response = rpc.send_rpc("command_station_get_params", {})
        if response is not None and response.get("status") == "ok":
            params_out = response.get("parameters", {})
            log(1, "")
            log(1, "Current Parameters:")
            log(1, f"  Track voltage:      {params_out.get('track_voltage')}")
            log(1, f"  Preamble bits:      {params_out.get('preamble_bits')}")
            log(1, f"  Bit1 duration:      {params_out.get('bit1_duration')} us")
            log(1, f"  Bit0 duration:      {params_out.get('bit0_duration')} us")
            log(1, f"  BiDi enable:        {params_out.get('bidi_enable')}")
            log(1, f"  BiDi DAC:           {params_out.get('bidi_dac')}")
            log(1, f"  Trigger first bit:  {params_out.get('trigger_first_bit')}")
            log(1, "")

        rpc.close()
        return 0

    except serial.SerialException as exc:
        log(1, f"ERROR: Serial port error: {exc}")
        log(1, f"Make sure {config['serial_port']} is the correct port and the device is connected.")
        return 1
    except KeyboardInterrupt:
        log(1, "\n\nOperation interrupted by user.")
        return 1
    except Exception as exc:
        log(1, f"\nERROR: Unexpected error: {exc}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
