# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from __future__ import annotations

from functools import partial
import itertools
import sys
import threading
from typing import Optional, Dict
import typing
import weakref

from pydevd_dap_adapter import _dap_log
from pydevd_dap_adapter.constants import DebugInfoHolder
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_schema import (
    TerminatedEvent,
    Event,
    TerminatedEventBody,
    Request,
    Response,
)

from pydevd_dap_adapter.constants import DEFAULT_TIMEOUT
from pydevd_dap_adapter.debug_adapter_core.dap.pydevd_base_schema import BaseSchema
import os
import queue


if sys.version_info[:2] < (3, 8):

    class Protocol(object):
        pass

else:
    from typing import Protocol


log = _dap_log

if typing.TYPE_CHECKING:
    from pydevd_dap_adapter.debug_adapter_comm import DebugAdapterComm


class IProtocolMessageCallable(Protocol):
    def __call__(self, message: BaseSchema) -> None:
        pass


class INextSeq(Protocol):
    def __call__(self) -> int:
        pass


class BaseLaunchProcessTargetComm(threading.Thread):
    def __init__(self, debug_adapter_comm: DebugAdapterComm) -> None:
        threading.Thread.__init__(self)
        self._weak_debug_adapter_comm = weakref.ref(debug_adapter_comm)

        self._process_event_msg = None
        self._process_event = threading.Event()

        self._terminated_event_msg: Optional[TerminatedEvent] = None
        self._terminated_lock = threading.Lock()
        self._terminated_event = threading.Event()
        self._next_seq: INextSeq = partial(next, itertools.count(0))
        self._msg_id_to_on_response: Dict[int, Optional[IProtocolMessageCallable]] = {}
        self._write_to_pydevd_queue: "queue.Queue[BaseSchema]" = queue.Queue()

    def _from_pydevd(self, protocol_message: BaseSchema) -> None:
        self._handle_received_protocol_message_from_backend(protocol_message, "pydevd")

    def write_to_pydevd_message(
        self, protocol_message, on_response: Optional[IProtocolMessageCallable] = None
    ):
        """
        :param BaseSchema protocol_message:
            Some instance of one of the messages in the debug_adapter.schema.
        """
        seq = protocol_message.seq = self._next_seq()
        if on_response is not None:
            self._msg_id_to_on_response[seq] = on_response
        self._write_to_pydevd_queue.put(protocol_message)

    def _handle_received_protocol_message_from_backend(
        self, protocol_message: BaseSchema, backend: str
    ) -> None:
        import json
        from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
            READER_THREAD_STOPPED,
        )

        if protocol_message is READER_THREAD_STOPPED:
            if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                log.debug(
                    f"{self.__class__.__name__} when reading from {backend}: READER_THREAD_STOPPED."
                )
            return

        if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
            log.debug(
                "Process json (from pydevd): %s\n"
                % (json.dumps(protocol_message.to_dict(), indent=4, sort_keys=True),)
            )

        try:
            on_response: Optional[IProtocolMessageCallable] = None
            if protocol_message.type == "request":
                req = typing.cast(Request, protocol_message)
                method_name = f"on_{req.command}_request"

            elif protocol_message.type == "event":
                ev = typing.cast(Event, protocol_message)
                method_name = f"on_{ev.event}_event"

            elif protocol_message.type == "response":
                resp = typing.cast(Response, protocol_message)
                on_response = self._msg_id_to_on_response.pop(resp.request_seq, None)
                method_name = f"on_{resp.command}_response"

            else:
                if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                    log.debug(
                        "Unable to decide how to deal with protocol type: %s (read from %s - %s).\n"
                        % (protocol_message.type, backend, self.__class__.__name__)
                    )
                return

            if on_response is not None:
                on_response(protocol_message)

            on_request = getattr(self, method_name, None)

            if on_request is not None:
                on_request(protocol_message)
            elif on_response is not None:
                pass
            else:
                if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                    log.debug(
                        "Unhandled: %s not available when reading from %s - %s.\n"
                        % (method_name, backend, self.__class__.__name__)
                    )
        except:
            log.exception("Error")

    def on_process_event(self, event):
        self._process_event_msg = event
        self._process_event.set()

        debug_adapter_comm = self._weak_debug_adapter_comm()
        if debug_adapter_comm is not None:
            event.body.kwargs["dapProcessId"] = os.getpid()
            debug_adapter_comm.write_to_client_message(event)
        else:
            log.debug("Command processor collected in event: %s" % (event,))

    def get_pid(self):
        assert self._process_event.is_set()
        return self._process_event_msg.body.systemProcessId

    def wait_for_process_event(self):
        log.debug("Waiting for process event for %s seconds." % (DEFAULT_TIMEOUT,))
        ret = self._process_event.wait(DEFAULT_TIMEOUT)
        log.debug("Received process event: %s" % (ret,))
        return ret

    def is_terminated(self):
        with self._terminated_lock:
            return self._terminated_event.is_set()

    def on_terminated_event(self, event: Optional[TerminatedEvent]) -> None:
        with self._terminated_lock:
            if self._terminated_event.is_set():
                return

            if event is None:
                restart = False
                event = TerminatedEvent(body=TerminatedEventBody(restart=restart))

            self._terminated_event_msg = event
            self._terminated_event.set()

        debug_adapter_comm = self._weak_debug_adapter_comm()
        if debug_adapter_comm is not None:
            debug_adapter_comm.write_to_client_message(event)
        else:
            log.debug("Command processor collected in event: %s" % (event,))

    def notify_exit(self) -> None:
        self.on_terminated_event(None)
        log.debug("Target process finished (forcibly exiting debug adapter in 100ms).")

        # If the target process is terminated, wait a bit and exit ourselves.
        import time

        time.sleep(0.1)
        os._exit(0)

    def noop(self, *arg, **kwargs):
        pass

    on_setDebuggerProperty_response = noop
    on_initialize_response = noop
    on_initialized_event = noop
    on_launch_response = noop
    on_attach_response = noop

    def _forward_event_to_client(self, event: Event) -> None:
        debug_adapter_comm = self._weak_debug_adapter_comm()
        if debug_adapter_comm is not None:
            debug_adapter_comm.write_to_client_message(event)
        else:
            log.debug("Command processor collected in event: %s" % (event,))

    on_output_event = _forward_event_to_client
    on_thread_event = _forward_event_to_client
    on_stopped_event = _forward_event_to_client
    on_continued_event = _forward_event_to_client
