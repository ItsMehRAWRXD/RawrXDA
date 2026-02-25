# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
import os
import sys
import threading

from pydevd_dap_adapter import _dap_log


PARENT_PROCESS_WATCH_INTERVAL = 3  # 3 s

if sys.platform == "win32":
    import ctypes

    kernel32 = ctypes.windll.kernel32
    PROCESS_SYNCHRONIZE = 0x00100000
    DWORD = ctypes.c_uint32
    BOOL = ctypes.c_int
    LPVOID = ctypes.c_void_p
    HANDLE = LPVOID

    OpenProcess = kernel32.OpenProcess
    OpenProcess.argtypes = [DWORD, BOOL, DWORD]
    OpenProcess.restype = HANDLE

    WaitForSingleObject = kernel32.WaitForSingleObject
    WaitForSingleObject.argtypes = [HANDLE, DWORD]
    WaitForSingleObject.restype = DWORD

    WAIT_TIMEOUT = 0x00000102
    WAIT_ABANDONED = 0x00000080
    WAIT_OBJECT_0 = 0
    WAIT_FAILED = 0xFFFFFFFF

    def is_process_alive(pid):
        """Check whether the process with the given pid is still alive.

        Running `os.kill()` on Windows always exits the process, so it can't be used to check for an alive process.
        see: https://docs.python.org/3/library/os.html?highlight=os%20kill#os.kill

        Hence ctypes is used to check for the process directly via windows API avoiding any other 3rd-party dependency.

        Args:
            pid (int): process ID

        Returns:
            bool: False if the process is not alive or don't have permission to check, True otherwise.
        """
        process = OpenProcess(PROCESS_SYNCHRONIZE, 0, pid)
        if process != 0:
            try:
                wait_result = WaitForSingleObject(process, 0)
                if wait_result == WAIT_TIMEOUT:
                    return True
            finally:
                kernel32.CloseHandle(process)
        return False

else:
    import errno

    def _is_process_alive(pid):
        """Check whether the process with the given pid is still alive.

        Args:
            pid (int): process ID

        Returns:
            bool: False if the process is not alive or don't have permission to check, True otherwise.
        """
        if pid < 0:
            return False
        try:
            os.kill(pid, 0)
        except OSError as e:
            if e.errno == errno.ESRCH:
                return False  # No such process.
            elif e.errno == errno.EPERM:
                return True  # permission denied.
            else:
                _dap_log.info("Unexpected errno: %s", e.errno)
                return False
        else:
            return True

    def is_process_alive(pid):
        import subprocess

        if _is_process_alive(pid):
            # Check if zombie...
            try:
                cmd = ["ps", "-p", str(pid), "-o", "stat"]
                try:
                    process = subprocess.Popen(
                        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE
                    )
                except:
                    _dap_log.exception("Error calling: %s.", " ".join(cmd))
                else:
                    stdout, _ = process.communicate()
                    stdout = stdout.decode("utf-8", "replace")
                    lines = [line.strip() for line in stdout.splitlines()]
                    if len(lines) > 1:
                        if lines[1].startswith("Z"):
                            return False  # It's a zombie
            except:
                _dap_log.exception("Error checking if process is alive.")

            return True
        return False


def _popen(cmdline, **kwargs):
    import subprocess

    try:
        return subprocess.Popen(cmdline, **kwargs)
    except:
        _dap_log.exception("Error running: %s", (" ".join(cmdline)))
        return None


def _call(cmdline, **kwargs):
    import subprocess

    try:
        subprocess.check_call(cmdline, **kwargs)
    except:
        _dap_log.exception("Error running: %s", (" ".join(cmdline)))
        return None


def _kill_process_and_subprocess_linux(pid):
    import subprocess

    initial_pid = pid

    def list_children_and_stop_forking(ppid):
        children_pids = []
        _call(
            ["kill", "-STOP", str(ppid)], stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        list_popen = _popen(
            ["pgrep", "-P", str(ppid)], stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        if list_popen is not None:
            stdout, _ = list_popen.communicate()
            for line in stdout.splitlines():
                line = line.decode("ascii").strip()
                if line:
                    pid = str(line)
                    children_pids.append(pid)
                    # Recursively get children.
                    children_pids.extend(list_children_and_stop_forking(pid))
        return children_pids

    previously_found = set()

    for _ in range(50):  # Try this at most 50 times before giving up.
        children_pids = list_children_and_stop_forking(initial_pid)
        found_new = False

        for pid in children_pids:
            if pid not in previously_found:
                found_new = True
                previously_found.add(pid)
                _call(
                    ["kill", "-KILL", str(pid)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )

        if not found_new:
            break

    # Now, finish the initial one.
    _call(
        ["kill", "-KILL", str(initial_pid)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def kill_process_and_subprocesses(pid):
    _dap_log.debug("Killing process and subprocesses of: %s", pid)
    from subprocess import CalledProcessError

    if sys.platform == "win32":
        import subprocess

        args = ["taskkill", "/F", "/PID", str(pid), "/T"]
        retcode = subprocess.call(
            args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE
        )
        if retcode not in (0, 128, 255):
            raise CalledProcessError(retcode, args)
    else:
        _kill_process_and_subprocess_linux(pid)


_track_pids_to_exit = set()
_watching_thread_global = None


def exit_when_pid_exists(pid):
    _track_pids_to_exit.add(pid)
    global _watching_thread_global
    if _watching_thread_global is None:
        import time

        def watch_parent_process():
            # exit when any of the ids we're tracking exit.
            while True:
                for pid in _track_pids_to_exit:
                    if not is_process_alive(pid):
                        # Note: just exit since the parent process already
                        # exited.
                        _dap_log.info(
                            f"Force-quit process: %s because parent: %s exited",
                            os.getpid(),
                            pid,
                        )
                        os._exit(0)

                time.sleep(PARENT_PROCESS_WATCH_INTERVAL)

        _watching_thread_global = threading.Thread(target=watch_parent_process, args=())
        _watching_thread_global.daemon = True
        _watching_thread_global.start()
