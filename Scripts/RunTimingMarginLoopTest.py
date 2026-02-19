#!/usr/bin/env python3
"""
RunTimingMarginLoopTest Script
==============================

This script sets default command station parameters, then runs the
PacketAcceptanceTest based on in_circuit_motor.
Configuration is read from RunTimingMarginLoopTestConfig.txt.
"""

import importlib.util
import os
import sys
import serial


def load_packet_acceptance_module(file_path, module_name):
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load module from {file_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


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
        "address",
        "inter_packet_delay_ms",
        "pass_count",
        "logging_level",
        "stop_on_failure",
        "serial_port",
        "in_circuit_motor",
        "preamble_bits",
        "bit1_duration",
        "bit0_duration",
        "trigger_first_bit",
    }

    missing = sorted(required_keys - set(config.keys()))
    if missing:
        raise ValueError(f"Missing required config keys: {', '.join(missing)}")

    return {
        "address": _parse_int(config.get("address"), "address"),
        "inter_packet_delay_ms": _parse_int(config.get("inter_packet_delay_ms"), "inter_packet_delay_ms"),
        "pass_count": _parse_int(config.get("pass_count"), "pass_count"),
        "logging_level": _parse_int(config.get("logging_level"), "logging_level"),
        "stop_on_failure": _parse_bool(config.get("stop_on_failure"), "stop_on_failure"),
        "serial_port": config.get("serial_port"),
        "in_circuit_motor": _parse_bool(config.get("in_circuit_motor"), "in_circuit_motor"),
        "preamble_bits": _parse_int(config.get("preamble_bits"), "preamble_bits"),
        "bit1_duration": _parse_int(config.get("bit1_duration"), "bit1_duration"),
        "bit0_duration": _parse_int(config.get("bit0_duration"), "bit0_duration"),
        "trigger_first_bit": _parse_bool(config.get("trigger_first_bit"), "trigger_first_bit"),
    }


def run_acceptance_series(
    rpc,
    run_label,
    address,
    delay_ms,
    pass_count,
    logging_level,
    stop_on_failure,
    log,
    run_packet_acceptance_test,
):
    log(1, run_label)

    passed_count = 0
    failed_count = 0

    for i in range(1, pass_count + 1):
        log(2, "")
        log(2, "=" * 70)
        log(2, f"Test Pass {i} of {pass_count}")
        log(2, "=" * 70)
        log(2, "")

        result = run_packet_acceptance_test(
            rpc,
            address,
            delay_ms,
            logging_level=logging_level
        )

        if result.get("status") == "PASS":
            passed_count += 1
            log(1, f"OK Pass {i}/{pass_count} completed successfully")
        else:
            failed_count += 1
            log(1, "")
            log(1, f"FAIL Pass {i}/{pass_count} failed")
            log(1, f"Error: {result.get('error', 'Unknown error')}")
            if stop_on_failure:
                log(1, "")
                log(1, "=" * 70)
                log(1, "TEST ABORTED DUE TO FAILURE")
                log(1, "=" * 70)
                log(1, "\nResults Summary:")
                log(1, f"  Total passes run: {i}")
                log(1, f"  Passed: {passed_count}")
                log(1, f"  Failed: {failed_count}")
                log(1, "")
                return False

    log(1, "")
    log(1, "=" * 70)
    log(1, "ALL TESTS COMPLETED SUCCESSFULLY")
    log(1, "=" * 70)
    log(1, "\nResults Summary:")
    log(1, f"  Total passes: {pass_count}")
    log(1, f"  Passed: {passed_count}")
    log(1, f"  Failed: {failed_count}")
    log(1, "  Success rate: 100%")
    log(1, "")
    log(1, f"OK All {pass_count} test passes completed with {delay_ms}ms inter-packet delay")
    log(1, "")

    return True


def main():
    print("=" * 70)
    print("DCC Timing Margin Loop Test")
    print("=" * 70)
    print()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "RunTimingMarginLoopTestConfig.txt")

    try:
        config = load_config(config_path)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}")
        print("Please update RunTimingMarginLoopTestConfig.txt with valid values.")
        return 1

    address = config["address"]
    delay_ms = config["inter_packet_delay_ms"]
    pass_count = config["pass_count"]
    logging_level = config["logging_level"]
    stop_on_failure = config["stop_on_failure"]
    port = config["serial_port"]
    in_circuit_motor = config["in_circuit_motor"]

    packet_data_dir = os.path.join(
        script_dir,
        "PacketData",
        "Motor Current Feedback" if in_circuit_motor else "NoMotor Voltage Feedback"
    )
    packet_module_path = os.path.join(packet_data_dir, "PacketAcceptanceTest.py")

    packet_module = load_packet_acceptance_module(
        packet_module_path,
        "packet_acceptance_motor" if in_circuit_motor else "packet_acceptance_no_motor"
    )

    DCCTesterRPC = packet_module.DCCTesterRPC
    run_packet_acceptance_test = packet_module.run_packet_acceptance_test
    log = packet_module.log
    set_log_level = packet_module.set_log_level

    set_log_level(logging_level)

    log(1, "")
    log(1, "=" * 70)
    log(1, "Configuration Summary:")
    log(1, "=" * 70)
    log(1, "System Parameters:")
    log(1, f"  Serial port:            {port}")
    log(1, f"  In circuit motor:       {in_circuit_motor}")
    log(1, f"  Logging level:          {logging_level}")
    log(1, "")
    log(1, "Test Parameters:")
    log(1, f"  Locomotive address:     {address}")
    log(1, f"  Inter-packet delay:     {delay_ms} ms")
    log(1, f"  Number of passes:       {pass_count}")
    log(1, f"  Stop on failure:        {stop_on_failure}")
    log(1, f"  Preamble bits:          {config['preamble_bits']}")
    log(1, f"  Bit1 duration:          {config['bit1_duration']} us")
    log(1, f"  Bit0 duration:          {config['bit0_duration']} us")
    log(1, f"  Trigger first bit:      {config['trigger_first_bit']}")
    log(1, "=" * 70)
    log(1, "")

    try:
        log(2, f"Connecting to {port}...")
        rpc = DCCTesterRPC(port)
        log(2, "OK Connected!\n")

        response = rpc.send_rpc("command_station_get_params", {})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to read parameters: {response}")
            rpc.close()
            return 1

        params_out = response.get("parameters", {})
        default_params = {
            "preamble_bits": params_out.get("preamble_bits"),
            "bit1_duration": params_out.get("bit1_duration"),
            "bit0_duration": params_out.get("bit0_duration"),
            "trigger_first_bit": params_out.get("trigger_first_bit"),
        }

        exit_code = 0
        timing_params = {
            "preamble_bits": config["preamble_bits"],
            "bit1_duration": config["bit1_duration"],
            "bit0_duration": config["bit0_duration"],
            "trigger_first_bit": config["trigger_first_bit"],
        }
        try:
            while True:
                log(1, "Timing parameters for this pass:")
                log(1, f"  Preamble bits:     {timing_params['preamble_bits']}")
                log(1, f"  Bit1 duration:     {timing_params['bit1_duration']} us")
                log(1, f"  Bit0 duration:     {timing_params['bit0_duration']} us")
                log(1, f"  Trigger first bit: {timing_params['trigger_first_bit']}")
                log(1, "Step 1: Setting command station parameters")
                params = {
                    "preamble_bits": timing_params["preamble_bits"],
                    "bit1_duration": timing_params["bit1_duration"],
                    "bit0_duration": timing_params["bit0_duration"],
                    "trigger_first_bit": timing_params["trigger_first_bit"],
                }

                response = rpc.send_rpc("command_station_params", params)
                if response is None or response.get("status") != "ok":
                    log(1, f"ERROR: Failed to set parameters: {response}")
                    exit_code = 1
                    break

                log(1, "OK Command station parameters updated")

                response = rpc.send_rpc("command_station_get_params", {})
                if response is not None and response.get("status") == "ok":
                    params_out = response.get("parameters", {})
                    log(1, "")
                    log(1, "Current Parameters:")
                    log(1, f"  Preamble bits:      {params_out.get('preamble_bits')}")
                    log(1, f"  Bit1 duration:      {params_out.get('bit1_duration')} us")
                    log(1, f"  Bit0 duration:      {params_out.get('bit0_duration')} us")
                    log(1, f"  Trigger first bit:  {params_out.get('trigger_first_bit')}")
                    log(1, "")

                if not run_acceptance_series(
                    rpc,
                    "Step 2: Running Packet Acceptance Test",
                    address,
                    delay_ms,
                    pass_count,
                    logging_level,
                    stop_on_failure,
                    log,
                    run_packet_acceptance_test
                ):
                    exit_code = 1
                    break

                choice = input("Exit loop? [y/N]: ").strip().lower()
                if choice in {"y", "yes"}:
                    break

                try:
                    updated_config = load_config(config_path)
                except (FileNotFoundError, ValueError) as exc:
                    log(1, f"ERROR: {exc}")
                    exit_code = 1
                    break

                if updated_config["serial_port"] != port:
                    log(1, f"WARNING: serial_port change ignored (still {port})")

                address = updated_config["address"]
                delay_ms = updated_config["inter_packet_delay_ms"]
                pass_count = updated_config["pass_count"]
                logging_level = updated_config["logging_level"]
                stop_on_failure = updated_config["stop_on_failure"]

                if updated_config["in_circuit_motor"] != in_circuit_motor:
                    in_circuit_motor = updated_config["in_circuit_motor"]
                    packet_data_dir = os.path.join(
                        script_dir,
                        "PacketData",
                        "Motor Current Feedback" if in_circuit_motor else "NoMotor Voltage Feedback"
                    )
                    packet_module_path = os.path.join(packet_data_dir, "PacketAcceptanceTest.py")
                    packet_module = load_packet_acceptance_module(
                        packet_module_path,
                        "packet_acceptance_motor" if in_circuit_motor else "packet_acceptance_no_motor"
                    )
                    run_packet_acceptance_test = packet_module.run_packet_acceptance_test
                    log = packet_module.log
                    set_log_level = packet_module.set_log_level

                set_log_level(logging_level)
                timing_params = {
                    "preamble_bits": updated_config["preamble_bits"],
                    "bit1_duration": updated_config["bit1_duration"],
                    "bit0_duration": updated_config["bit0_duration"],
                    "trigger_first_bit": updated_config["trigger_first_bit"],
                }
        finally:
            response = rpc.send_rpc("command_station_params", default_params)
            if response is None or response.get("status") != "ok":
                log(1, f"ERROR: Failed to restore parameters: {response}")
            else:
                log(1, "OK Command station parameters restored")

            response = rpc.send_rpc("command_station_get_params", {})
            if response is not None and response.get("status") == "ok":
                params_out = response.get("parameters", {})
                log(1, "")
                log(1, "Restored Parameters:")
                log(1, f"  Preamble bits:      {params_out.get('preamble_bits')}")
                log(1, f"  Bit1 duration:      {params_out.get('bit1_duration')} us")
                log(1, f"  Bit0 duration:      {params_out.get('bit0_duration')} us")
                log(1, f"  Trigger first bit:  {params_out.get('trigger_first_bit')}")
                log(1, "")

        if exit_code == 0:
            log(1, "Loop completed successfully")
        else:
            log(1, "Loop exited with errors")

        rpc.close()
        return exit_code

    except serial.SerialException as exc:
        log(1, f"ERROR: Serial port error: {exc}")
        log(1, f"Make sure {port} is the correct port and the device is connected.")
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
