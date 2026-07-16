"""
serial_uid_logger.py

Bridges the ESP8266/PN532 firmware to the access-control backend over
a serial connection during prototyping.

Listens on a serial port for "UID value:" lines printed by the firmware,
parses the UID, checks it against a local whitelist file, logs the scan
with a timestamp, and writes the access decision (0/1) back to the
firmware over the same serial connection so it can drive the LEDs.

This is a stand-in for the real Flask/SQLite backend query — useful for
bench-testing the firmware's read/decision/LED loop without a live
server. See NFC_Reader_Design_Document.pdf for the target architecture.

Hardware: ESP8266 NodeMCU + PN532 (SPI), connected via USB serial.

Usage:
    python serial_uid_logger.py

Author: Can
Date: 2026-07
"""

import time
from datetime import datetime

import serial

PORT = "COM5"                           # Windows COM port for the NodeMCU's USB-serial adapter
BAUD_RATE = 115200                      # Must match Serial.begin() in the firmware
OUTPUT_FILE = "uid_log.txt"             # Append-only scan history: timestamp + UID
ACCEPTED_UIDS_FILE = "accepted_UIDs.txt"    # Whitelist, one UID per line (format: "AA:BB:CC:DD")


def load_uid_data(filepath):
    """Load known UIDs (one per line) from filepath into a set of UIDs."""
    with open(filepath, "r", encoding="utf-8") as f:
        return {line.strip().upper() for line in f if line.strip()}


def is_uid_known(uid, known_uids):
    """Return 1 if uid is present in known_uids, 0 otherwise."""
    return 1 if uid.upper() in known_uids else 0

# --- Serial setup -----------------------------------------------------------

ser = serial.Serial(PORT, BAUD_RATE, timeout=1)

#Disabling DTR/RTS prevents the NodeMCU from auto-resetting when the
# serial port opens (some ESP8266 boards reset on DTR toggle, which would
# otherwise interrupt the firmware right as we start listening).
ser.setDTR(False)
ser.setRTS(False)
time.sleep(1)                           # give the port time to settle before flushing
ser.reset_input_buffer()                # discard any stale bytes buffered before we were ready

known_uids = load_uid_data(ACCEPTED_UIDS_FILE)

# --- Main loop ---------------------------------------------------------------
# Reads firmware output line by line. Only lines matching "UID value:"
# (printed by the ESP8266 after a successful PN532 read) are processed;
# everything else is echoed to stdout for debugging.

with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
    while True:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if line:
            print(repr(line))
        if line.startswith("UID value:"):
            hex_bytes = line[len("UID value:"):].strip().split()
            uid = ":".join(b[2:].upper() for b in hex_bytes if b.startswith("0x"))
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            f.write(f"{timestamp}\t{uid}\n")
            f.flush()
            result = is_uid_known(uid, known_uids)
            ser.write(f"{result}\n".encode("utf-8"))
            print("Known UID" if result else "Unknown UID")