"""A test agent that prints heartbeat for a few iterations and then hangs silently.
"""
import time
import sys

if __name__ == '__main__':
    print('[hang_agent] started')
    sys.stdout.flush()
    for i in range(3):
        print(f'[hang_agent] heartbeat {i}')
        sys.stdout.flush()
        time.sleep(1)
    # Now hang silently (no more output), but keep process alive
    time.sleep(30)
    print('[hang_agent] done')
    sys.stdout.flush()
    sys.exit(0)
