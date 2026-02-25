# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
"""
This module has the main entries for launching the debugger
(used in tests but may also be useful in other situations).
"""

import os
import sys

curdir = os.path.dirname(os.path.abspath(__file__))
src_adapter = os.path.dirname(curdir)
root = os.path.dirname(src_adapter)
_critical_error_log_file = os.path.join(root, "logs", "pydevd_dap_critical.log")


def start_reader_writer(to_client_queue, write_to=None, read_from=None):
    import threading
    from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
        writer_thread,
        reader_thread,
    )
    from pydevd_dap_adapter.debug_adapter_comm import DebugAdapterComm

    if write_to is None:
        write_to = sys.stdout.buffer

    if read_from is None:
        read_from = sys.stdin.buffer

    comm = DebugAdapterComm(to_client_queue)

    writer = threading.Thread(
        target=writer_thread,
        args=(write_to, to_client_queue, "write to client"),
        name="Write to client (dap __main__)",
    )
    reader = threading.Thread(
        target=reader_thread,
        args=(read_from, comm.from_client, to_client_queue, b"read from client"),
        name="Read from client (dap __main__)",
    )

    reader.start()
    writer.start()
    return reader, writer


def main():
    from pydevd_dap_adapter import _dap_log
    from pydevd_dap_adapter.debug_adapter_core.debug_adapter_threads import (
        STOP_WRITER_THREAD,
    )

    from queue import Queue

    _dap_log.debug("In main() of %s", __file__)

    try:
        to_client_queue = Queue()
        reader, writer = start_reader_writer(to_client_queue)

        _dap_log.debug("Reader and writer threads started.")
        reader.join()
        _dap_log.debug("Exited reader.\n")
        to_client_queue.put(STOP_WRITER_THREAD)
        writer.join()
        _dap_log.debug("Exited writer.\n")
    except Exception as e:
        _dap_log.exception("Exception received in main()")
        raise


if __name__ == "__main__":
    try:
        sys.path.append(src_adapter)
        main()
    except Exception as e:
        os.makedirs(os.path.dirname(_critical_error_log_file), exist_ok=True)
        import traceback

        with open(_critical_error_log_file, "a+") as stream:
            stream.write("----- Critical error with pydevd dap adapter:\n")
            traceback.print_exc(file=stream)
