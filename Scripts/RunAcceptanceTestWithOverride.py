#!/usr/bin/env python3
"""
Interactive Wrapper for Acceptance Test with Packet Override
=============================================================

This script prompts the user for override parameters and then runs
AcceptanceTestWithNBit0Change.py with those parameters.
"""

import subprocess
import sys
import os


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


def get_hex_input(prompt, default=None):
    """
    Get hexadecimal input from user with optional default value.
    Accepts input with or without '0x' prefix.
    
    Args:
        prompt: Prompt message to display
        default: Default value if user presses Enter (None = required)
        
    Returns:
        Integer value parsed from hex input
    """
    while True:
        if default is not None:
            user_input = input(f"{prompt} [default: 0x{default:X}]: ").strip()
            if not user_input:
                return default
        else:
            user_input = input(f"{prompt}: ").strip()
        
        try:
            # Accept input with or without 0x prefix
            if user_input.lower().startswith('0x'):
                return int(user_input, 16)
            else:
                return int(user_input, 16)
        except ValueError:
            print("  ERROR: Please enter a valid hexadecimal value (e.g., 0x04 or 04)")


def main():
    """Main entry point."""
    
    print("=" * 70)
    print("DCC Acceptance Test - Interactive Override Configuration")
    print("=" * 70)
    print()
    print("This script will run acceptance tests with custom packet override")
    print("parameters. You will be prompted to enter:")
    print("  - Bit mask (which zero bits to modify)")
    print("  - P-phase delta (microseconds)")
    print("  - N-phase delta (microseconds)")
    print()
    print("Common mask values:")
    print("  0x01 = Bit 0 only")
    print("  0x02 = Bit 1 only")
    print("  0x04 = Bit 2 only")
    print("  0x07 = Bits 0, 1, and 2")
    print("  0x08 = Bit 3 only")
    print()
    
    # Get override parameters
    print("-" * 70)
    print("Override Parameters:")
    print("-" * 70)
    
    mask = get_hex_input("Enter zero bit override mask (hex, e.g., 0x04 or 04)")
    deltaP = get_int_input("Enter P-phase delta in microseconds (e.g., 20 or -10)")
    deltaN = get_int_input("Enter N-phase delta in microseconds (e.g., -20 or 10)")
    
    print()
    print("-" * 70)
    print("Optional Parameters:")
    print("-" * 70)
    
    port = input("Enter serial port [default: COM6]: ").strip()
    if not port:
        port = "COM6"
    
    address = get_int_input("Enter locomotive address", default=3)
    
    print()
    print("=" * 70)
    print("Configuration Summary:")
    print("=" * 70)
    print(f"  Mask:    0x{mask:X} ({mask})")
    print(f"  DeltaP:  {deltaP:+d} μs")
    print(f"  DeltaN:  {deltaN:+d} μs")
    print(f"  Port:    {port}")
    print(f"  Address: {address}")
    print("=" * 70)
    print()
    
    # Confirm before running
    confirm = input("Run test with these parameters? [Y/n]: ").strip().lower()
    if confirm and confirm not in ['y', 'yes']:
        print("Test cancelled by user.")
        return 0
    
    # Build command to run AcceptanceTestWithNBit0Change.py
    script_dir = os.path.dirname(os.path.abspath(__file__))
    test_script = os.path.join(script_dir, "AcceptanceTestWithNBit0Change.py")
    
    if not os.path.exists(test_script):
        print(f"ERROR: Test script not found: {test_script}")
        return 1
    
    command = [
        sys.executable,
        test_script,
        "--mask", str(mask),
        "--deltaP", str(deltaP),
        "--deltaN", str(deltaN),
        "--port", port,
        "--address", str(address)
    ]
    
    print()
    print("=" * 70)
    print(f"Running: {' '.join(command)}")
    print("=" * 70)
    print()
    
    # Run the test script
    try:
        result = subprocess.run(command)
        return result.returncode
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user.")
        return 1
    except Exception as e:
        print(f"\nERROR: Failed to run test script: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
