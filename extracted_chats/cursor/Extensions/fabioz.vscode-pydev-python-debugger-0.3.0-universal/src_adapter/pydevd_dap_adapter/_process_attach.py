# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from __future__ import annotations

import typing

from pydevd_dap_adapter import _dap_log, server_attach_launch
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
    AttachRequest,
    AttachResponse,
    LaunchRequest,
    LaunchRequestArguments,
    LaunchResponse,
)
from pydevd_dap_adapter._debugger_runner import get_localhost, get_pydevd_file
from pydevd_dap_adapter.launch_process_pydevd_comm import (
    LaunchProcessDebugAdapterPydevdComm,
)
from ._process_base import _ProcessBase
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_base_schema import build_response
import os
from typing import Optional
from pydevd_dap_adapter.constants import TERMINAL_NONE

if typing.TYPE_CHECKING:
    from pydevd_dap_adapter.debug_adapter_comm import DebugAdapterComm

log = _dap_log


class ProcessAttach(_ProcessBase):
    def __init__(
        self,
        request: AttachRequest,
        attach_response: AttachResponse,
        debug_adapter_comm: DebugAdapterComm,
    ) -> None:
        from pydevd_dap_adapter._process_launch import ProcessLaunch

        _ProcessBase.__init__(self, request, debug_adapter_comm)

        host = request.arguments.kwargs.get("host")
        port = request.arguments.kwargs.get("port")
        mode = request.arguments.kwargs.get("mode")
        pydevd_py_debugger_file = request.arguments.kwargs.get("pydevdPyDebuggerFile")

        self._attach_response = attach_response
        self._attach_request = request

        if not host:
            host = get_localhost()

        if not port:
            # The port is mandatory!
            msg = (
                "Unable to make attach (no port was given in request). Config received: %s"
                % (request.to_dict(),)
            )
            attach_response.success = False
            attach_response.message = msg
            return

        if not mode:
            msg = (
                'Unable to make attach. Mode: ("client" or "server) was not given. Config received: %s'
                % (request.to_dict(),)
            )
            attach_response.success = False
            attach_response.message = msg
            return

        if mode not in ("client", "server"):
            msg = (
                'Unable to make attach. Invalid "mode": %s (must be either "client" or "server)'
                % (mode,)
            )
            attach_response.success = False
            attach_response.message = msg
            return

        try:
            port = int(port)
        except ValueError:
            msg = (
                "Unable to make attach (the port '%s' is not a valid int). Config received: %s"
                % (port, request.to_dict())
            )
            attach_response.success = False
            attach_response.message = msg
            return

        self._host = host
        self._port = port
        self._mode = mode

        self._launch_request: Optional[LaunchRequest] = None
        self._launch_response: Optional[LaunchResponse] = None
        self._process_launch: Optional[ProcessLaunch] = None

        if mode == "server":
            # In this mode we create a dummy launch internally and then
            # wait for connections from the user.
            f = os.path.abspath(server_attach_launch.__file__)
            if f.endswith(".pyc"):
                f = f[:-1]

            pydevd_file = (
                pydevd_py_debugger_file
                if pydevd_py_debugger_file
                else get_pydevd_file()
            )
            add_to_syspath = os.path.dirname(pydevd_file)

            self._launch_request = launch_request = LaunchRequest(
                LaunchRequestArguments(
                    program=f,
                    noDebug=False,
                    console=TERMINAL_NONE,
                    args=[
                        f"""Waiting for connections in {host}:{port}
The code below may be used to connect:

import sys
sys.path.append({add_to_syspath!r})

import pydevd
pydevd.settrace(host={host!r}, port={port!r}, protocol='dap')
"""
                    ],
                )
            )
            self._launch_response = launch_response = build_response(request)

            launch_response.success = True
            self._process_launch = ProcessLaunch(
                launch_request,
                launch_response,
                debug_adapter_comm,
                host=host,
                port=port,
            )
            self._update_attach_response_from_launch_response()

    def _update_attach_response_from_launch_response(self) -> bool:
        if self._launch_response:
            if not self._launch_response.success:
                self._attach_response.success = False
                self._attach_response.message = self._launch_response.message = (
                    self._launch_response.message
                )

        return self._attach_response.success

    def attach(self) -> None:
        from pydevd_dap_adapter._comm import start_client

        mode = self._mode
        host = self._host
        port = self._port

        debug_adapter_comm = self._weak_debug_adapter_comm()
        if debug_adapter_comm is None:
            log.info("_weak_debug_adapter_comm already collected in: %s", self)
            return

        attach_response = self._attach_response
        if mode == "server":
            # In this mode a server socket will be created and it'll wait
            # for connections from pydevd in the given port.
            # We should fake an initial connection (which is the base
            # server itself) and then wait for incoming connections.
            # We have to be "careful" not to close the base server connection as
            # it needs to be persistent to accept more connections.
            try:
                assert (
                    self._process_launch
                ), "self._process_launch should've been set in attach/server mode."
                self._process_launch.launch()
                self.server_socket = self._process_launch.server_socket
            except Exception as e:
                msg = (
                    "Error when creating spawning attach/server mode at host:%s, port:%s. Error: %s"
                    % (
                        host,
                        port,
                        e,
                    )
                )
                log.exception(e)
                attach_response.success = False
                attach_response.message = msg
                return

            self._update_attach_response_from_launch_response()
            if attach_response.success:
                self._debug_adapter_pydevd_target_comm = (
                    self._process_launch._debug_adapter_pydevd_target_comm
                )
        else:
            assert mode == "client"

            # Make a single connection
            try:
                use_socket = self.client_socket = start_client(host, port)

                self._debug_adapter_pydevd_target_comm = (
                    LaunchProcessDebugAdapterPydevdComm(debug_adapter_comm)
                )
            except Exception as e:
                msg = "Error when connecting to host:%s, port:%s. Error: %s" % (
                    host,
                    port,
                    e,
                )
                log.exception(e)
                attach_response.success = False
                attach_response.message = msg
                return

            try:
                self._debug_adapter_pydevd_target_comm.start_reader_and_writer_threads(
                    use_socket
                )
            except Exception as e:
                msg = f"Error starting reader and writer threads: {e}."
                _dap_log.exception(msg)
                attach_response.success = False
                attach_response.message = msg
                return

            try:
                self.after_connection_in_place()
            except Exception as e:
                msg = f"Error while initializing backend after communication was already in place: {e}."
                _dap_log.exception(msg)
                attach_response.success = False
                attach_response.message = msg
                return
