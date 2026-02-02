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

# Add PacketData directory to path
script_dir = os.path.dirname(os.path.abspath(__file__))
packet_data_dir = os.path.join(script_dir, "PacketData", "Motor Current Feedback")
sys.path.insert(0, packet_data_dir)

from PacketAcceptanceTest import DCCTesterRPC, run_packet_acceptance_test


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
    print("If any iteration fails, the test will abort immediately.")
    print()
    
    # Get test parameters
    print("-" * 70)
    print("Test Parameters:")
    print("-" * 70)
    
    delay_ms = get_int_input("Inter-packet delay in milliseconds", default=1000)
    pass_count = get_int_input("Number of test passes", default=10)
    logging_level = get_int_input("Logging level (0=none, 1=minimum, 2=verbose)", default=1)
    
    print()
    print("-" * 70)
    print("Connection Parameters:")
    print("-" * 70)
    
    port = input("Enter serial port [default: COM6]: ").strip()
    if not port:
        port = "COM6"
    
    address = get_int_input("Enter locomotive address", default=3)
    
    print()
    print("=" * 70)
    print("Configuration Summary:")
    print("=" * 70)
    print(f"  Inter-packet delay: {delay_ms} ms")
    print(f"  Number of passes:   {pass_count}")
    print(f"  Serial port:        {port}")
    print(f"  Locomotive address: {address}")
    print(f"  Logging level:      {logging_level}")
    print("=" * 70)
    print()
    
    # Confirm before running
    confirm = input("Run test with these parameters? [Y/n]: ").strip().lower()
    if confirm and confirm not in ['y', 'yes']:
        print("Test cancelled by user.")
        return 0
    
    print()
    print("=" * 70)
    print("Starting Test Run")
    print("=" * 70)
    print()
    
    try:
        # Connect to DCC_tester
        print(f"Connecting to {port}...")
        rpc = DCCTesterRPC(port)
        print("✓ Connected!\n")
        
        # Run test iterations
        passed_count = 0
        failed_count = 0
        
        for i in range(1, pass_count + 1):
            print()
            print("=" * 70)
            print(f"Test Pass {i} of {pass_count}")
            print("=" * 70)
            print()
            
            # Run the test
            result = run_packet_acceptance_test(rpc, address, delay_ms, logging_level=logging_level)
            
            if result.get("status") == "PASS":
                passed_count += 1
                print(f"✓ Pass {i}/{pass_count} completed successfully")
            else:
                failed_count += 1
                print()
                print(f"✗ Pass {i}/{pass_count} FAILED")
                print(f"Error: {result.get('error', 'Unknown error')}")
                print()
                print("=" * 70)
                print("TEST ABORTED DUE TO FAILURE")
                print("=" * 70)
                print(f"\nResults Summary:")
                print(f"  Total passes run: {i}")
                print(f"  Passed: {passed_count}")
                print(f"  Failed: {failed_count}")
                print()
                rpc.close()
                return 1
        
        # All tests passed
        print()
        print("=" * 70)
        print("ALL TESTS COMPLETED SUCCESSFULLY")
        print("=" * 70)
        print(f"\nResults Summary:")
        print(f"  Total passes: {pass_count}")
        print(f"  Passed: {passed_count}")
        print(f"  Failed: {failed_count}")
        print(f"  Success rate: 100%")
        print()
        print(f"✓ All {pass_count} test passes completed with {delay_ms}ms inter-packet delay")
        print()
        
        # Close connection
        rpc.close()
        return 0
        
    except serial.SerialException as e:
        print(f"\nERROR: Serial port error: {e}")
        print(f"Make sure {port} is the correct port and the device is connected.")
        return 1
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")
        print()
        print("=" * 70)
        print(f"Results Summary (Partial):")
        print("=" * 70)
        print(f"  Completed passes: {passed_count + failed_count}")
        print(f"  Passed: {passed_count}")
        print(f"  Failed: {failed_count}")
        print()
        return 1
    except Exception as e:
        print(f"\nERROR: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
