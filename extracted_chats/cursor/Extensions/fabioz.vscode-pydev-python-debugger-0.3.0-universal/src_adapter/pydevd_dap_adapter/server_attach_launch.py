# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
import sys

if __name__ == "__main__":
    try:
        msg = sys.argv[1]
    except Exception:
        msg = "Waiting for new connections..."

    print(msg)

    import time

    while True:
        time.sleep(50000)
