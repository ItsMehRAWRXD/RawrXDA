# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
from __future__ import annotations

import threading
import typing

from pydevd_dap_adapter import _dap_log
from pydevd_dap_adapter.base_launch_process_target import (
    BaseLaunchProcessTargetComm,
)
import socket as socket_module


if typing.TYPE_CHECKING:
    from pydevd_dap_adapter.debug_adapter_comm import DebugAdapterComm

log = _dap_log


class LaunchProcessDebugAdapterPydevdComm(BaseLaunchProcessTargetComm):
    def __init__(self, debug_adapter_comm: DebugAdapterComm):
        BaseLaunchProcessTargetComm.__init__(self, debug_adapter_comm)

    def start_reader_and_writer_threads(self, socket: socket_module.socket):
        from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
            writer_thread_no_auto_seq,
        )
        from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
            reader_thread,
        )

        debug_adapter_comm = self._weak_debug_adapter_comm()
        if debug_adapter_comm is None:
            raise RuntimeError(
                f"Error: _weak_debug_adapter_comm() already collected in start_reader_and_writer_threads ({self})"
            )

        read_from = socket.makefile("rb")
        write_to = socket.makefile("wb")

        writer = self._writer_thread = threading.Thread(
            target=writer_thread_no_auto_seq,
            args=(write_to, self._write_to_pydevd_queue, "write to pydevd process"),
            name="Write to pydevd (LaunchProcessDebugAdapterPydevdComm)",
        )
        writer.daemon = True

        reader = self._reader_thread = threading.Thread(
            target=reader_thread,
            args=(
                read_from,
                self._from_pydevd,
                debug_adapter_comm.write_to_client_queue,  # Used for errors
                b"read from pydevd process",
            ),
            name="Read from pydevd (LaunchProcessDebugAdapterPydevdComm)",
        )
        reader.daemon = True

        reader.start()
        writer.start()
