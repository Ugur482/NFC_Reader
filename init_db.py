"""
init_db.py

One-time setup script for the NFC access-control SQLite database used by
serial_read.py. Creates the `whitelist` and `scans` tables if they don't
already exist, and seeds `whitelist` from the legacy accepted_UIDs.txt file
(if present) so existing entries aren't lost when switching off the
text-file whitelist.

Usage:
    python init_db.py
"""

import sqlite3

DB_FILE = "nfc_access.db"
LEGACY_WHITELIST_FILE = "accepted_UIDs.txt"

SCHEMA = """
CREATE TABLE IF NOT EXISTS whitelist (
    uid TEXT PRIMARY KEY,
    label TEXT,
    added_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
);

CREATE TABLE IF NOT EXISTS scans (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    uid TEXT NOT NULL,
    known INTEGER NOT NULL
);
"""


def seed_legacy_whitelist(conn):
    try:
        with open(LEGACY_WHITELIST_FILE, "r", encoding="utf-8") as f:
            uids = {line.strip().upper() for line in f if line.strip()}
    except FileNotFoundError:
        return

    conn.executemany(
        "INSERT OR IGNORE INTO whitelist (uid) VALUES (?)",
        [(uid,) for uid in uids],
    )


if __name__ == "__main__":
    with sqlite3.connect(DB_FILE) as conn:
        conn.executescript(SCHEMA)
        seed_legacy_whitelist(conn)
    print(f"Database ready at {DB_FILE}")
