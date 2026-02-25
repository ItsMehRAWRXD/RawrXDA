# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from __future__ import annotations

from functools import partial
import itertools
import threading
from typing import Optional, Union
import typing
import weakref

from pydevd_dap_adapter import _dap_log
from pydevd_dap_adapter.base_launch_process_target import IProtocolMessageCallable
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_base_schema import BaseSchema
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
    AttachRequest,
    LaunchRequest,
    InitializeRequest,
    InitializeRequestArguments,
)
from pydevd_dap_adapter.launch_process_pydevd_comm import (
    LaunchProcessDebugAdapterPydevdComm,
)
import socket as socket_module


if typing.TYPE_CHECKING:
    from pydevd_dap_adapter.debug_adapter_comm import DebugAdapterComm

log = _dap_log


class _ProcessBase:
    _run_in_debug_mode: bool
    _debug_adapter_pydevd_target_comm: LaunchProcessDebugAdapterPydevdComm

    # Only set later on if everything is correct. If this was a launch or
    # an attach to in server mode, this is the server socket.
    server_socket: Optional[socket_module.socket]

    # Only set later on if everything is correct. If this was an attach
    # to with client mode, this is the client socket
    client_socket: Optional[socket_module.socket]

    def __init__(
        self,
        request: Union[LaunchRequest, AttachRequest],
        debug_adapter_comm: DebugAdapterComm,
    ) -> None:
        self._next_seq = partial(next, itertools.count(0))
        if isinstance(request, LaunchRequest):
            self._run_in_debug_mode = not request.arguments.noDebug
        else:
            self._run_in_debug_mode = True
        self._weak_debug_adapter_comm: "weakref.ref[DebugAdapterComm]" = weakref.ref(
            debug_adapter_comm
        )
        self.server_socket = None
        self._launch_or_attach_request = request

    def send_and_wait_for_configuration_done_request(self) -> bool:
        """
        :return: Whether the configuration done response was received.
        """
        from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
            ConfigurationDoneArguments,
        )
        from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
            ConfigurationDoneRequest,
        )
        from pydevd_dap_adapter.constants import DEFAULT_TIMEOUT

        track_events = []

        if self._run_in_debug_mode:
            event_pydevd = threading.Event()
            track_events.append(event_pydevd)
            self._debug_adapter_pydevd_target_comm.write_to_pydevd_message(
                ConfigurationDoneRequest(ConfigurationDoneArguments()),
                on_response=lambda *args, **kwargs: event_pydevd.set(),
            )

        log.debug(
            "Wating for configuration_done response for %s seconds."
            % (DEFAULT_TIMEOUT,)
        )
        ret = True
        for event in track_events:
            ret = ret and event.wait(DEFAULT_TIMEOUT)
            if not ret:
                break
        log.debug("Received configuration_done response: %s" % (ret,))
        return ret

    def after_connection_in_place(self) -> None:
        _dap_log.info("pydevd connected.")
        initialize_request = InitializeRequest(
            InitializeRequestArguments("pydevd-launch-process-adapter")
        )
        self.write_to_pydevd(initialize_request)

        # _pydevd_bundle.pydevd_json_debug_options.DebugOptions.update_from_args
        kwargs = self._launch_or_attach_request.arguments.kwargs
        if "justMyCode" not in kwargs and "debugStdLib" not in kwargs:
            # Change the default
            kwargs["justMyCode"] = False

        self.write_to_pydevd(self._launch_or_attach_request)

        if not self._debug_adapter_pydevd_target_comm.wait_for_process_event():
            raise RuntimeError("Debug adapter timed out waiting for process event.")

    def resend_request_to_pydevd(self, request: BaseSchema) -> None:
        request_seq = request.seq

        def on_response(response_msg):
            # Renumber and forward response to client.
            response_msg.request_seq = request_seq
            debug_adapter_comm = self._weak_debug_adapter_comm()
            if debug_adapter_comm is not None:
                debug_adapter_comm.write_to_client_message(response_msg)
            else:
                log.debug(
                    "Command processor collected in resend request: %s" % (request,)
                )

        self._debug_adapter_pydevd_target_comm.write_to_pydevd_message(
            request, on_response
        )

    def write_to_pydevd(
        self,
        request: BaseSchema,
        on_response: Optional[IProtocolMessageCallable] = None,
    ):
        if self._run_in_debug_mode:
            self._debug_adapter_pydevd_target_comm.write_to_pydevd_message(
                request, on_response=on_response
            )

    def notify_exit(self):
        self._debug_adapter_pydevd_target_comm.notify_exit()
