#!/usr/bin/env python3
"""
ReadManufacturerIdCv Script
===========================

Best-effort read of CV8 (manufacturer ID) using service mode direct bit verify
packets and current feedback for ACK detection.
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
        print(message)


class DCCTesterRPC:
    """RPC client for DCC_tester command station."""

    def __init__(self, port, baudrate=115200, timeout=2):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.5)

    def send_rpc(self, method, params, quiet=False):
        request = {
            "method": method,
            "params": params,
        }
        request_json = json.dumps(request) + "\r\n"
        if not quiet:
            log(2, f"→ {request_json.strip()}")
        self.ser.write(request_json.encode("utf-8"))

        response_line = self.ser.readline().decode("utf-8").strip()
        if not quiet:
            log(2, f"← {response_line}")
        if response_line:
            return json.loads(response_line)
        return None

    def close(self):
        self.ser.close()


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
        "serial_port",
        "cv_number",
        "ack_current_threshold_ma",
        "ack_window_ms",
        "repeats_per_bit",
        "inter_packet_delay_ms",
        "preamble_bits",
        "logging_level",
    }

    missing = sorted(required_keys - set(config.keys()))
    if missing:
        raise ValueError(f"Missing required config keys: {', '.join(missing)}")

    return {
        "serial_port": config.get("serial_port"),
        "cv_number": _parse_int(config.get("cv_number"), "cv_number"),
        "ack_current_threshold_ma": _parse_int(config.get("ack_current_threshold_ma"), "ack_current_threshold_ma"),
        "ack_window_ms": _parse_int(config.get("ack_window_ms"), "ack_window_ms"),
        "repeats_per_bit": _parse_int(config.get("repeats_per_bit"), "repeats_per_bit"),
        "inter_packet_delay_ms": _parse_int(config.get("inter_packet_delay_ms"), "inter_packet_delay_ms"),
        "preamble_bits": _parse_int(config.get("preamble_bits"), "preamble_bits"),
        "logging_level": _parse_int(config.get("logging_level"), "logging_level"),
    }


def calculate_dcc_checksum(bytes_list):
    checksum = 0
    for byte in bytes_list:
        checksum ^= byte
    return checksum


def make_direct_bit_verify_packet(cv_number, bit_index, bit_value):
    if cv_number < 1 or cv_number > 1024:
        raise ValueError("cv_number must be in range 1-1024")
    if bit_index < 0 or bit_index > 7:
        raise ValueError("bit_index must be in range 0-7")
    if bit_value not in (0, 1):
        raise ValueError("bit_value must be 0 or 1")

    cv_addr = cv_number - 1
    addr_high = (cv_addr >> 8) & 0x03
    addr_low = cv_addr & 0xFF

    # Service mode direct bit verify: 0b011111AA, data byte 0b1110DBBB
    instruction = 0x7C | addr_high
    data = 0xE0 | (bit_value << 3) | (bit_index & 0x07)

    packet = [instruction, addr_low, data]
    checksum = calculate_dcc_checksum(packet)
    packet.append(checksum)

    return packet


def make_reset_packet():
    # Service mode reset packet (3-byte reset)
    return [0x00, 0x00, 0x00]


def send_reset_and_verify_packets(rpc, verify_packet, inter_packet_delay_ms):
    reset_packet = make_reset_packet()

    response = rpc.send_rpc("command_station_load_packet", {"bytes": reset_packet, "replace": True})
    if response is None or response.get("status") != "ok":
        raise RuntimeError(f"Failed to load reset packet 1: {response}")

    response = rpc.send_rpc("command_station_load_packet", {"bytes": reset_packet, "replace": False})
    if response is None or response.get("status") != "ok":
        raise RuntimeError(f"Failed to load reset packet 2: {response}")

    response = rpc.send_rpc("command_station_load_packet", {"bytes": verify_packet, "replace": False})
    if response is None or response.get("status") != "ok":
        raise RuntimeError(f"Failed to load verify packet: {response}")

    response = rpc.send_rpc(
        "command_station_transmit_packet",
        {"count": 3, "delay_ms": inter_packet_delay_ms},
    )
    if response is None or response.get("status") != "ok":
        raise RuntimeError(f"Failed to transmit reset+verify packets: {response}")


def send_packet(rpc, packet_bytes):
    response = rpc.send_rpc("command_station_load_packet", {"bytes": packet_bytes, "replace": True})
    if response is None or response.get("status") != "ok":
        return False, response

    response = rpc.send_rpc("command_station_transmit_packet", {"delay_ms": 0})
    if response is None or response.get("status") != "ok":
        return False, response

    return True, response


def read_current_ma(rpc):
    response = rpc.send_rpc("get_current_feedback_ma", {})
    if response is None or response.get("status") != "ok":
        return None
    return response.get("current_ma")


def wait_for_button_press(rpc):
    log(1, "Press USER button to continue...")
    while True:
        response = rpc.send_rpc("get_gpio_input", {"pin": 16}, quiet=True)
        if response is not None and response.get("status") == "ok":
            if response.get("value") == 1:
                time.sleep(0.1)
                return
        time.sleep(0.05)


def detect_ack(rpc, baseline_ma, threshold_ma, window_ms, bit_index=None):
    if bit_index is not None:
        log(1, f"Checking ACK for bit {bit_index} (baseline {baseline_ma} mA)")
    end_time = time.monotonic() + (window_ms / 1000.0)
    while time.monotonic() < end_time:
        current_ma = read_current_ma(rpc)
        if current_ma is None:
            return False
        if bit_index is not None:
            log(1, f"  Bit {bit_index} current: {current_ma} mA")
        if current_ma >= baseline_ma + threshold_ma:
            return True
        time.sleep(0.002)
    return False


def read_cv_bits(rpc, cv_number, repeats_per_bit, inter_packet_delay_ms, ack_threshold_ma, ack_window_ms):
    bits = []
    for bit_index in range(8):
        bit_is_one = False
        for attempt in range(repeats_per_bit):
            baseline_ma = read_current_ma(rpc)
            if baseline_ma is None:
                raise RuntimeError("Failed to read current feedback")

            packet = make_direct_bit_verify_packet(cv_number, bit_index, 1)
            if attempt == 0:
                send_reset_and_verify_packets(rpc, packet, inter_packet_delay_ms)
            else:
                ok, response = send_packet(rpc, packet)
                if not ok:
                    raise RuntimeError(f"Failed to transmit verify packet: {response}")

            if detect_ack(rpc, baseline_ma, ack_threshold_ma, ack_window_ms, bit_index=bit_index):
                bit_is_one = True
                log(1, f"ACK detected for bit {bit_index} (value=1)")
                break

            # time.sleep(inter_packet_delay_ms / 1000.0)

        bits.append(1 if bit_is_one else 0)
        log(1, f"Bit {bit_index}: {1 if bit_is_one else 0}")
        if not bit_is_one:
            raise RuntimeError(f"No ACK detected for bit {bit_index} after {repeats_per_bit} attempts")
        # wait_for_button_press(rpc)

    return bits


def bits_to_value(bits):
    value = 0
    for bit_index, bit in enumerate(bits):
        if bit:
            value |= (1 << bit_index)
    return value


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "ReadManufacturerIdCvConfig.txt")

    print("=" * 70)
    print("DCC CV8 Manufacturer ID Reader")
    print("=" * 70)

    try:
        config = load_config(config_path)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}")
        print("Please update ReadManufacturerIdCvConfig.txt with valid values.")
        return 1

    set_log_level(config["logging_level"])

    log(1, "")
    log(1, "=" * 70)
    log(1, "Configuration Summary:")
    log(1, "=" * 70)
    log(1, f"  Serial port:             {config['serial_port']}")
    log(1, f"  CV number:               {config['cv_number']}")
    log(1, f"  Ack threshold:           {config['ack_current_threshold_ma']} mA")
    log(1, f"  Ack window:              {config['ack_window_ms']} ms")
    log(1, f"  Repeats per bit:          {config['repeats_per_bit']}")
    log(1, f"  Inter-packet delay:       {config['inter_packet_delay_ms']} ms")
    log(1, f"  Preamble bits (service):  {config['preamble_bits']}")
    log(1, f"  Logging level:            {config['logging_level']}")
    log(1, "=" * 70)
    log(1, "")

    rpc = None
    original_preamble = None

    try:
        rpc = DCCTesterRPC(config["serial_port"])
        log(1, "Connected to DCC_tester")

        log(1, "Wait start up stabilization...")

        response = rpc.send_rpc("command_station_get_params", {})
        if response is not None and response.get("status") == "ok":
            original_preamble = response.get("parameters", {}).get("preamble_bits")

        response = rpc.send_rpc("command_station_params", {"preamble_bits": config["preamble_bits"]})
        if response is None or response.get("status") != "ok":
            raise RuntimeError(f"Failed to set preamble bits: {response}")

        response = rpc.send_rpc("command_station_start", {"loop": 0})
        if response is None or response.get("status") != "ok":
            raise RuntimeError(f"Failed to start command station: {response}")

        time.sleep(1.0)

        bits = read_cv_bits(
            rpc,
            config["cv_number"],
            config["repeats_per_bit"],
            config["inter_packet_delay_ms"],
            config["ack_current_threshold_ma"],
            config["ack_window_ms"],
        )

        value = bits_to_value(bits)

        log(1, "")
        log(1, "=" * 70)
        log(1, "Manufacturer ID Read Result")
        log(1, "=" * 70)
        log(1, f"  CV{config['cv_number']} value: {value} (0x{value:02X})")
        log(1, "" )

        return 0

    except serial.SerialException as exc:
        log(1, f"ERROR: Serial port error: {exc}")
        log(1, f"Make sure {config['serial_port']} is correct and the device is connected.")
        return 1
    except KeyboardInterrupt:
        log(1, "\nOperation interrupted by user.")
        return 1
    except Exception as exc:
        log(1, f"ERROR: {exc}")
        return 1
    finally:
        if rpc is not None:
            rpc.send_rpc("command_station_stop", {})
            if original_preamble is not None:
                rpc.send_rpc("command_station_params", {"preamble_bits": original_preamble})
            rpc.close()


if __name__ == "__main__":
    sys.exit(main())
