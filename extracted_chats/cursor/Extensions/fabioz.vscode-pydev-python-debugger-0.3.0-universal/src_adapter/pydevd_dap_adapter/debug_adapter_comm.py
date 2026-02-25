# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from __future__ import annotations

import json
from typing import Optional, Union, Dict
import socket as socket_module

from pydevd_dap_adapter import _dap_log
from pydevd_dap_adapter.constants import DebugInfoHolder
from pydevd_dap_adapter.debug_adapter_core.dap import pydevd_base_schema as base_schema
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_base_schema import (
    BaseSchema,
    build_response,
)
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
    Breakpoint,
    ConfigurationDoneRequest,
    ConfigurationDoneResponse,
    ContinueRequest,
    DisconnectRequest,
    EvaluateRequest,
    InitializeRequest,
    InitializedEvent,
    LaunchRequest,
    LaunchResponse,
    NextRequest,
    PauseRequest,
    ScopesRequest,
    SetBreakpointsRequest,
    SetBreakpointsResponseBody,
    SetExceptionBreakpointsRequest,
    SourceBreakpoint,
    StackTraceRequest,
    StepInRequest,
    StepOutRequest,
    ThreadsRequest,
    VariablesRequest,
    SetFunctionBreakpointsRequest,
    TerminateRequest,
    AttachRequest,
    AttachResponse,
    StartDebuggingRequest,
    StartDebuggingRequestArguments,
    SetDebuggerPropertyRequest,
    SetDebuggerPropertyArguments,
    StepInTargetsRequest,
    GotoTargetsRequest,
    GotoRequest,
    CompletionsRequest,
)
import threading

log = _dap_log


class DebugAdapterComm(object):
    """
    This is the class that actually processes commands from the client (VSCode)
    using the process stdin/stdout.

    It's responsible for actually starting the debugger process later on.

    It's created in the main thread and then control is passed on to the reader thread so that whenever
    something is read the json is handled by this processor.

    The queue it receives in the constructor should be used to talk to the writer thread, where it's expected
    to post protocol messages (which will be converted with 'to_dict()' and will have the 'seq' updated as
    needed).
    """

    def __init__(self, write_to_client_queue) -> None:
        from ._process_attach import ProcessAttach
        from ._process_launch import ProcessLaunch

        self.write_to_client_queue = write_to_client_queue
        self._launch_or_attach_process: Optional[
            Union[ProcessLaunch, ProcessAttach]
        ] = None
        self._supports_run_in_terminal = False
        self._initialize_request_arguments = None
        self._run_in_debug_mode = True
        self._launch_or_attach_request: Optional[
            Union[LaunchRequest, AttachRequest]
        ] = None

    @property
    def supports_run_in_terminal(self):
        return self._supports_run_in_terminal

    @property
    def initialize_request_arguments(self):
        return self._initialize_request_arguments

    def from_client(self, protocol_message):
        from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
            READER_THREAD_STOPPED,
        )

        if protocol_message is READER_THREAD_STOPPED:
            if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                log.debug("%s: READER_THREAD_STOPPED." % (self.__class__.__name__,))
            return

        if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
            log.debug(
                "Process json (from client): %s\n"
                % (json.dumps(protocol_message.to_dict(), indent=4, sort_keys=True),)
            )

        if protocol_message.type == "request":
            method_name = "on_%s_request" % (protocol_message.command,)
            on_request = getattr(self, method_name, None)
            if on_request is not None:
                on_request(protocol_message)
            else:
                if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                    log.debug(
                        "Unhandled: %s not available in %s.\n"
                        % (method_name, self.__class__.__name__)
                    )

    def on_initialize_request(self, request: InitializeRequest):
        from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
            InitializeResponse,
        )
        from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import Capabilities

        self._initialize_request_arguments = request.arguments
        initialize_response: InitializeResponse = build_response(request)
        self._supports_run_in_terminal = request.arguments.supportsRunInTerminalRequest
        capabilities: Capabilities = initialize_response.body
        capabilities.supportTerminateDebuggee = True
        capabilities.supportsClipboardContext = True
        capabilities.supportsCompletionsRequest = True
        capabilities.supportsConditionalBreakpoints = True
        capabilities.supportsConfigurationDoneRequest = True
        capabilities.supportsDataBreakpoints = False
        # capabilities.supportsDebuggerProperties = True
        capabilities.supportsDelayedStackTraceLoading = True
        capabilities.supportsDisassembleRequest = False
        capabilities.supportsEvaluateForHovers = True
        capabilities.supportsExceptionInfoRequest = True
        capabilities.supportsExceptionOptions = True
        capabilities.supportsFunctionBreakpoints = True
        capabilities.supportsGotoTargetsRequest = True
        capabilities.supportsHitConditionalBreakpoints = True
        capabilities.supportsLoadedSourcesRequest = False
        capabilities.supportsLogPoints = True
        capabilities.supportsModulesRequest = True
        capabilities.supportsReadMemoryRequest = False
        capabilities.supportsRestartFrame = False
        capabilities.supportsRestartRequest = False
        capabilities.supportsSetExpression = True
        capabilities.supportsSetVariable = True
        capabilities.supportsStepBack = False
        capabilities.supportsStepInTargetsRequest = True
        capabilities.supportsTerminateRequest = True
        capabilities.supportsTerminateThreadsRequest = False
        capabilities.supportsValueFormattingOptions = True
        capabilities.supportsLogPoints = True
        capabilities.exceptionBreakpointFilters = [
            {"default": False, "filter": "raised", "label": "Raised Exceptions"},
            {"default": True, "filter": "uncaught", "label": "Uncaught Exceptions"},
            {
                "default": False,
                "filter": "userUnhandled",
                "label": "User Uncaught Exceptions",
            },
        ]
        self.write_to_client_message(initialize_response)

    def on_attach_request(self, request: AttachRequest):
        """
        A request to attach to a running process was just issued.

        The debugger must be hearing at the given host/port for this to work.
        """
        from ._process_attach import ProcessAttach

        self._launch_or_attach_request = request

        # An attach is always in debug mode.
        self._run_in_debug_mode = True
        attach_response: AttachResponse = build_response(request)
        attach_response.success = True  # May become false...

        try:
            attach_process = self._launch_or_attach_process = ProcessAttach(
                request, attach_response, self
            )
            if not attach_response.success:
                self.write_to_client_message(attach_response)  # acknowledge it
                return

            # Ok, configuration seems ok, let's make the actual attach.
            attach_process.attach()
            if not attach_response.success:
                self.write_to_client_message(attach_response)  # acknowledge it
                return

            # Only write the initialized event after the process has been
            # launched so that we can forward breakpoints directly to the
            # target.
            self.write_to_client_message(InitializedEvent())
            self._write_set_debugger_property_request()
            self._start_tracking_subprocess_connections()
            self.write_to_client_message(attach_response)
        except Exception as e:
            log.exception("Error launching.")
            attach_response.success = False
            attach_response.message = str(e)
            self.write_to_client_message(attach_response)  # acknowledge it
            return

    def _start_tracking_subprocess_connections(self) -> None:
        if not self._launch_or_attach_process:
            raise RuntimeError(
                "Error: self._launch_or_attach_process not assigned in %s", self
            )

        server_socket = self._launch_or_attach_process.server_socket
        if not server_socket:
            log.info(
                "server_socket not available to track subprocess connections (this "
                "is expected when it is an attach in 'client' mode)."
            )
            return

        def _handle_new_subprocess(socket_with_pydevd) -> None:
            from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
                forward_data,
            )

            try:
                from pydevd_dap_adapter._comm import create_server_socket
                from pydevd_dap_adapter._debugger_runner import get_localhost

                wait_for_client_connection_in = create_server_socket("", 0)
                _, port = wait_for_client_connection_in.getsockname()
                host = get_localhost()
                log.info(
                    "Will wait for client to handle subprocess to attach to: %s:%s",
                    host,
                    port,
                )

                configuration: Dict[str, Union[str, int, dict, list]] = dict(
                    mode="client",
                    host=host,
                    port=port,
                    isSubprocessAttach=True,
                )
                if self._launch_or_attach_request:
                    if self._launch_or_attach_request.arguments.kwargs.get(
                        "pythonExecutable"
                    ):
                        configuration[
                            "pythonExecutable"
                        ] = self._launch_or_attach_request.arguments.kwargs.get(
                            "pythonExecutable"
                        )

                    env = self._launch_or_attach_request.arguments.kwargs.get("env")
                    if env:
                        configuration["env"] = env

                self.write_to_client_message(
                    StartDebuggingRequest(
                        StartDebuggingRequestArguments(
                            configuration,
                            "attach",
                        )
                    )
                )

                wait_for_client_connection_in.listen(1)
                client_connected_at, _addr = wait_for_client_connection_in.accept()
                assert isinstance(client_connected_at, socket_module.socket)
                assert isinstance(socket_with_pydevd, socket_module.socket)

                log.info(
                    "Client to handle subprocess now attached %s", client_connected_at
                )
                log.info("Making socket link")

                forward_thread1 = threading.Thread(
                    target=forward_data,
                    args=(
                        socket_with_pydevd,
                        client_connected_at,
                        b"Forward data pydevd  > CLIENT",
                    ),
                    daemon=True,
                    name="Forward data pydevd  > CLIENT",
                )
                forward_thread2 = threading.Thread(
                    target=forward_data,
                    args=(
                        client_connected_at,
                        socket_with_pydevd,
                        b"Forward data client >  pydevd",
                    ),
                    daemon=True,
                    name="Forward data client >  pydevd",
                )
                forward_thread1.start()
                forward_thread2.start()

            except Exception:
                log.exception("Error handling new subprocess")
                raise

        def _wait_for_subprocesses() -> None:
            while True:
                try:
                    server_socket.listen(1)
                    socket_with_pydevd, _addr = server_socket.accept()
                    # A connection was received
                    log.info(
                        "Accepted new subprocess connection from pydevd: %s",
                        socket_with_pydevd,
                    )

                    threading.Thread(
                        target=_handle_new_subprocess,
                        name="Handle subprocess connection",
                        daemon=True,
                        args=(socket_with_pydevd,),
                    ).start()
                except Exception:
                    log.exception("Error accepting subprocess connections")

        threading.Thread(
            target=_wait_for_subprocesses,
            name="Wait for subprocess connections",
            daemon=True,
        ).start()

    def on_launch_request(self, request: LaunchRequest):
        from ._process_launch import ProcessLaunch

        try:
            self._launch_or_attach_request = request
            self._run_in_debug_mode = run_in_debug_mode = not request.arguments.noDebug

            launch_response: LaunchResponse = build_response(request)
            launch_process: Optional[ProcessLaunch] = None

            self._launch_or_attach_process = launch_process = ProcessLaunch(
                request, launch_response, self
            )

            if not launch_process.valid:
                self.write_to_client_message(launch_response)
                return

            # If on debug mode the launch is only considered finished when the
            # connection from the other side finishes properly.
            launch_process.launch()

            if not launch_process.valid:
                self.write_to_client_message(launch_response)
                return

            # Only write the initialized event after the process has been
            # launched so that we can forward breakpoints directly to the
            # target.
            self.write_to_client_message(InitializedEvent())
            if run_in_debug_mode and launch_process and launch_process.valid:
                self._write_set_debugger_property_request()
                self._start_tracking_subprocess_connections()

            self.write_to_client_message(launch_response)
        except Exception as e:
            log.exception("Error launching.")
            launch_response.success = False
            launch_response.message = str(e)
            self.write_to_client_message(launch_response)

    def _write_set_debugger_property_request(self):
        # We must configure pydevd
        self._launch_or_attach_process.write_to_pydevd(
            SetDebuggerPropertyRequest(
                SetDebuggerPropertyArguments(
                    skipSuspendOnBreakpointException=("BaseException",),
                    skipPrintBreakpointException=("NameError",),
                    # multiThreadsSingleNotification=False, -- this is now passed in the launch request
                )
            )
        )

    def on_configurationDone_request(self, request: ConfigurationDoneRequest):
        configuration_done_response: ConfigurationDoneResponse = build_response(request)
        launch_process = self._launch_or_attach_process
        if launch_process is None:
            configuration_done_response.success = False
            configuration_done_response.message = (
                "Launch is not done (configurationDone uncomplete)."
            )
            self.write_to_client_message(configuration_done_response)
            return

        if launch_process.send_and_wait_for_configuration_done_request():
            self.write_to_client_message(configuration_done_response)  # acknowledge it

        else:
            # timed out
            configuration_done_response.success = False
            configuration_done_response.message = (
                "Timed out waiting for configurationDone event."
            )
            self.write_to_client_message(configuration_done_response)

    def on_disconnect_request(self, request: DisconnectRequest):
        from ._process_launch import ProcessLaunch

        disconnect_response = base_schema.build_response(request)

        if self._launch_or_attach_process is not None:
            if isinstance(self._launch_or_attach_process, ProcessLaunch):
                self._launch_or_attach_process.kill_processes_on_disconnect(request)

        self.write_to_client_message(disconnect_response)

    def on_pause_request(self, request: PauseRequest):
        if self._run_in_debug_mode and self._launch_or_attach_process is not None:
            self._launch_or_attach_process.resend_request_to_pydevd(request)
        else:
            pause_response = base_schema.build_response(request)
            self.write_to_client_message(pause_response)

    def on_evaluate_request(self, request: EvaluateRequest):
        if self._launch_or_attach_process is not None:
            if request.arguments.context == "repl":
                pass
                # i.e.: if not stopped anywhere we could send to the stdin...
                # self._launch_or_attach_process.send_to_stdin(request.arguments.expression)
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_setExceptionBreakpoints_request(
        self, request: SetExceptionBreakpointsRequest
    ):
        if self._run_in_debug_mode and self._launch_or_attach_process is not None:
            self._launch_or_attach_process.resend_request_to_pydevd(request)
        else:
            response = base_schema.build_response(request)
            self.write_to_client_message(response)

    def on_setBreakpoints_request(self, request: SetBreakpointsRequest):
        if self._launch_or_attach_process is None or not self._run_in_debug_mode:
            # Just acknowledge that no breakpoints are valid.
            breakpoints = []
            if request.arguments.breakpoints:
                for bp in request.arguments.breakpoints:
                    source_breakpoint = SourceBreakpoint(**bp)
                    breakpoints.append(
                        Breakpoint(
                            verified=False,
                            line=source_breakpoint.line,
                            source=request.arguments.source,
                        ).to_dict()
                    )

            self.write_to_client_message(
                base_schema.build_response(
                    request,
                    kwargs=dict(
                        body=SetBreakpointsResponseBody(breakpoints=breakpoints)
                    ),
                )
            )
            return

        self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_continue_request(self, request: ContinueRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_stepIn_request(self, request: StepInRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_stepOut_request(self, request: StepOutRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_stackTrace_request(self, request: StackTraceRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_next_request(self, request: NextRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_scopes_request(self, request: ScopesRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_variables_request(self, request: VariablesRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_setFunctionBreakpoints_request(self, request: SetFunctionBreakpointsRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_terminate_request(self, request: TerminateRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_stepInTargets_request(self, request: StepInTargetsRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_gotoTargets_request(self, request: GotoTargetsRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_goto_request(self, request: GotoRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_completions_request(self, request: CompletionsRequest):
        if self._launch_or_attach_process:
            self._launch_or_attach_process.resend_request_to_pydevd(request)

    def on_threads_request(self, request: ThreadsRequest):
        from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
            ThreadsResponse,
        )

        if self._launch_or_attach_process:
            if self._run_in_debug_mode:
                self._launch_or_attach_process.resend_request_to_pydevd(request)
                return

        response: ThreadsResponse = build_response(request)
        return response

    def write_to_client_message(self, protocol_message: BaseSchema):
        """
        :param protocol_message:
            Some instance of one of the messages in the debug_adapter.schema.
        """
        self.write_to_client_queue.put(protocol_message)
