import subprocess
import time
import os

# This simple test ensures the supervisor restarts an agent that exits with error.

CONFIG = 'tools/self_heal_config.json'
SUPERVISOR_CMD = ['python', 'tools/self_heal.py', '--config', CONFIG]


def test_supervisor_restarts_agent():
    # Ensure build dir exists
    os.makedirs('build', exist_ok=True)

    sup_proc = subprocess.Popen(SUPERVISOR_CMD)
    try:
        # Wait for supervisor to start monitored process and agent to appear
        time.sleep(2)
        # Read log and confirm agent started
        log_path = 'build/self_heal_test_agent.log'
        for _ in range(10):
            if os.path.exists(log_path):
                with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                if 'started' in content:
                    print('[test] Agent started; now waiting for exit')
                    break
            time.sleep(0.5)
        else:
            raise AssertionError('Agent log not found or not started')

        # Wait longer than agent lifecycle so it exits and triggers restart
        time.sleep(5)
        with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        starts = content.count('started')
        if starts < 2:
            raise AssertionError('Supervisor did not restart agent')
        print('[test] Supervisor restarted agent successfully')

    finally:
        sup_proc.terminate()
        try:
            sup_proc.wait(timeout=3)
        except Exception:
            sup_proc.kill()


if __name__ == '__main__':
    test_supervisor_restarts_agent()
    print('OK')
