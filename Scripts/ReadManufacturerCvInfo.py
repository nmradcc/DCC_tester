#!/usr/bin/env python3
"""
ReadManufacturerCvInfo Script
=============================

Reads required DCC manufacturer information CVs using service-mode direct bit
verify packets with current-based ACK detection.

Configuration:
    - SystemConfig.txt                 global settings (serial port, logging, ACK/preamble tuning)
    - ReadManufacturerCvInfoConfig.txt  test-specific settings (CV selection only)
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


def _parse_int(value, key):
    if value is None or str(value).strip() == "":
        raise ValueError(f"Missing integer value for '{key}'")
    try:
        return int(str(value).strip(), 0)
    except ValueError as exc:
        raise ValueError(f"Invalid integer value for '{key}': {value}") from exc


def load_config(config_path):
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Configuration file not found: {config_path}")

    config = {}
    with open(config_path, "r", encoding="utf-8") as cfg:
        for raw in cfg:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            if "=" not in line:
                raise ValueError(f"Invalid config line (expected key=value): {raw.strip()}")
            key, value = line.split("=", 1)
            config[key.strip()] = value.strip()

    required = {
        "cv8_expected_manufacturer_id",
    }
    missing = sorted(required - set(config.keys()))
    if missing:
        raise ValueError(f"Missing required config keys: {', '.join(missing)}")

    cfg = {
        "cv8_number": 8,
        "cv8_expected_manufacturer_id": _parse_int(
            config["cv8_expected_manufacturer_id"], "cv8_expected_manufacturer_id"
        ),
        "cv7_number": 7,
    }

    if cfg["cv8_expected_manufacturer_id"] < -1 or cfg["cv8_expected_manufacturer_id"] > 255:
        raise ValueError("cv8_expected_manufacturer_id must be -1 (skip check) or 0..255")

    return cfg


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

    # Service mode direct bit verify format: 0b011110AA, data 0b1110DBBB.
    instruction = 0x78 | addr_high
    data = 0xE0 | ((bit_value & 0x01) << 3) | (bit_index & 0x07)

    packet = [instruction, addr_low, data]
    packet.append(calculate_dcc_checksum(packet))
    return packet


def send_verify(rpc, verify_packet, delay_ms, trigger_first_bit=True):

    log(2, "Queueing service-mode packet sequence:")
    log(2, f"  verify:   {[f'0x{b:02X}' for b in verify_packet]}")
    log(2, f"  tx: count=3, delay_ms={delay_ms}")

    response = rpc.send_rpc("command_station_load_packet", {"bytes": verify_packet, "replace": True})
    if response is None or response.get("status") != "ok":
        raise RuntimeError(f"Failed to load verify packet: {response}")

    response = rpc.send_rpc(
        "command_station_transmit_packet",
        {"count": 2, "delay_ms": delay_ms, "trigger_first_bit": trigger_first_bit},
    )
    if response is None or response.get("status") != "ok":
        raise RuntimeError(f"Failed to transmit reset+verify packets: {response}")


def detect_ack_firmware(rpc, cfg):
    response = rpc.send_rpc(
        "command_station_detect_ack",
        {
            "baseline_samples": cfg["baseline_samples"],
            "baseline_sample_delay_ms": cfg["baseline_sample_delay_ms"],
            "ack_window_ms": cfg["ack_window_ms"],
            "ack_poll_interval_ms": cfg["ack_poll_interval_ms"],
            "ack_threshold_ma": cfg["ack_current_threshold_ma"],
        },
        quiet=True,
    )

    if response is None:
        return None, None

    if response.get("status") != "ok":
        return None, response

    ack_detected = bool(response.get("ack_detected", False))
    ack_details = {
        "baseline_ma": float(response.get("baseline_ma", 0.0)),
        "threshold_ma": float(response.get("threshold_ma", cfg["ack_current_threshold_ma"])),
        "target_ma": float(response.get("target_ma", 0.0)),
        "peak_ma": float(response.get("peak_ma", 0.0)),
        "peak_delta_ma": float(response.get("peak_delta_ma", 0.0)),
        "samples": int(response.get("samples", 0)),
        "first_over_target_ms": float(response.get("first_crossing_ms", 0.0)) if response.get("first_crossing_ms", 0) > 0 else None,
        "ack_reason": str(response.get("ack_reason", "none")),
    }
    return ack_detected, ack_details


def verify_bit_value(rpc, cv_number, bit_index, bit_value, cfg):
    verify_packet = make_direct_bit_verify_packet(cv_number, bit_index, bit_value)
    log(2, "-" * 70)
    log(2, f"Verify bit request: CV{cv_number} bit {bit_index} == {bit_value}")
    log(2, f"Verify packet bytes: {[f'0x{b:02X}' for b in verify_packet]}")

    for attempt in range(cfg["repeats_per_verify"]):
        log(2, f"Attempt {attempt + 1}/{cfg['repeats_per_verify']}")
        send_verify(
            rpc,
            verify_packet,
            cfg["inter_packet_delay_ms"],
            trigger_first_bit=True,
        )
        ack_detected, ack_details = detect_ack_firmware(rpc, cfg)

        if ack_detected is None:
            error_message = ack_details
            if isinstance(ack_details, dict):
                error_message = ack_details.get("message", ack_details)
            raise RuntimeError(f"Firmware ACK RPC failed/unavailable: {error_message}")

        if ack_detected:
            if ack_details["first_over_target_ms"] is not None:
                log(2, f"  ACK detected at +{ack_details['first_over_target_ms']:.2f} ms")
            else:
                reason = ack_details.get("ack_reason", "peak-delta fallback")
                log(2, f"  ACK detected by {reason}")
            log(
                2,
                "  ACK stats: "
                f"samples={ack_details['samples']}, "
                f"peak={ack_details['peak_ma']:.2f} mA, "
                f"delta={ack_details['peak_delta_ma']:.2f} mA",
            )
            log(2, f"Bit {bit_index} verify {bit_value}: ACK on attempt {attempt + 1}")
            return True

        log(
            2,
            "  No ACK in window: "
            f"samples={ack_details['samples']}, "
            f"peak={ack_details['peak_ma']:.2f} mA, "
            f"delta={ack_details['peak_delta_ma']:.2f} mA, "
            f"target={ack_details['target_ma']:.2f} mA",
        )

    log(2, f"Verify bit request failed: CV{cv_number} bit {bit_index} == {bit_value}")
    return False


def read_cv_value(rpc, cv_number, cfg):
    bits = []

    for bit_index in range(8):
        log(2, "=" * 70)
        log(2, f"Reading CV{cv_number} bit {bit_index}")
        ack_for_one = verify_bit_value(rpc, cv_number, bit_index, 1, cfg)
        if ack_for_one:
            bits.append(1)
            log(1, f"Bit {bit_index}: 1")
            continue

        ack_for_zero = verify_bit_value(rpc, cv_number, bit_index, 0, cfg)
        if ack_for_zero:
            bits.append(0)
            log(1, f"Bit {bit_index}: 0")
            continue

        raise RuntimeError(
            f"No ACK for bit {bit_index} verifying 1 or 0; check wiring, decoder, and thresholds"
        )

    value = 0
    for bit_index, bit in enumerate(bits):
        if bit:
            value |= (1 << bit_index)

    return value, bits


def read_and_report_cv(rpc, cv_number, cfg, label, expected_value=None):
    value, bits = read_cv_value(rpc, cv_number, cfg)
    log(1, "")
    log(1, "=" * 70)
    log(1, f"{label} Result")
    log(1, "=" * 70)
    log(1, f"  CV{cv_number} bits (LSB->MSB): {''.join(str(b) for b in bits)}")
    log(1, f"  CV{cv_number} value: {value} (0x{value:02X})")

    if expected_value is None:
        log(1, "  Verdict: READ ONLY")
        return 0

    if value == expected_value:
        log(1, f"  Verdict: PASS (matches expected {label.lower()})")
        return 0

    log(1, f"  Verdict: FAIL (does not match expected {label.lower()})")
    return 2


def main():
    print("=" * 70)
    print("Read Manufacturer CV Info")
    print("=" * 70)

    config_path = os.path.join(script_dir, "ReadManufacturerCvInfoConfig.txt")
    try:
        cfg = load_config(config_path)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}")
        print("Please update ReadManufacturerCvInfoConfig.txt with valid values.")
        return 1

    sys_cfg = System.get_config()
    port = sys_cfg.serial_port
    cfg["ack_current_threshold_ma"] = sys_cfg.ack_current_threshold_ma
    cfg["ack_window_ms"] = sys_cfg.ack_window_ms
    cfg["ack_poll_interval_ms"] = sys_cfg.ack_poll_interval_ms
    cfg["baseline_sample_delay_ms"] = sys_cfg.baseline_sample_delay_ms
    cfg["baseline_samples"] = sys_cfg.baseline_samples
    cfg["repeats_per_verify"] = sys_cfg.repeats_per_verify
    cfg["service_preamble_bits"] = sys_cfg.service_preamble_bits
    cfg["inter_packet_delay_ms"] = 0

    # When this script is run directly, opt into the same file-logging
    # behavior used by the System menu flow.
    file_logging_started = False
    if getattr(sys_cfg, "save_logs", False):
        if hasattr(System, "_logging_active"):
            System._logging_active = True
        System.start_logging()
        file_logging_started = True

    set_log_level(sys_cfg.logging_level)

    if file_logging_started:
        log(1, "File logging enabled from SystemConfig.txt")
    else:
        log(2, "File logging disabled (set save_logs=true in SystemConfig.txt to enable)")

    log(1, "")
    log(1, "=" * 70)
    log(1, "Configuration Summary:")
    log(1, "=" * 70)
    log(1, f"  Serial port:            {port}")
    log(1, f"  CV8 expected ID:         {cfg['cv8_expected_manufacturer_id'] if cfg['cv8_expected_manufacturer_id'] >= 0 else '(not checked)'}")
    log(1, f"  ACK threshold:          {cfg['ack_current_threshold_ma']} mA")
    log(1, f"  ACK window:             {cfg['ack_window_ms']} ms")
    log(1, f"  ACK poll interval:      {cfg['ack_poll_interval_ms']} ms")
    log(1, f"  Baseline sample delay:  {cfg['baseline_sample_delay_ms']} ms")
    log(1, f"  Baseline samples:       {cfg['baseline_samples']}")
    log(1, f"  Repeats per verify:     {cfg['repeats_per_verify']}")
    log(1, f"  Inter-packet delay:     {cfg['inter_packet_delay_ms']} ms")
    log(1, f"  Service preamble bits:  {cfg['service_preamble_bits']}")
    log(1, "=" * 70)
    log(1, "")

    log(2, "Verbose logging enabled: detailed ACK/current diagnostics are active")

    rpc = None
    original_preamble = None
    original_trigger_first_bit = None
    try:
        rpc = DCCTesterRPC(port)
        log(1, "Connected to DCC_tester")

        response = rpc.send_rpc("command_station_get_params", {})
        if response is not None and response.get("status") == "ok":
            parameters = response.get("parameters", {})
            original_preamble = parameters.get("preamble_bits")
            original_trigger_first_bit = parameters.get("trigger_first_bit")
            log(2, f"Original preamble bits: {original_preamble}")
            log(2, f"Original trigger_first_bit: {original_trigger_first_bit}")
        else:
            log(2, f"Could not query original command station params: {response}")

        response = rpc.send_rpc("command_station_params", {"preamble_bits": cfg["service_preamble_bits"]})
        if response is None or response.get("status") != "ok":
            raise RuntimeError(f"Failed to set command station params: {response}")
        log(2, f"Applied service-mode preamble bits: {cfg['service_preamble_bits']}")

        response = rpc.send_rpc("command_station_params", {"trigger_first_bit": False})
        if response is None or response.get("status") != "ok":
            raise RuntimeError(f"Failed to disable trigger_first_bit: {response}")
        log(2, "Disabled trigger_first_bit for startup")

        response = rpc.send_rpc("command_station_start", {"loop": 0, "servicemode": True})
        if response is None or response.get("status") != "ok":
            raise RuntimeError(f"Failed to start command station: {response}")
        log(2, "Command station started in loop=0, servicemode=true")

        time.sleep(1.0)

        exit_code = read_and_report_cv(
            rpc,
            8,
            cfg,
            "CV8 Manufacturer ID",
            cfg["cv8_expected_manufacturer_id"] if cfg["cv8_expected_manufacturer_id"] >= 0 else None,
        )

        cv7_exit_code = read_and_report_cv(rpc, 7, cfg, "CV7 Version", None)
        return max(exit_code, cv7_exit_code)

    except serial.SerialException as exc:
        log(1, f"ERROR: Serial port error: {exc}")
        log(1, f"Check serial_port in SystemConfig.txt (current: {port}).")
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
            if original_trigger_first_bit is not None:
                rpc.send_rpc("command_station_params", {"trigger_first_bit": original_trigger_first_bit})
            rpc.close()
        if file_logging_started:
            System.stop_logging(close_file=True)


if __name__ == "__main__":
    sys.exit(main())