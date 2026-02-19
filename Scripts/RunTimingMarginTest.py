#!/usr/bin/env python3
"""
RunTimingMarginTest Script
==========================

This script sets default command station parameters, then runs the
PacketAcceptanceTest based on in_circuit_motor.
Configuration is read from RunTimingMarginTestConfig.txt.
"""

import importlib.util
import os
import sys
import time
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
        "min_bit1_duration",
        "max_bit1_duration",
        "min_bit0_duration",
        "max_bit0_duration",
    }

    missing = sorted(required_keys - set(config.keys()))
    if missing:
        raise ValueError(f"Missing required config keys: {', '.join(missing)}")

    wait_value = config.get("wait_for_button", config.get("wait_on_button"))
    if wait_value is None:
        raise ValueError("Missing boolean value for 'wait_for_button'")

    return {
        "address": _parse_int(config.get("address"), "address"),
        "inter_packet_delay_ms": _parse_int(config.get("inter_packet_delay_ms"), "inter_packet_delay_ms"),
        "pass_count": _parse_int(config.get("pass_count"), "pass_count"),
        "logging_level": _parse_int(config.get("logging_level"), "logging_level"),
        "stop_on_failure": _parse_bool(config.get("stop_on_failure"), "stop_on_failure"),
        "serial_port": config.get("serial_port"),
        "in_circuit_motor": _parse_bool(config.get("in_circuit_motor"), "in_circuit_motor"),
        "wait_for_button": _parse_bool(wait_value, "wait_for_button"),
        "preamble_bits": _parse_int(config.get("preamble_bits"), "preamble_bits"),
        "bit1_duration": _parse_int(config.get("bit1_duration"), "bit1_duration"),
        "bit0_duration": _parse_int(config.get("bit0_duration"), "bit0_duration"),
        "trigger_first_bit": _parse_bool(config.get("trigger_first_bit"), "trigger_first_bit"),
        "min_bit1_duration": _parse_int(config.get("min_bit1_duration"), "min_bit1_duration"),
        "max_bit1_duration": _parse_int(config.get("max_bit1_duration"), "max_bit1_duration"),
        "min_bit0_duration": _parse_int(config.get("min_bit0_duration"), "min_bit0_duration"),
        "max_bit0_duration": _parse_int(config.get("max_bit0_duration"), "max_bit0_duration"),
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


def wait_for_button_press(rpc, log):
    log(1, "Press the user button to continue.")
    log(1, "Waiting for button press...")
    was_pressed = False

    while True:
        response = rpc.send_rpc("get_gpio_input", {"pin": 16})
        if response is None or response.get("status") != "ok":
            log(1, f"WARNING: Failed to read button state: {response}")
            time.sleep(0.1)
            continue

        pressed = response.get("value", 0) == 1
        if pressed and not was_pressed:
            log(1, "OK Button pressed")
            break

        was_pressed = pressed
        time.sleep(0.05)


def main():
    print("=" * 70)
    print("DCC Timing Margin Test")
    print("=" * 70)
    print()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "RunTimingMarginTestConfig.txt")

    try:
        config = load_config(config_path)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}")
        print("Please update RunTimingMarginTestConfig.txt with valid values.")
        return 1

    address = config["address"]
    delay_ms = config["inter_packet_delay_ms"]
    pass_count = config["pass_count"]
    logging_level = config["logging_level"]
    stop_on_failure = config["stop_on_failure"]
    port = config["serial_port"]
    in_circuit_motor = config["in_circuit_motor"]
    wait_for_button = config["wait_for_button"]
    min_bit1_duration = config["min_bit1_duration"]
    max_bit1_duration = config["max_bit1_duration"]
    min_bit0_duration = config["min_bit0_duration"]
    max_bit0_duration = config["max_bit0_duration"]

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
    log(1, f"  Wait for button:        {wait_for_button}")
    log(1, f"  Preamble bits:          {config['preamble_bits']}")
    log(1, f"  Bit1 duration:          {config['bit1_duration']} us")
    log(1, f"  Bit0 duration:          {config['bit0_duration']} us")
    log(1, f"  Trigger first bit:      {config['trigger_first_bit']}")
    log(1, f"  Min bit1 duration:      {min_bit1_duration} us")
    log(1, f"  Max bit1 duration:      {max_bit1_duration} us")
    log(1, f"  Min bit0 duration:      {min_bit0_duration} us")
    log(1, f"  Max bit0 duration:      {max_bit0_duration} us")
    log(1, "=" * 70)
    log(1, "")

    try:
        log(2, f"Connecting to {port}...")
        rpc = DCCTesterRPC(port)
        log(2, "OK Connected!\n")

        log(1, "Step 1: Setting default command station parameters")
        params = {
            "preamble_bits": config["preamble_bits"],
            "bit1_duration": config["bit1_duration"],
            "bit0_duration": config["bit0_duration"],
            "trigger_first_bit": config["trigger_first_bit"],
        }

        response = rpc.send_rpc("command_station_params", params)
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set parameters: {response}")
            rpc.close()
            return 1

        log(1, "OK Default command station parameters updated")

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
            rpc.close()
            return 1

        log(1, "Step 3: Setting minimum bit1 duration")

        if wait_for_button:
            wait_for_button_press(rpc, log)

        response = rpc.send_rpc("command_station_params", {"bit1_duration": min_bit1_duration})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set bit1 duration: {response}")
            rpc.close()
            return 1

        log(1, f"OK Bit1 duration set to {min_bit1_duration} us")

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

        ok = False
        try:
            ok = run_acceptance_series(
                rpc,
                "Step 4: Running Packet Acceptance Test",
                address,
                delay_ms,
                pass_count,
                logging_level,
                stop_on_failure,
                log,
                run_packet_acceptance_test
            )
        finally:
            response = rpc.send_rpc("command_station_params", {"bit1_duration": config["bit1_duration"]})
            if response is None or response.get("status") != "ok":
                log(1, f"ERROR: Failed to restore bit1 duration: {response}")
            else:
                log(1, f"OK Bit1 duration restored to {config['bit1_duration']} us")

        if not ok:
            rpc.close()
            return 1

        log(1, "Step 5: Setting maximum bit1 duration")
        if wait_for_button:
            wait_for_button_press(rpc, log)

        response = rpc.send_rpc("command_station_params", {"bit1_duration": max_bit1_duration})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set bit1 duration: {response}")
            rpc.close()
            return 1

        log(1, f"OK Bit1 duration set to {max_bit1_duration} us")

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

        ok = False
        try:
            ok = run_acceptance_series(
                rpc,
                "Step 6: Running Packet Acceptance Test",
                address,
                delay_ms,
                pass_count,
                logging_level,
                stop_on_failure,
                log,
                run_packet_acceptance_test
            )
        finally:
            response = rpc.send_rpc("command_station_params", {"bit1_duration": config["bit1_duration"]})
            if response is None or response.get("status") != "ok":
                log(1, f"ERROR: Failed to restore bit1 duration: {response}")
            else:
                log(1, f"OK Bit1 duration restored to {config['bit1_duration']} us")

        if not ok:
            rpc.close()
            return 1

        log(1, "Step 7: Setting minimum bit0 duration")
        if wait_for_button:
            wait_for_button_press(rpc, log)

        response = rpc.send_rpc("command_station_params", {"bit0_duration": min_bit0_duration})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set bit0 duration: {response}")
            rpc.close()
            return 1

        log(1, f"OK Bit0 duration set to {min_bit0_duration} us")

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

        ok = False
        try:
            ok = run_acceptance_series(
                rpc,
                "Step 8: Running Packet Acceptance Test",
                address,
                delay_ms,
                pass_count,
                logging_level,
                stop_on_failure,
                log,
                run_packet_acceptance_test
            )
        finally:
            response = rpc.send_rpc("command_station_params", {"bit0_duration": config["bit0_duration"]})
            if response is None or response.get("status") != "ok":
                log(1, f"ERROR: Failed to restore bit0 duration: {response}")
            else:
                log(1, f"OK Bit0 duration restored to {config['bit0_duration']} us")

        if not ok:
            rpc.close()
            return 1

        log(1, "Step 9: Setting maximum bit0 duration")
        if wait_for_button:
            wait_for_button_press(rpc, log)

        response = rpc.send_rpc("command_station_params", {"bit0_duration": max_bit0_duration})
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set bit0 duration: {response}")
            rpc.close()
            return 1

        log(1, f"OK Bit0 duration set to {max_bit0_duration} us")

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

        ok = False
        try:
            ok = run_acceptance_series(
                rpc,
                "Step 10: Running Packet Acceptance Test",
                address,
                delay_ms,
                pass_count,
                logging_level,
                stop_on_failure,
                log,
                run_packet_acceptance_test
            )
        finally:
            response = rpc.send_rpc("command_station_params", {"bit0_duration": config["bit0_duration"]})
            if response is None or response.get("status") != "ok":
                log(1, f"ERROR: Failed to restore bit0 duration: {response}")
            else:
                log(1, f"OK Bit0 duration restored to {config['bit0_duration']} us")

        if not ok:
            rpc.close()
            return 1

        log(1, "Step 11: Restoring default command station parameters")
        if wait_for_button:
            wait_for_button_press(rpc, log)

        params = {
            "preamble_bits": config["preamble_bits"],
            "bit1_duration": config["bit1_duration"],
            "bit0_duration": config["bit0_duration"],
            "trigger_first_bit": config["trigger_first_bit"],
        }

        response = rpc.send_rpc("command_station_params", params)
        if response is None or response.get("status") != "ok":
            log(1, f"ERROR: Failed to set parameters: {response}")
            rpc.close()
            return 1

        log(1, "OK Default command station parameters restored")

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

        if wait_for_button:
            wait_for_button_press(rpc, log)

        if not run_acceptance_series(
            rpc,
            "Step 12: Running Packet Acceptance Test",
            address,
            delay_ms,
            pass_count,
            logging_level,
            stop_on_failure,
            log,
            run_packet_acceptance_test
        ):
            rpc.close()
            return 1

        rpc.close()
        return 0

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
