"""A small test agent that prints a heartbeat and exits after a short time.
Meant to be used with the self-healing supervisor for testing restart behavior.
"""
import time
import sys

if __name__ == '__main__':
    print('[agent] started')
    sys.stdout.flush()
    # run for a bit and exit to simulate a crash
    for i in range(3):
        print(f'[agent] heartbeat {i}')
        sys.stdout.flush()
        time.sleep(1)
    print('[agent] exiting')
    sys.stdout.flush()
    # Exit with non-zero to indicate crash
    sys.exit(1)
