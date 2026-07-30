"""
serial_uid_logger.py

Bridges the ESP8266/PN532 firmware to the access-control backend over
a serial connection during prototyping.

Listens on a serial port for "UID value:" lines printed by the firmware,
parses the UID, checks it against a whitelist stored in SQLite, logs the
scan with a timestamp to the same database, and writes the access
decision (0/1) back to the firmware over the same serial connection so
it can drive the LEDs.

This is a stand-in for the real Flask/SQLite backend query — useful for
bench-testing the firmware's read/decision/LED loop without a live
server. See NFC_Reader_Design_Document.pdf for the target architecture.

Run init_db.py once beforehand to create the database and tables.

Hardware: ESP8266 NodeMCU + PN532 (SPI), connected via USB serial.

Usage:
    python serial_uid_logger.py

Author: Can
Date: 2026-07
"""

import sqlite3
import time
from datetime import datetime

import serial

PORT = "COM5"                            # Windows COM port for the NodeMCU's USB-serial adapter
BAUD_RATE = 115200                       # Must match Serial.begin() in the firmware
DB_FILE = "nfc_access.db"                # SQLite DB holding the whitelist and scan log


def load_uid_data(conn):
    """Load known UIDs from the whitelist table into a set of UIDs."""
    rows = conn.execute("SELECT uid FROM whitelist").fetchall()
    return {row[0].upper() for row in rows}


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

conn = sqlite3.connect(DB_FILE)
known_uids = load_uid_data(conn)

# --- Main loop ---------------------------------------------------------------
# Reads firmware output line by line. Lines matching "UID value:" (a card
# read) or "TOKEN value:" (a phone answering our HCE AID) are processed the
# same way; everything else is echoed to stdout for debugging.

IDENTIFIER_PREFIXES = {
    "UID value:": "UID",
    "TOKEN value:": "TOKEN",
}

while True:
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    if line:
        print(repr(line))
    for prefix, scan_type in IDENTIFIER_PREFIXES.items():
        if not line.startswith(prefix):
            continue
        hex_bytes = line[len(prefix):].strip().split()
        uid = ":".join(b[2:].upper() for b in hex_bytes if b.startswith("0x"))
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        result = is_uid_known(uid, known_uids)
        conn.execute(
            "INSERT INTO scans (timestamp, uid, type, known) VALUES (?, ?, ?, ?)",
            (timestamp, uid, scan_type, result),
        )
        conn.commit()
        ser.write(f"{result}\n".encode("utf-8"))
        print(f"Known {scan_type}" if result else f"Unknown {scan_type}")
        break