# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from functools import partial
import itertools
import os
import pathlib
import sys
import threading
import time
from typing import Dict, List, Optional, Tuple

from pydevd_dap_adapter import _dap_log


log = _dap_log


def create_run_with_env_code(
    env: Dict[str, str], base_executable_and_args: List[str]
) -> str:
    """
    :param env:
        This is the environment

    :param executable:
        This is the executable which should be called (usually the path to python.exe)
    """
    set_vars = []
    as_dict: Dict[str, str] = {}
    found_new_line = False

    for key, value in env.items():
        if sys.platform == "win32":
            # Reference (just text, not code): https://stackoverflow.com/a/16018942/110451
            value = value.replace("^", "^^")
            value = value.replace("%", "%%")
            value = value.replace("!", "^!")
            value = value.replace("|", "^|")
            value = value.replace("&", "^&")
            value = value.replace(">", "^>")
            value = value.replace("<", "^<")
            value = value.replace("'", "^'")

            if "\n" in value or "\r" in value:
                found_new_line = True
                value = value.replace("\r\n", "\n").replace("\r", "\n")
                value = value.replace("\n", "!__NEW_LINE_IN_ENV__!")

            set_vars.append(f'SET "{key}={value}"')
        else:
            # Reference (just text, not code): https://stackoverflow.com/a/20053121/110451
            value = value.replace("'", "'\\''")
            value = f"'{value}'"
            set_vars.append(f"export {key}={value}")
        as_dict[key] = value

    set_vars_as_str = "\n".join(set_vars)

    if found_new_line:
        if sys.platform == "win32":
            new_line_preamble = """
setlocal EnableDelayedExpansion
(set __NEW_LINE_IN_ENV__=^
%=Do not remove this line=%
)
"""
            set_vars_as_str = new_line_preamble + set_vars_as_str

    import subprocess

    if sys.platform == "win32":
        shebang = "@echo off"
        executable_with_args = f"{subprocess.list2cmdline(base_executable_and_args)} %*"
    else:
        shebang = "#!/usr/bin/env bash"
        executable_with_args = (
            f'{subprocess.list2cmdline(base_executable_and_args)} "$@"'
        )

    code = f"""{shebang}

{set_vars_as_str}
{executable_with_args}
"""

    return code


_next_number: "partial[int]" = partial(next, itertools.count())


def _compute_path_for_env(temp_dir: Optional[str] = None) -> pathlib.Path:
    import tempfile

    if not temp_dir:
        temp_dir = os.path.join(tempfile.gettempdir(), "pydevd-run")

    os.makedirs(temp_dir, exist_ok=True)
    f = tempfile.mktemp(
        suffix=(".bat" if sys.platform == "win32" else ".sh"),
        prefix="run_env_%02d_" % _next_number(),
        dir=temp_dir,
    )

    _delete_in_thread(temp_dir)
    return pathlib.Path(f)


def _delete_in_thread(temp_dir) -> threading.Thread:
    t = threading.Thread(target=_delete_old, args=(temp_dir,))
    t.daemon = True
    t.start()
    return t


def _delete_old(temp_dir: str):
    try:
        # Remove files only after 2 days.
        one_day_in_seconds = 86400
        delete_older_than = time.time() - (one_day_in_seconds * 2)

        f = pathlib.Path(temp_dir)
        for entry in os.scandir(f):
            if entry.name.startswith("run_env_"):
                if entry.stat().st_mtime < delete_older_than:
                    remove = f / entry.name
                    try:
                        remove.unlink()
                    except:
                        log.debug("Unable to remove: %s", remove)
    except:
        log.exception("Error removing old launch files.")


def write_as_script(code: str, script_path: pathlib.Path):
    script_path.write_text(code, "utf-8", "replace")

    if sys.platform != "win32":
        # We need to make it executable...
        import stat

        st = os.stat(str(script_path))
        os.chmod(str(script_path), st.st_mode | stat.S_IEXEC)


def disable_launch_env_script():
    return os.environ.get("PYDEVD_LAUNCH_ENV_SCRIPT", "1").lower() in (
        "0",
        "false",
    )


def _update_command_line_to_write_pid(cmdline: List[str], env: dict, write_pid_to: str):
    from pydevd_dap_adapter import run_and_save_pid

    new_cmdline = [sys.executable, run_and_save_pid.__file__, write_pid_to] + cmdline
    return new_cmdline, env


def update_cmdline_and_env(
    cmdline: List[str], env: Dict[str, str], write_pid_to: Optional[str] = None
) -> Tuple[List[str], Dict[str, str]]:
    """
    Ideally only this function is actually used from this module.

    It receives an existing command line and environment and provides a new
    command line and environment to be used depending which should have the
    same effect when running.

    :param write_pid_to: if passed, the launch will be made in a way that
        a wrapper script is used to launch the script and then write the
        pid of the launched executable to the passed file.
    """
    if write_pid_to:
        cmdline, env = _update_command_line_to_write_pid(cmdline, env, write_pid_to)
        embed_args = 3
    else:
        embed_args = 1

    if disable_launch_env_script():
        return cmdline, env

    set_env_and_run_code = create_run_with_env_code(env, cmdline[:embed_args])
    if len(set_env_and_run_code) > 240:
        script_path = _compute_path_for_env()
        write_as_script(set_env_and_run_code, script_path)
        new_cmdline = [str(script_path)] + cmdline[embed_args:]

        return new_cmdline, {}
    return cmdline, env
