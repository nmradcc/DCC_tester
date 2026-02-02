#!/usr/bin/env python3
"""
TestNoMotorStop Utility
=======================

Reads IO13 and IO14 inputs in a single RPC call.

- IO13 low => RUN_REV
- IO14 low => RUN_FWD

If both IO13 and IO14 are high, returns True.
If either is low, prints RUN status and returns False.
"""

import json


class DCCTesterRPC:
	"""RPC client for DCC_tester command station."""

	def __init__(self, ser):
		"""
		Initialize RPC client with an already-open serial port.

		Args:
			ser: Open serial port instance
		"""
		self.ser = ser

	def send_rpc(self, method, params):
		"""
		Send an RPC request and return the response.

		Args:
			method: RPC method name
			params: Dictionary of parameters

		Returns:
			Response dictionary
		"""
		request = {
			"method": method,
			"params": params
		}

		request_json = json.dumps(request) + "\r\n"
		print(f"→ {request_json.strip()}")

		self.ser.write(request_json.encode("utf-8"))

		# Read response
		response_line = self.ser.readline().decode("utf-8").strip()
		print(f"← {response_line}")

		if response_line:
			return json.loads(response_line)
		return None


RUN_REV_PIN = 13
RUN_FWD_PIN = 14


def _is_pin_high(gpio_word, pin_num):
	bit_index = pin_num - 1
	return (gpio_word & (1 << bit_index)) != 0


def read_run_inputs(rpc):
	"""
	Read IO13/IO14 in a single RPC call and return run status.

	Args:
		rpc: DCCTesterRPC client instance (already connected)

	Returns:
		True if both IO13 and IO14 are HIGH, otherwise False.
	"""
	response = rpc.send_rpc("get_gpio_inputs", {})
	if response is None or response.get("status") != "ok":
		print(f"ERROR: Failed to read GPIO inputs: {response}")
		return False

	if "value" not in response:
		print(f"ERROR: Missing GPIO value in response: {response}")
		return False

	gpio_word = response.get("value", 0)
	io13_high = _is_pin_high(gpio_word, RUN_REV_PIN)
	io14_high = _is_pin_high(gpio_word, RUN_FWD_PIN)

	if io13_high and io14_high:
		return True

	run_rev = not io13_high
	run_fwd = not io14_high

	if run_rev and run_fwd:
		print("RUNNING: REV + FWD (IO13 low, IO14 low)")
	elif run_rev:
		print("RUNNING: REV (IO13 low)")
	elif run_fwd:
		print("RUNNING: FWD (IO14 low)")
	return False
