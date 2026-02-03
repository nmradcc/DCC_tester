NMRA S‑9.1 & S‑9.2 Automated DCC Decoder Compliance Test Plan
This document defines an automated test plan for verifying compliance with:

NMRA S‑9.1 — Electrical Standards for Digital Command Control

NMRA S‑9.2 — Digital Command Control Packet Standards

Together, these standards ensure that a DCC decoder can safely receive power, interpret DCC packets, and behave predictably on any NMRA‑compliant layout.

1. Scope
This test plan covers:

1.1 S‑9.1 (Electrical)
Track voltage limits

Waveform characteristics

Current handling

Short‑circuit behavior

Power‑on behavior

Electrical isolation

1.2 S‑9.2 (Packet Format & Interpretation)
Packet structure

Packet timing

Addressing

Speed & direction

Function groups

Error detection

Packet rejection

This document does not include S‑9.2.1/9.2.2 programming mode tests or RP‑9.x recommendations.

2. Required Test Equipment
Programmable DCC packet generator

Oscilloscope or logic analyzer (≥ 10 MHz)

Programmable electronic load

Current and voltage measurement tools

Automated response logger

Temperature monitoring (optional)

3. S‑9.1 Electrical Compliance
3.1 Track Voltage & Waveform
3.1.1 Voltage Range
Sweep track voltage across the allowed range for the target scale.

Verify decoder powers up and remains stable.

3.1.2 Waveform Recognition
Apply a compliant DCC square‑wave signal.

Verify decoder correctly interprets packets.

3.1.3 Distorted Waveform Tolerance
Introduce controlled rise/fall time variations and asymmetry.

Verify decoder still interprets valid packets.

3.2 Current Handling
3.2.1 Steady‑State Current
Apply typical motor and function loads.

Verify decoder remains within rated current.

3.2.2 Stall Current
Simulate motor stall.

Verify decoder limits current or shuts down safely.

3.3 Short‑Circuit Protection
3.3.1 Motor Output Short
Short motor output.

Verify decoder protects itself and recovers.

3.3.2 Function Output Short
Short each function output individually.

Verify decoder limits current and recovers.

3.4 Power‑On Behavior
3.4.1 Inrush Current
Measure peak current at power‑on.

Verify it does not exceed typical booster limits.

3.4.2 Startup Stability
Power cycle at minimum and nominal voltages.

Verify decoder boots consistently.

3.5 Electrical Isolation
Verify isolation between track, motor, function outputs, and logic circuits.

Confirm no unintended continuity or leakage.

4. S‑9.2 Packet Structure Compliance
4.1 Preamble
4.1.1 Valid Preamble
Send packets with legal preamble length.

Verify decoder accepts them.

4.1.2 Shortened Preamble
Send packets with insufficient preamble.

Verify decoder ignores them.

4.2 Start Bit
4.2.1 Valid Start Bit
Send packets with correct start bit.

Verify decoder accepts them.

4.2.2 Invalid Start Bit
Inject malformed start bits.

Verify decoder ignores them.

4.3 Data Bytes
4.3.1 Valid Bit Timing
Send packets with correct “1” and “0” timing.

Verify decoder accepts them.

4.3.2 Out‑of‑Spec Timing
Shorten or lengthen bit times beyond S‑9.2 limits.

Verify decoder rejects them.

4.4 Error Detection Byte
4.4.1 Correct XOR
Send packets with correct XOR error byte.

Verify decoder acts on them.

4.4.2 Incorrect XOR
Corrupt the error byte.

Verify decoder ignores the packet.

5. Addressing Compliance
5.1 Primary (Short) Address
5.1.1 Matching Address
Send packets to decoder’s assigned address.

Verify decoder responds.

5.1.2 Non‑Matching Address
Send packets to other addresses.

Verify decoder ignores them.

5.2 Broadcast Packets
5.2.1 Broadcast Behavior
Send broadcast packets.

Verify decoder responds according to S‑9.2 rules.

6. Speed & Direction Packet Compliance
6.1 Direction Bit
6.1.1 Forward
Send forward direction packet.

Verify decoder moves forward.

6.1.2 Reverse
Send reverse direction packet.

Verify decoder moves reverse.

6.2 Speed Values
6.2.1 Valid Speed Codes
Send all valid speed values.

Verify consistent response.

6.2.2 Emergency Stop
Send E‑stop packet.

Verify decoder stops immediately.

7. Function Group Packet Compliance
7.1 Function Groups
7.1.1 F0–F4
Toggle each function bit.

Verify correct output behavior.

7.1.2 F5–F8
Toggle each function bit.

Verify correct output behavior.

7.1.3 Additional Groups (If Implemented)
Send packets for higher function groups.

Verify correct interpretation.

8. Timing Tolerance Compliance
8.1 Bit Timing
8.1.1 Within Limits
Vary bit timing within S‑9.2 limits.

Verify decoder accepts packets.

8.1.2 Outside Limits
Exceed timing limits.

Verify decoder rejects packets.

8.2 Inter‑Packet Spacing
8.2.1 Minimum Spacing
Send packets with minimal legal spacing.

Verify decoder accepts them.

8.2.2 Long Gaps
Send packets with extended gaps.

Verify decoder does not misinterpret gaps as resets.

9. Packet Rejection Behavior
9.1 Malformed Packets
9.1.1 Missing Data Byte
Send packet missing a data byte.

Verify decoder ignores it.

9.1.2 Extra Data Byte
Send packet with an extra data byte.

Verify decoder ignores it.

9.2 Noise & Jitter
9.2.1 Injected Jitter
Add jitter within reasonable limits.

Verify decoder still interprets valid packets.

9.2.2 Noise Pulses
Inject random noise pulses.

Verify decoder does not act on noise.

10. Automated Test Flow
Initialize test bench

Run S‑9.1 electrical tests

Run S‑9.2 packet structure tests

Run addressing tests

Run speed/direction tests

Run function group tests

Run timing tolerance tests

Run rejection/error‑handling tests

Log results

Generate compliance report