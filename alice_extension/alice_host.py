#!/usr/bin/env python3
"""
Alice Native Messaging Host
===========================
Chrome launches this via alice_host.bat when the extension calls connectNative().
DO NOT run this script manually — Chrome must be the parent process so that
stdin/stdout are wired to the native messaging protocol.

Communication:
  stdin/stdout  : 4-byte little-endian length prefix + UTF-8 JSON
  web_prompt.txt: Alice writes a prompt here; we forward it to Chrome
  web_response.txt: we write Chrome's reply here; Alice reads it
  bridge.log    : full debug log
"""

import sys
import json
import struct
import threading
import time
import os
import logging

# ── Paths ─────────────────────────────────────────────────────────────────────
ALICE_DIR     = r"C:\Users\JASKA\AppData\Roaming\Alice"
PROMPT_FILE   = os.path.join(ALICE_DIR, "web_prompt.txt")
RESPONSE_FILE = os.path.join(ALICE_DIR, "web_response.txt")
LOG_FILE      = os.path.join(ALICE_DIR, "bridge.log")

os.makedirs(ALICE_DIR, exist_ok=True)

# ── Logging — file + stderr ────────────────────────────────────────────────────
logging.basicConfig(
    filename=LOG_FILE,
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("alice_bridge")

def dbg(msg):
    """Log to both file and stderr (stderr goes to bridge.log via Chrome's error pipe)."""
    log.debug(msg)
    print(f"[AliceHost] {msg}", file=sys.stderr, flush=True)

def info(msg):
    log.info(msg)
    print(f"[AliceHost] INFO  {msg}", file=sys.stderr, flush=True)

def warn(msg):
    log.warning(msg)
    print(f"[AliceHost] WARN  {msg}", file=sys.stderr, flush=True)

def err(msg):
    log.error(msg)
    print(f"[AliceHost] ERROR {msg}", file=sys.stderr, flush=True)

# ── I/O lock so reads/writes don't interleave ─────────────────────────────────
stdout_lock = threading.Lock()

# ── Native messaging protocol ─────────────────────────────────────────────────
def read_message():
    """Read one JSON message from Chrome (4-byte LE length + UTF-8 payload)."""
    raw_len = sys.stdin.buffer.read(4)
    if not raw_len or len(raw_len) < 4:
        warn("stdin: got empty/short length header — Chrome closed the pipe")
        return None
    msg_len = struct.unpack("<I", raw_len)[0]
    dbg(f"stdin: reading {msg_len} bytes")
    raw_msg = sys.stdin.buffer.read(msg_len)
    if not raw_msg:
        warn("stdin: got empty body after length header")
        return None
    parsed = json.loads(raw_msg.decode("utf-8"))
    dbg(f"← Chrome: {parsed}")
    return parsed


def send_message(obj):
    """Send one JSON message to Chrome (4-byte LE length + UTF-8 payload)."""
    data = json.dumps(obj).encode("utf-8")
    with stdout_lock:
        sys.stdout.buffer.write(struct.pack("<I", len(data)))
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
    dbg(f"→ Chrome: {obj}")


# ── Thread: read responses from Chrome → write to response file ───────────────
def chrome_reader():
    info("chrome_reader thread started — waiting for messages from Chrome")
    while True:
        try:
            msg = read_message()
            if msg is None:
                warn("chrome_reader: stdin closed — Chrome disconnected or exited")
                break

            response_text = msg.get("response", "")
            if response_text:
                info(f"Response from Chrome: {len(response_text)} chars")
                try:
                    with open(RESPONSE_FILE, "w", encoding="utf-8") as f:
                        f.write(response_text)
                    info(f"Written to {RESPONSE_FILE}")
                except Exception as e:
                    err(f"Failed to write response file: {e}")
            else:
                dbg(f"Chrome message had no 'response' field: {msg}")

        except Exception as e:
            err(f"chrome_reader exception: {e}")
            time.sleep(0.5)


# ── Main loop: poll prompt file → forward to Chrome ───────────────────────────
def main():
    info(f"Alice native messaging host started (pid={os.getpid()})")
    info(f"Prompt file  : {PROMPT_FILE}")
    info(f"Response file: {RESPONSE_FILE}")
    info(f"Log file     : {LOG_FILE}")

    # Start background reader thread
    reader = threading.Thread(target=chrome_reader, daemon=True)
    reader.start()
    info("chrome_reader thread launched")

    # Clear stale files from previous sessions
    for path in (PROMPT_FILE, RESPONSE_FILE):
        try:
            if os.path.exists(path):
                open(path, "w").close()
                dbg(f"Cleared stale file: {path}")
        except Exception as e:
            warn(f"Could not clear {path}: {e}")

    info(f"Polling {PROMPT_FILE} every 500ms...")
    poll_count = 0

    while True:
        try:
            poll_count += 1
            if poll_count % 20 == 0:                       # log alive every 10s
                dbg(f"Still polling... ({poll_count} ticks)")

            if os.path.exists(PROMPT_FILE):
                with open(PROMPT_FILE, "r", encoding="utf-8") as f:
                    prompt = f.read().strip()

                if prompt:
                    info(f"Prompt detected ({len(prompt)} chars): {prompt[:80]!r}")

                    # Clear prompt file so Alice knows we picked it up
                    with open(PROMPT_FILE, "w", encoding="utf-8") as f:
                        f.write("")
                    dbg("Prompt file cleared")

                    # Clear previous response
                    with open(RESPONSE_FILE, "w", encoding="utf-8") as f:
                        f.write("")
                    dbg("Response file cleared")

                    # Forward to Chrome
                    send_message({"prompt": prompt})
                    info(f"Prompt forwarded to Chrome")

        except Exception as e:
            err(f"Main loop error: {e}")

        time.sleep(0.5)


if __name__ == "__main__":
    info("=== alice_host.py starting ===")
    try:
        main()
    except KeyboardInterrupt:
        info("Interrupted by keyboard — exiting")
    except Exception as e:
        err(f"Unhandled exception: {e}")
        log.critical("Unhandled exception", exc_info=True)
        sys.exit(1)
