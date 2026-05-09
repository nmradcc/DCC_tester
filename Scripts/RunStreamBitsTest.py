#!/usr/bin/env python3
"""
RunStreamBitsTest Script
=========================

This runner builds ONE combined bit stream:
    preamble -> start-speed packet -> N idle packets -> stop packet -> 1 idle packet

The stream is loaded once and transmitted once.

Configuration:
  - SystemConfig.txt            global settings (serial port, motor mode, logging)
  - RunStreamBitsTestConfig.txt test-specific settings
"""

import importlib.util
import os
import serial
import sys
import time
from datetime import datetime

script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_dir)
import System


LOG_LEVEL = 1


def set_log_level(level):
    global LOG_LEVEL
    LOG_LEVEL = max(0, min(2, int(level)))


def log(level, msg):
    if LOG_LEVEL >= level:
        if LOG_LEVEL >= 2:
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{ts}] {msg}")
        else:
            print(msg)


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
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Configuration file not found: {config_path}")

    config = {}
    with open(config_path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            if "=" not in line:
                raise ValueError(f"Invalid config line (expected key=value): {raw.strip()}")
            key, value = line.split("=", 1)
            config[key.strip()] = value.strip()

    required = {
        "address",
        "preamble_bits",
        "bit1_duration",
        "bit0_duration",
        "trigger_first_bit",
        "idle_packet_count",
        "test_stop_delay_ms",
    }
    missing = sorted(required - set(config.keys()))
    if missing:
        raise ValueError(f"Missing required config keys: {', '.join(missing)}")

    idle_packet_count = _parse_int(config["idle_packet_count"], "idle_packet_count")
    if idle_packet_count < 0:
        raise ValueError("idle_packet_count must be >= 0")

    return {
        "address": _parse_int(config["address"], "address"),
        "preamble_bits": _parse_int(config["preamble_bits"], "preamble_bits"),
        "bit1_duration": _parse_int(config["bit1_duration"], "bit1_duration"),
        "bit0_duration": _parse_int(config["bit0_duration"], "bit0_duration"),
        "trigger_first_bit": _parse_bool(config["trigger_first_bit"], "trigger_first_bit"),
        "idle_packet_count": idle_packet_count,
        "test_stop_delay_ms": _parse_int(config["test_stop_delay_ms"], "test_stop_delay_ms"),
    }


def load_packet_module(file_path, module_name):
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load module from {file_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def xor_checksum(data):
    result = 0
    for b in data:
        result ^= b
    return result


def make_speed_packet(address, speed, forward=True):
    direction_bit = (1 << 7) if forward else 0
    speed_byte = direction_bit | (speed & 0x7F)
    packet = [address, 0x3F, speed_byte]
    packet.append(xor_checksum(packet))
    return packet


def make_emergency_stop_packet(address):
    packet = [address, 0x3F, (1 << 7) | 1]
    packet.append(xor_checksum(packet))
    return packet


def make_idle_packet():
    # Standard DCC idle packet body: FF 00 FF.
    return [0xFF, 0x00, 0xFF]


def packet_payload_bits(packet_bytes):
    # One packet payload after preamble: [0 + 8 data bits] per byte, then end bit 1.
    bits = []
    for byte in packet_bytes:
        bits.append(0)
        for shift in range(7, -1, -1):
            bits.append((byte >> shift) & 1)
    bits.append(1)
    return bits


def packet_bits_with_preamble(packet_bytes, preamble_bits):
    bits = [1] * preamble_bits
    bits.extend(packet_payload_bits(packet_bytes))
    return bits


def make_combined_stream_bits(start_packet, idle_packet_count, stop_packet, preamble_bits):
    bits = []
    bits.extend(packet_bits_with_preamble(start_packet, preamble_bits))
    idle_packet = make_idle_packet()
    for _ in range(idle_packet_count):
        bits.extend(packet_bits_with_preamble(idle_packet, preamble_bits))
    bits.extend(packet_bits_with_preamble(stop_packet, preamble_bits))
    bits.extend(packet_bits_with_preamble(idle_packet, preamble_bits))
    return bits


def stream_duration_ms(bits, bit1_us, bit0_us):
    total_us = sum((bit1_us if b else bit0_us) * 2 for b in bits)
    return total_us / 1000.0


def packet_duration_ms(packet_bytes, preamble_bits, bit1_us, bit0_us):
    bits = [1] * preamble_bits
    bits.extend(packet_payload_bits(packet_bytes))
    return stream_duration_ms(bits, bit1_us, bit0_us)


def run_stream_bits_test(
    rpc,
    loco_address,
    preamble_bits,
    bit1_duration,
    bit0_duration,
    trigger_first_bit,
    idle_packet_count,
    in_circuit_motor,
    test_stop_delay_ms,
):
    half_speed = 64

    start_packet = make_speed_packet(loco_address, half_speed, forward=False)
    stop_packet = make_emergency_stop_packet(loco_address)
    idle_packet = make_idle_packet()

    combined_bits = make_combined_stream_bits(
        start_packet,
        idle_packet_count,
        stop_packet,
        preamble_bits,
    )

    full_stream_ms = stream_duration_ms(combined_bits, bit1_duration, bit0_duration)
    start_window_ms = packet_duration_ms(start_packet, preamble_bits, bit1_duration, bit0_duration)
    idle_window_ms = stream_duration_ms(packet_payload_bits(idle_packet), bit1_duration, bit0_duration) * idle_packet_count

    log(1, "")
    log(1, "=" * 70)
    log(1, "DCC Stream Bits Test")
    log(1, f"  Loco address:        {loco_address}")
    log(1, f"  Preamble bits:       {preamble_bits}")
    log(1, f"  bit1_duration:       {bit1_duration} us (half-bit)")
    log(1, f"  bit0_duration:       {bit0_duration} us (half-bit)")
    log(1, f"  Trigger first bit:   {trigger_first_bit}")
    log(1, f"  Idle packet count:   {idle_packet_count}")
    log(1, f"  Stream bit count:    {len(combined_bits)}")
    log(1, f"  Feedback mode:       {'current' if in_circuit_motor else 'IO13/IO14'}")
    log(1, "=" * 70)

    log(2, f"\nStart packet: {' '.join(f'0x{b:02X}' for b in start_packet)}")
    log(2, f"Idle packet:  {' '.join(f'0x{b:02X}' for b in idle_packet)} x {idle_packet_count}")
    log(2, f"Stop packet:  {' '.join(f'0x{b:02X}' for b in stop_packet)}")
    log(2, f"Combined stream bit length: {len(combined_bits)}")
    log(2, f"Combined stream duration: {full_stream_ms:.2f} ms")

    try:
        log(1, "\nPre-step: Setting command station trigger_first_bit...")
        response = rpc.send_rpc("command_station_params", {"trigger_first_bit": trigger_first_bit})
        if response is None or response.get("status") != "ok":
            return {"status": "FAIL", "error": f"Failed to set trigger_first_bit: {response}"}
        log(1, "OK trigger_first_bit configured")

        log(1, "\nStep 1: Starting command station in custom packet mode (loop=0)...")
        response = rpc.send_rpc("command_station_start", {"loop": 0})
        if response is None or response.get("status") != "ok":
            return {"status": "FAIL", "error": f"Failed to start command station: {response}"}
        log(1, f"OK Command station started (loop={response.get('loop', 0)})")
        time.sleep(0.5)

        if in_circuit_motor:
            log(1, "\nStep 2: Reading motor-off current (baseline)...")
            response = rpc.send_rpc("get_current_feedback_ma", {"num_samples": 4, "sample_delay_ms": 25})
            if response is None or response.get("status") != "ok":
                return {"status": "FAIL", "error": "Failed to read baseline current"}
            motor_off_ma = response["current_ma"]
            log(1, f"OK Motor off: {motor_off_ma} mA")
        else:
            log(1, "\nStep 2: Reading motor-off IO13/IO14 (baseline)...")
            response = rpc.send_rpc("get_gpio_inputs", {})
            if response is None or response.get("status") != "ok":
                return {"status": "FAIL", "error": "Failed to read baseline IO"}
            gpio = response["value"]
            off_io13 = bool(gpio & (1 << 12))
            off_io14 = bool(gpio & (1 << 13))
            log(1, f"OK Motor off: IO13={'HIGH' if off_io13 else 'LOW'}, IO14={'HIGH' if off_io14 else 'LOW'}")

        log(1, f"\nStep 3: Loading combined stream ({len(combined_bits)} bits)...")
        response = rpc.send_rpc("command_station_load_bits", {
            "bits": combined_bits,
            "bit1_duration": bit1_duration,
            "bit0_duration": bit0_duration,
            "replace": True,
        })
        if response is None or response.get("status") != "ok":
            return {"status": "FAIL", "error": f"Failed to load combined stream bits: {response}"}
        log(1, f"OK Loaded {response.get('bit_count')} bits")

        log(1, "\nStep 4: Triggering combined stream (single dump)...")
        response = rpc.send_rpc("command_station_transmit_bits", {"count": 1})
        if response is None or response.get("status") != "ok":
            return {"status": "FAIL", "error": f"Failed to trigger combined stream: {response}"}
        log(1, "OK Combined stream triggered")

        # Try to sample motor-run state while idle packets are still being streamed.
        run_sample_ms = min(max(5.0, start_window_ms + (idle_window_ms * 0.5)), max(5.0, full_stream_ms - 2.0))
        log(1, f"\nStep 5: Waiting {run_sample_ms:.0f} ms then sampling motor-run state...")
        time.sleep(run_sample_ms / 1000.0)

        if in_circuit_motor:
            response = rpc.send_rpc("get_current_feedback_ma", {"num_samples": 4, "sample_delay_ms": 25})
            if response is None or response.get("status") != "ok":
                return {"status": "FAIL", "error": "Failed to read motor-run current"}
            motor_on_ma = response["current_ma"]
            log(1, f"OK Motor run: {motor_on_ma} mA")
        else:
            response = rpc.send_rpc("get_gpio_inputs", {})
            if response is None or response.get("status") != "ok":
                return {"status": "FAIL", "error": "Failed to read motor-run IO"}
            gpio = response["value"]
            run_io13 = bool(gpio & (1 << 12))
            run_io14 = bool(gpio & (1 << 13))
            log(1, f"OK Motor run: IO13={'HIGH' if run_io13 else 'LOW'}, IO14={'HIGH' if run_io14 else 'LOW'}")

        remaining_ms = max(0.0, full_stream_ms - run_sample_ms)
        total_wait_stop_ms = remaining_ms + test_stop_delay_ms
        log(1, f"\nStep 6: Waiting {total_wait_stop_ms:.0f} ms for stream completion and stop settle...")
        time.sleep(total_wait_stop_ms / 1000.0)

        if in_circuit_motor:
            response = rpc.send_rpc("get_current_feedback_ma", {"num_samples": 4, "sample_delay_ms": 25})
            if response is None or response.get("status") != "ok":
                return {"status": "FAIL", "error": "Failed to read stopped current"}
            motor_stopped_ma = response["current_ma"]
            log(1, f"OK Motor stopped: {motor_stopped_ma} mA")
        else:
            response = rpc.send_rpc("get_gpio_inputs", {})
            if response is None or response.get("status") != "ok":
                return {"status": "FAIL", "error": "Failed to read stopped IO"}
            gpio = response["value"]
            stop_io13 = bool(gpio & (1 << 12))
            stop_io14 = bool(gpio & (1 << 13))
            log(1, f"OK Motor stopped: IO13={'HIGH' if stop_io13 else 'LOW'}, IO14={'HIGH' if stop_io14 else 'LOW'}")

        log(1, "\nStep 7: Stopping command station...")
        rpc.send_rpc("command_station_stop", {})
        log(1, "OK Command station stopped")

        if in_circuit_motor:
            min_delta_ma = 1
            test_pass = (
                (motor_on_ma - motor_off_ma) >= min_delta_ma and
                (motor_on_ma - motor_stopped_ma) >= min_delta_ma
            )
            return {
                "status": "PASS" if test_pass else "FAIL",
                "motor_off_ma": motor_off_ma,
                "motor_on_ma": motor_on_ma,
                "motor_stopped_ma": motor_stopped_ma,
            }

        motor_started = (not run_io13) or (not run_io14)
        motor_stopped = stop_io13 and stop_io14
        test_pass = motor_started and motor_stopped
        return {
            "status": "PASS" if test_pass else "FAIL",
            "motor_off_io": (off_io13, off_io14),
            "motor_run_io": (run_io13, run_io14),
            "motor_stop_io": (stop_io13, stop_io14),
        }

    except Exception as exc:
        log(1, f"EXCEPTION: {exc}")
        try:
            rpc.send_rpc("command_station_stop", {})
        except Exception:
            pass
        return {"status": "FAIL", "error": str(exc)}


def main():
    print("=" * 70)
    print("DCC Stream Bits Test Runner")
    print("=" * 70)
    print()

    config_path = os.path.join(script_dir, "RunStreamBitsTestConfig.txt")

    try:
        cfg = load_config(config_path)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}")
        return 1

    sys_cfg = System.get_config()

    address = cfg["address"]
    preamble_bits = cfg["preamble_bits"]
    bit1_duration = cfg["bit1_duration"]
    bit0_duration = cfg["bit0_duration"]
    trigger_first_bit = cfg["trigger_first_bit"]
    idle_packet_count = cfg["idle_packet_count"]
    test_stop_delay_ms = cfg["test_stop_delay_ms"]

    port = sys_cfg.serial_port
    in_circuit_motor = sys_cfg.in_circuit_motor
    logging_level = sys_cfg.logging_level

    set_log_level(logging_level)

    log(1, "Configuration Summary:")
    log(1, "=" * 70)
    log(1, f"  Serial port:           {port}")
    log(1, f"  In-circuit motor:      {in_circuit_motor}")
    log(1, f"  Logging level:         {logging_level}")
    log(1, f"  Loco address:          {address}")
    log(1, f"  Preamble bits:         {preamble_bits}")
    log(1, f"  bit1_duration:         {bit1_duration} us")
    log(1, f"  bit0_duration:         {bit0_duration} us")
    log(1, f"  Trigger first bit:     {trigger_first_bit}")
    log(1, f"  Idle packet count:     {idle_packet_count}")
    log(1, f"  Test stop delay:       {test_stop_delay_ms} ms")
    log(1, "=" * 70)
    log(1, "")

    pkt_module_path = os.path.join(script_dir, "PacketData", "PacketAcceptanceTest.py")
    try:
        pkt_module = load_packet_module(pkt_module_path, "packet_acceptance_test")
        dcc_tester_rpc = pkt_module.DCCTesterRPC
    except Exception as exc:
        print(f"ERROR: Failed to import DCCTesterRPC: {exc}")
        return 1

    try:
        rpc = dcc_tester_rpc(port)
    except serial.SerialException as exc:
        print(f"ERROR: Could not open serial port '{port}': {exc}")
        return 1

    try:
        result = run_stream_bits_test(
            rpc,
            address,
            preamble_bits,
            bit1_duration,
            bit0_duration,
            trigger_first_bit,
            idle_packet_count,
            in_circuit_motor,
            test_stop_delay_ms,
        )
    finally:
        rpc.close()

    log(1, "")
    log(1, "=" * 70)
    status = result.get("status", "FAIL")
    if status == "PASS":
        log(1, "OK TEST PASS")
    else:
        log(1, f"FAIL TEST FAIL  ({result.get('error', '')})")
    log(1, "=" * 70)

    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
