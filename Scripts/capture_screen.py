#!/usr/bin/env python3
"""
Capture Screen Module
=====================

This module captures a screenshot from the secondary monitor and saves it
with a timestamp. Called when user presses 'c' during test execution.
"""

import os
import sys
from datetime import datetime
from pathlib import Path

try:
    import mss
    MSS_AVAILABLE = True
except ImportError:
    MSS_AVAILABLE = False

# Configuration
MONITOR_INDEX = 2  # 1 = primary, 2 = secondary, etc.


def capture_screen():
    """Capture the secondary screen (monitor 2) and save with timestamp."""
    if not MSS_AVAILABLE:
        print("Error: mss not installed. Install with: pip install mss")
        return False
    
    try:
        # Get the script directory and create screenshots folder
        script_dir = Path(__file__).parent
        screenshots_dir = script_dir / "screenshots"
        screenshots_dir.mkdir(exist_ok=True)
        
        # Generate filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = screenshots_dir / f"screen_capture_{timestamp}.png"
        
        # Capture screenshot from configured monitor
        with mss.mss() as sct:
            # Monitor 0 is all monitors, Monitor 1 is primary, Monitor 2 is secondary
            monitors = sct.monitors
            
            if len(monitors) <= MONITOR_INDEX:  # monitors[0] is all monitors
                print(f"Warning: Monitor {MONITOR_INDEX} not available (only {len(monitors) - 1} monitor(s) detected)")
                print("Capturing from primary monitor instead")
                monitor_num = 1
            else:
                monitor_num = MONITOR_INDEX
                print(f"Capturing from monitor {monitor_num}...")
            
            # Capture the screenshot
            screenshot = sct.grab(sct.monitors[monitor_num])
            
            # Save to PNG file
            mss.tools.to_png(screenshot.rgb, screenshot.size, output=str(filename))
        
        print(f"✓ Screenshot saved: {filename}")
        return True
        
    except Exception as e:
        print(f"Error capturing screen: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Main entry point when called as a script."""
    print("=" * 70)
    print("Screen Capture Utility")
    print("=" * 70)
    print()
    
    success = capture_screen()
    
    print()
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
