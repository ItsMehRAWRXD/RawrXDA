#!/usr/bin/env python3
"""
RawrXD Chat Server — delegates to Ship/RawrEngine.py
=====================================================
The chat panel in the Win32 IDE calls start_server()/stop_server().
This module re-exports those from the unified RawrEngine so there
is a single implementation.
"""

import sys
import os

# Ensure Ship/ is importable
_here = os.path.dirname(os.path.abspath(__file__))
if _here not in sys.path:
    sys.path.insert(0, _here)

from RawrEngine import start_server, stop_server, is_server_running, main  # noqa: F401,E402


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="RawrXD Chat Server")
    parser.add_argument("--port", type=int, default=23959)
    parser.add_argument("--stop", action="store_true")
    args = parser.parse_args()

    if args.stop:
        stop_server()
    else:
        if start_server(args.port):
            print(f"[Chat Server] Running on port {args.port}. Ctrl+C to stop.")
            try:
                while True:
                    pass
            except KeyboardInterrupt:
                stop_server()
        else:
            sys.exit(1)
