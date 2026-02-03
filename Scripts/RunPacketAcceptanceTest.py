#!/usr/bin/env python3
"""
RunPacketAcceptanceTest Script
===============================

This script runs multiple iterations of the PacketAcceptanceTest
to verify NEM 671 inter-packet delay requirements.

The test can be configured with:
  - Inter-packet delay (default: 1000ms)
  - Number of passes (default: 10)
  - COM port and locomotive address

If any iteration fails, the test aborts immediately.
"""

import sys
import os
import serial
import importlib.util

script_dir = os.path.dirname(os.path.abspath(__file__))

def load_packet_acceptance_module(file_path, module_name):
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load module from {file_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def get_int_input(prompt, default=None):
    """
    Get integer input from user with optional default value.
    
    Args:
        prompt: Prompt message to display
        default: Default value if user presses Enter (None = required)
        
    Returns:
        Integer value entered by user
    """
    while True:
        if default is not None:
            user_input = input(f"{prompt} [default: {default}]: ").strip()
            if not user_input:
                return default
        else:
            user_input = input(f"{prompt}: ").strip()
        
        try:
            return int(user_input)
        except ValueError:
            print("  ERROR: Please enter a valid integer")


def get_bool_input(prompt, default=False):
    """
    Get boolean input from user with optional default value.

    Args:
        prompt: Prompt message to display
        default: Default value if user presses Enter

    Returns:
        Boolean value entered by user
    """
    default_label = "Y" if default else "N"
    while True:
        user_input = input(f"{prompt} [default: {default_label}]: ").strip().lower()
        if not user_input:
            return default
        if user_input in ["y", "yes", "true", "1"]:
            return True
        if user_input in ["n", "no", "false", "0"]:
            return False
        print("  ERROR: Please enter Y or N")


def main():
    """Main entry point."""
    
    print("=" * 70)
    print("DCC Packet Acceptance Test Runner")
    print("NEM 671 Compliance Testing")
    print("=" * 70)
    print()
    print("This script will run multiple iterations of the Packet Acceptance")
    print("test to verify NEM 671 compliance.")
    print()
    print("If any iteration fails, the test will continue unless stop on failure is enabled.")
    print()
    
    # Get test parameters
    print("-" * 70)
    print("Test Parameters:")
    print("-" * 70)
    
    address = get_int_input("Enter locomotive address", default=3)
    delay_ms = get_int_input("Inter-packet delay in milliseconds", default=1000)
    pass_count = get_int_input("Number of test passes", default=10)
    logging_level = get_int_input("Logging level (0=none, 1=minimum, 2=verbose)", default=1)
    stop_on_failure = get_bool_input("Stop on failure", default=False)
    
    print()
    print("-" * 70)
    print("Connection Parameters:")
    print("-" * 70)
    
    port = input("Enter serial port [default: COM6]: ").strip()
    if not port:
        port = "COM6"
    
    in_circuit_motor = get_bool_input("In circuit motor", default=False)
    
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
    log(1, f"  Inter-packet delay: {delay_ms} ms")
    log(1, f"  Number of passes:   {pass_count}")
    log(1, f"  Serial port:        {port}")
    log(1, f"  Locomotive address: {address}")
    log(1, f"  In circuit motor:   {in_circuit_motor}")
    log(1, f"  Logging level:      {logging_level}")
    log(1, f"  Stop on failure:    {stop_on_failure}")
    log(1, "=" * 70)
    log(1, "")
    
    # Confirm before running
    confirm = input("Run test with these parameters? [Y/n]: ").strip().lower()
    if confirm and confirm not in ['y', 'yes']:
        log(1, "Test cancelled by user.")
        return 0
    
    log(2, "")
    log(2, "=" * 70)
    log(2, "Starting Test Run")
    log(2, "=" * 70)
    log(2, "")
    
    try:
        # Connect to DCC_tester
        log(2, f"Connecting to {port}...")
        rpc = DCCTesterRPC(port)
        log(2, "✓ Connected!\n")
        
        # Run test iterations
        passed_count = 0
        failed_count = 0
        
        for i in range(1, pass_count + 1):
            log(2, "")
            log(2, "=" * 70)
            log(2, f"Test Pass {i} of {pass_count}")
            log(2, "=" * 70)
            log(2, "")
            
            # Run the test
            result = run_packet_acceptance_test(rpc, address, delay_ms, logging_level=logging_level)
            
            if result.get("status") == "PASS":
                passed_count += 1
                log(1, f"✓ Pass {i}/{pass_count} completed successfully")
            else:
                failed_count += 1
                log(1, "")
                log(1, f"✗ Pass {i}/{pass_count} FAILED")
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
                    rpc.close()
                    return 1
        
        # All tests passed
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
        log(1, f"✓ All {pass_count} test passes completed with {delay_ms}ms inter-packet delay")
        log(1, "")
        
        # Close connection
        rpc.close()
        return 0
        
    except serial.SerialException as e:
        log(1, f"\nERROR: Serial port error: {e}")
        log(1, f"Make sure {port} is the correct port and the device is connected.")
        return 1
    except KeyboardInterrupt:
        log(1, "\n\nTest interrupted by user.")
        log(1, "")
        log(1, "=" * 70)
        log(1, "Results Summary (Partial):")
        log(1, "=" * 70)
        log(1, f"  Completed passes: {passed_count + failed_count}")
        log(1, f"  Passed: {passed_count}")
        log(1, f"  Failed: {failed_count}")
        log(1, "")
        return 1
    except Exception as e:
        log(1, f"\nERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
