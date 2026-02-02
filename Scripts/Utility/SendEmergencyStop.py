#!/usr/bin/env python3
"""
SendEmergencyStop Script
========================

Sends a single DCC emergency stop packet to a specific address or broadcast (0).
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

		request_json = json.dumps(request) + '\r\n'
		print(f"→ {request_json.strip()}")

		self.ser.write(request_json.encode('utf-8'))

		# Read response
		response_line = self.ser.readline().decode('utf-8').strip()
		print(f"← {response_line}")

		if response_line:
			return json.loads(response_line)
		return None

def calculate_dcc_checksum(bytes_list):
	"""
	Calculate DCC packet checksum (XOR of all bytes).

	Args:
		bytes_list: List of packet bytes (address + instruction)

	Returns:
		Checksum byte
	"""
	checksum = 0
	for byte in bytes_list:
		checksum ^= byte
	return checksum


def make_emergency_stop_packet(address):
	"""
	Create a DCC emergency stop packet.

	Args:
		address: Locomotive address (0 for broadcast to all locomotives)

	Returns:
		List of packet bytes
	"""
	# Advanced operations speed instruction: 0x3F
	# Emergency stop: speed = 1, direction = forward (bit 7 = 1)
	instruction = 0x3F
	speed_byte = (1 << 7) | 1  # 0x81

	packet = [address, instruction, speed_byte]
	checksum = calculate_dcc_checksum(packet)
	packet.append(checksum)

	return packet


def send_emergency_stop(rpc, address=0):
	"""
	Send an emergency stop packet using an already-open RPC connection.

	Args:
		rpc: DCCTesterRPC client instance (already connected)
		address: Locomotive address (0 for broadcast)
	"""
	estop_packet = make_emergency_stop_packet(address)
	response = rpc.send_rpc("command_station_load_packet", {"bytes": estop_packet})
	if response is None or response.get("status") != "ok":
		print(f"ERROR: Failed to load emergency stop packet: {response}")
		return 1

	response = rpc.send_rpc("command_station_transmit_packet", {"count": 1, "delay_ms": 0})
	if response is None or response.get("status") != "ok":
		print(f"ERROR: Failed to transmit emergency stop packet: {response}")
		return 1

	return 0
