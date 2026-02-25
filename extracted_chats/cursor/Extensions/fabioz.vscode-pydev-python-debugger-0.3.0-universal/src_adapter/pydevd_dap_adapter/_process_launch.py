# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from __future__ import annotations

import os.path
import tempfile
import threading
from typing import List, Union, Tuple, Optional
import typing

from pydevd_dap_adapter import _dap_log
from .constants import DebugInfoHolder
from .debug_adapter_core.dap.pydevd_schema import (
    LaunchRequest,
    LaunchResponse,
    DisconnectRequest,
)
from ._process_base import _ProcessBase

from .launch_process_pydevd_comm import (
    LaunchProcessDebugAdapterPydevdComm,
)
from ._comm import wait_for_connection
import subprocess

if typing.TYPE_CHECKING:
    from pydevd_dap_adapter.debug_adapter_comm import DebugAdapterComm

log = _dap_log


def compute_cmd_line_and_env(
    pydevd_py_debugger_file: Optional[str],
    target: Optional[Union[str, List[str]]],
    module: Optional[str],
    port: int,
    args: List[str],
    run_in_debug_mode: bool,
    env: dict,
    multiprocess: bool,
) -> Tuple[List[str], dict]:
    """
    Note that cwd and target MUST be absolute at this point.
    """
    import sys
    from ._debugger_runner import DebuggerRunner

    new_env = env.copy()
    if run_in_debug_mode:
        debugger_runner = DebuggerRunner(pydevd_py_debugger_file)
        if target:
            cmdline = (
                debugger_runner.get_command_line(
                    port, module=None, filename=target, multiprocess=multiprocess
                )
                + args
            )
        else:
            cmdline = (
                debugger_runner.get_command_line(
                    port, module=module, filename=None, multiprocess=multiprocess
                )
                + args
            )
    else:
        if target:
            if isinstance(target, str):
                target = [target]
            cmdline = [sys.executable, "-u"] + target + args
        else:
            cmdline = [sys.executable, "-u", "-m"] + [module] + args

    return cmdline, new_env


def _read_stream(stream, on_line, category):
    try:
        while True:
            output = stream.readline()
            if len(output) == 0:
                log.debug("Finished reading stream: %s.\n" % (category,))
                break
            output = output.decode("utf-8", errors="replace")
            on_line(output, category)
    except:
        log.exception("Error")


def _notify_on_exited_pid(on_exit, pid):
    from pydevd_dap_adapter.process_alive import is_process_alive

    try:
        import time

        log.debug("Waiting for pid to exit (_notify_on_exited_pid).")

        while True:
            if not is_process_alive(pid):
                break

            time.sleep(0.2)
        log.debug("pid exited (_notify_on_exited_pid).")
        on_exit()
    except:
        log.exception("Error")


class ProcessLaunch(_ProcessBase):
    def __init__(
        self,
        request: LaunchRequest,
        launch_response: LaunchResponse,
        debug_adapter_comm: DebugAdapterComm,
        host: str = "",
        port: int = 0,
    ) -> None:
        from .constants import TERMINAL_INTEGRATED
        from .constants import as_str
        from .constants import VALID_TERMINAL_OPTIONS
        from ._comm import create_server_socket

        _ProcessBase.__init__(self, request, debug_adapter_comm)

        # Only available when the launch is being used from an attach.
        self._host = host
        self._port = port

        self._cmdline: List[str] = []
        self._popen: Optional[subprocess.Popen] = None
        self._launch_response = launch_response

        # Initially mark as success. If something goes bad it should be marked as False.
        launch_response.success = True

        self._track_process_pid: Optional[int] = None
        self._launch_request: LaunchRequest = request

        target = request.arguments.kwargs.get("program")
        module = request.arguments.kwargs.get("module")
        pydevd_py_debugger_file = request.arguments.kwargs.get("pydevdPyDebuggerFile")

        self._cwd = request.arguments.kwargs.get("cwd", "")
        self._terminal = request.arguments.kwargs.get("console", TERMINAL_INTEGRATED)
        args = request.arguments.kwargs.get("args") or []
        args = [str(arg) for arg in args]

        env = {}
        request_env = request.arguments.kwargs.get("env")
        if isinstance(request_env, dict) and request_env:
            env.update(request_env)

        env = dict(((as_str(key), as_str(value)) for (key, value) in env.items()))

        self._env = env

        if self._terminal not in VALID_TERMINAL_OPTIONS:
            launch_response.success = False
            launch_response.message = f"Invalid console option: {self._terminal} (must be one of: {VALID_TERMINAL_OPTIONS})"
            return

        if not target and not module:
            launch_response.success = False
            launch_response.message = (
                "Neither 'program' nor 'module' were provided in the launch."
            )
            return

        if target and module:
            launch_response.success = False
            launch_response.message = (
                "Only one of 'program' and 'module' can be provided in the launch."
            )
            return

        if target:
            try:
                new_target = []

                if not isinstance(target, list):
                    target = [target]

                for t in target:
                    if not os.path.isabs(t):
                        if not self._cwd:
                            launch_response.success = False
                            launch_response.message = (
                                f"Target: {t} is relative and cwd was not given."
                            )
                            return

                        t = os.path.abspath(os.path.join(self._cwd, t))
                    else:
                        # This will also normalize
                        t = os.path.abspath(t)

                    if not os.path.exists(t):
                        launch_response.success = False
                        launch_response.message = f"File: {t} does not exist."
                        return

                    new_target.append(t)

                target = new_target
            except Exception as e:
                msg = f"Error checking if target ({target}) exists:\n{e}"
                log.exception(msg)
                launch_response.success = False
                launch_response.message = msg
                return

        try:
            if not self._cwd and target:
                if isinstance(target, list):
                    t = target[0]
                else:
                    t = target

                if os.path.isdir(t):
                    dirname = t
                else:
                    dirname = os.path.dirname(t)

                self._cwd = dirname

            if self._cwd:
                if not os.path.exists(self._cwd):
                    launch_response.success = False
                    launch_response.message = (
                        f"cwd specified does not exist: {self._cwd}"
                    )
                    return

                # make sure cwd is absolute
                self._cwd = os.path.abspath(self._cwd)
        except Exception as e:
            msg = f"Error checking if cwd ({self._cwd}) exists:\n{e}"
            log.exception(msg)
            launch_response.success = False
            launch_response.message = msg
            return

        if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
            log.debug("Run in debug mode: %s\n" % (self._run_in_debug_mode,))

        try:
            self.server_socket = server_socket = create_server_socket(
                host=host, port=port
            )
            port = server_socket.getsockname()[1]

            self._debug_adapter_pydevd_target_comm = (
                LaunchProcessDebugAdapterPydevdComm(debug_adapter_comm)
            )
        except Exception as e:
            msg = f"Error creating server socket to wait for connection:\n{e}"
            log.exception(msg)
            launch_response.success = False
            launch_response.message = msg
            return

        multiprocess = True

        try:
            cmdline_and_env = compute_cmd_line_and_env(
                pydevd_py_debugger_file,
                target,
                module,
                port,
                args,
                self._run_in_debug_mode,
                self._env,
                multiprocess,
            )

            self._cmdline, self._env = cmdline_and_env
        except Exception as e:
            msg = f"Error computing command line and environment:\n{e}"
            log.exception(msg)
            launch_response.success = False
            launch_response.message = msg
            return

    @property
    def valid(self) -> bool:
        return self._launch_response.success

    def launch(self) -> None:
        from .constants import TERMINAL_NONE
        from .constants import TERMINAL_INTEGRATED
        from .constants import TERMINAL_EXTERNAL
        from .debug_adapter_core.dap.pydevd_schema import (
            RunInTerminalRequest,
        )
        from .debug_adapter_core.dap.pydevd_schema import (
            RunInTerminalRequestArguments,
        )
        from pydevd_dap_adapter import run_and_save_pid

        launch_response = self._launch_response

        # Note: using a weak-reference so that callbacks don't keep it alive
        weak_debug_adapter_comm = self._weak_debug_adapter_comm
        debug_adapter_comm = weak_debug_adapter_comm()
        if debug_adapter_comm is None:
            try:
                raise RuntimeError(
                    "Error: did not expect DebugAdapterComm weak-ref to be collected at this point."
                )
            except Exception as e:
                msg = f"Error establishing connection: {e}."
                _dap_log.exception(msg)
                launch_response.success = False
                launch_response.message = msg
                return

        terminal = self._terminal
        if not debug_adapter_comm.supports_run_in_terminal:
            # If the client doesn't support running in the terminal we fallback to using the debug console.
            terminal = TERMINAL_NONE

        threads = []
        if terminal == TERMINAL_NONE:
            if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                log.debug(
                    "Launching in debug console (not in terminal): %s"
                    % (self._cmdline,)
                )

            env = os.environ.copy()
            env.update(self._env)
            popen = self._popen = subprocess.Popen(
                self._cmdline,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                stdin=subprocess.PIPE,
                cwd=self._cwd,
                env=env,
            )

            def on_output(output, category):
                from .debug_adapter_core.dap.pydevd_schema import (
                    OutputEvent,
                )
                from .debug_adapter_core.dap.pydevd_schema import (
                    OutputEventBody,
                )

                debug_adapter_comm = weak_debug_adapter_comm()
                if debug_adapter_comm is not None:
                    output_event = OutputEvent(
                        OutputEventBody(output, category=category)
                    )
                    debug_adapter_comm.write_to_client_message(output_event)

            stdout_stream_thread = threading.Thread(
                target=_read_stream,
                args=(self._popen.stdout, on_output, "stdout"),
                name="Read stdout",
            )
            stderr_stream_thread = threading.Thread(
                target=_read_stream,
                args=(self._popen.stderr, on_output, "stderr"),
                name="Read stderr",
            )

            threads.append(stdout_stream_thread)
            threads.append(stderr_stream_thread)

            self._track_process_pid = popen.pid

        elif terminal in (TERMINAL_INTEGRATED, TERMINAL_EXTERNAL):
            kind = terminal

            if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                log.debug('Launching in "%s" terminal: %s' % (kind, self._cmdline))

            debug_adapter_comm = weak_debug_adapter_comm()
            cmdline = self._cmdline
            if debug_adapter_comm is not None:
                env = self._env
                from pydevd_dap_adapter import run_with_env

                write_pid_to = tempfile.mktemp(".pid", "pydevd_")
                try:
                    cmdline, env = run_with_env.update_cmdline_and_env(
                        cmdline, env, write_pid_to=write_pid_to
                    )

                    debug_adapter_comm.write_to_client_message(
                        RunInTerminalRequest(
                            RunInTerminalRequestArguments(
                                cwd=self._cwd,
                                args=cmdline,
                                kind=(
                                    "integrated"
                                    if terminal == TERMINAL_INTEGRATED
                                    else "external"
                                ),
                                env=env,
                            )
                        )
                    )

                    self._track_process_pid = run_and_save_pid.wait_for_pid_in_file(
                        write_pid_to,
                        timeout=15,
                        timeout_error_message="runInTerminal requested, but the process did not start (waited for 15 seconds)",
                    )
                finally:
                    try:
                        os.remove(write_pid_to)
                    except:
                        # Ignore if it failed (it's possible that it wasn't created at all...).
                        log.debug("Error removing: %s", write_pid_to)

        if self._run_in_debug_mode:
            _dap_log.info("Starting thread to wait for pydevd connection.")

            try:
                server_socket = self.server_socket
                if server_socket is None:
                    raise RuntimeError(
                        "Expected server_socket to be setup already in launch."
                    )
                socket = wait_for_connection(server_socket=server_socket)
            except Exception as e:
                msg = f"Error establishing connection: {e}."
                _dap_log.exception(msg)
                launch_response.success = False
                launch_response.message = msg
                return

            try:
                self._debug_adapter_pydevd_target_comm.start_reader_and_writer_threads(
                    socket
                )
            except Exception as e:
                msg = f"Error starting reader and writer threads: {e}."
                _dap_log.exception(msg)
                launch_response.success = False
                launch_response.message = msg
                return

            try:
                self.after_connection_in_place()
            except Exception as e:
                msg = f"Error while initializing backend after communication was already in place: {e}."
                _dap_log.exception(msg)
                launch_response.success = False
                launch_response.message = msg
                return

        threads.append(
            threading.Thread(
                target=_notify_on_exited_pid,
                args=(self.notify_exit, self._track_process_pid),
                name="Track PID alive",
            )
        )

        # Note: only start listening stdout/stderr when connected.
        for t in threads:
            t.daemon = True
            t.start()

    def kill_processes_on_disconnect(
        self, disconnect_request: DisconnectRequest
    ) -> None:
        # This is just for the launch (in the attach when disconnecting it doesn't
        # kill subprocesses -- they may not even be in the same machine...).

        from .process_alive import kill_process_and_subprocesses
        from .constants import is_true_in_env

        is_terminated = self._debug_adapter_pydevd_target_comm.is_terminated()
        # i.e.: if the disconnect happens before the RF session sends a terminate
        # then we need to kill subprocesses (this means the user pressed the
        # stop button).
        if is_true_in_env("PYDEVD_KILL_ZOMBIE_PROCESSES") or not is_terminated:
            if self._popen is not None:
                if self._popen.returncode is None:
                    kill_process_and_subprocesses(self._popen.pid)
            else:
                kill_process_and_subprocesses(self._track_process_pid)

    def send_to_stdin(self, expression):
        popen = self._popen
        if popen is not None:
            try:
                log.debug("Sending: %s to stdin." % (expression,))

                def write_to_stdin(popen, expression):
                    popen.stdin.write(expression)
                    if not expression.endswith("\r") and not expression.endswith("\n"):
                        popen.stdin.write("\n")
                    popen.stdin.flush()

                # Do it in a thread (in theory the OS could have that filled up and we would never complete
                # trying to write -- although probably a bit far fetched, let's code as if that could actually happen).
                t = threading.Thread(
                    target=write_to_stdin,
                    args=(popen, expression),
                    name="Send to STDIN",
                )
                t.daemon = True
                t.start()
            except:
                log.exception("Error writing: >>%s<< to stdin." % (expression,))
