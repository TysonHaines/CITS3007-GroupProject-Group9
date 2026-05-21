#!/usr/bin/env python3
# =============================================================================
# AI ASSISTANCE DECLARATION
# This file was developed with the assistance of Claude.
# Specifically, AI was used to:
# 1. /proc polling approach: Claude suggested reading VmHWM from
#    /proc/PID/status as a portable peak-RSS fallback on Linux.
# 2. Threading pattern: Claude generated the daemon thread and nonlocal
#    peak_kb polling loop structure.
# =============================================================================
"""Run a command and report its peak RSS by polling /proc/PID/status.

Used as a fallback when /usr/bin/time -v isn't available on the SDE.
Output format mimics the relevant /usr/bin/time -v line:
  Maximum resident set size (kbytes): <N>
"""
import os
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) < 2:
        print("Usage: measure_rss.py <cmd> [args...]", file=sys.stderr)
        sys.exit(1)

    cmd = sys.argv[1:]
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    peak_kb = 0
    running = True

    def poll():
        nonlocal peak_kb
        pid = proc.pid
        while running:
            try:
                with open(f"/proc/{pid}/status") as f:
                    for line in f:
                        if line.startswith("VmHWM:") or line.startswith("VmRSS:"):
                            kb = int(line.split()[1])
                            if kb > peak_kb:
                                peak_kb = kb
                            break
            except (FileNotFoundError, ProcessLookupError):
                return
            time.sleep(0.05)

    t = threading.Thread(target=poll, daemon=True)
    t.start()
    proc.wait()
    running = False
    t.join(timeout=0.5)

    # /usr/bin/time -v compatible line
    print(f"\tMaximum resident set size (kbytes): {peak_kb}")
    sys.exit(proc.returncode)


if __name__ == "__main__":
    main()
