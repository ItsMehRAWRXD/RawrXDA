import subprocess
import time
import os

CONFIG = 'tools/self_heal_config_hang.json'
SUPERVISOR_CMD = ['python', 'tools/self_heal.py', '--config', CONFIG]


def test_supervisor_restarts_on_hang():
    os.makedirs('build', exist_ok=True)
    sup_proc = subprocess.Popen(SUPERVISOR_CMD)
    try:
        time.sleep(2)
        log_path = 'build/self_heal_hang_agent.log'
        for _ in range(10):
            if os.path.exists(log_path):
                with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                if 'started' in content:
                    break
            time.sleep(0.5)
        else:
            raise AssertionError('Hang agent log not found or not started')

        # Now wait for hang detection: agent emits 3 heartbeats then hangs; supervisor should restart
        time.sleep(8)
        with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        # Should have at least two 'started' occurrences, agent should have been restarted due to lack of heartbeat
        starts = content.count('started')
        if starts < 2:
            raise AssertionError('Supervisor did not restart hung agent')
        print('[test] Supervisor restarted hung agent successfully')
    finally:
        sup_proc.terminate()
        try:
            sup_proc.wait(timeout=3)
        except Exception:
            sup_proc.kill()


if __name__ == '__main__':
    test_supervisor_restarts_on_hang()
    print('OK')
