# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
"""
Contains the main loops for our threads (reader_thread and writer_thread, which should
be initialized as the target of threading.Thread).

The reader_thread will read a message and ask a callable to deal with it and the
writer_thread just keeps on sending any message posted to the queue it receives.
"""
# Note: sentinel. Signals that the writer thread should stop processing.
from functools import partial
import itertools
import json
from typing import Optional, Dict, Any
import socket as socket_module

from pydevd_dap_adapter import _dap_log
from pydevd_dap_adapter.constants import DebugInfoHolder


STOP_WRITER_THREAD = "STOP_WRITER_THREAD"

# Note: sentinel. Sent by reader thread when stopped.
READER_THREAD_STOPPED = "READER_THREAD_STOPPED"


def forward_data(
    source_socket: socket_module.socket,
    dest_socket: socket_module.socket,
    debug_prefix: bytes,
):
    while True:
        data = source_socket.recv(1024)
        if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
            _dap_log.debug(
                (
                    debug_prefix
                    + b": >>%s<<\n"
                    % (data.replace(b"\r", b"\\r").replace(b"\n", b"\\n"))
                ).decode("utf-8", "replace")
            )
        if not data:
            break

        dest_socket.sendall(data)


def read(stream, debug_prefix=b"read") -> Optional[Dict]:
    """
    Reads one message from the stream and returns the related dict (or None if EOF was reached).

    :param stream:
        The stream we should be reading from.

    :return dict|NoneType:
        The dict which represents a message or None if the stream was closed.
    """
    headers = {}
    while True:
        # Interpret the http protocol headers
        line = stream.readline()  # The trailing \r\n should be there.

        if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
            _dap_log.debug(
                (
                    debug_prefix
                    + b": >>%s<<\n"
                    % (line.replace(b"\r", b"\\r").replace(b"\n", b"\\n"))
                ).decode("utf-8", "replace")
            )

        if not line:  # EOF
            return None
        line = line.strip().decode("ascii")
        if not line:  # Read just a new line without any contents
            break
        try:
            name, value = line.split(": ", 1)
        except ValueError:
            raise RuntimeError("Invalid header line: {}.".format(line))
        headers[name.strip()] = value.strip()

    if not headers:
        raise RuntimeError("Got message without headers.")

    content_length = int(headers["Content-Length"])

    # Get the actual json
    body = _read_len(stream, content_length)
    if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
        _dap_log.debug((debug_prefix + b": %s" % (body,)).decode("utf-8", "replace"))

    try:
        return json.loads(body.decode("utf-8"))
    except:
        raise RuntimeError(f"Error reading: {body!r}")


def _read_len(stream, content_length) -> bytes:
    buf = b""
    if not content_length:
        return buf

    # Grab the body
    while True:
        data = stream.read(content_length - len(buf))
        if not buf and len(data) == content_length:
            # Common case
            return data
        buf += data
        if len(buf) == content_length:
            return buf
        if len(buf) > content_length:
            raise AssertionError(
                "Expected to read message up to len == %s (already read: %s). Found:\n%s"
                % (content_length, len(buf), buf.decode("utf-8", "replace"))
            )
        # len(buf) < content_length (just keep on going).


def reader_thread(
    stream,
    process_command,
    write_queue,
    debug_prefix=b"read",
    update_ids_from_dap=False,
):
    from .dap import pydevd_base_schema as dap_base_schema

    from .dap import (
        pydevd_schema,  # @UnusedImport -- register classes
    )
    from .dap.pydevd_schema import Response

    try:
        while True:
            data = read(stream, debug_prefix)
            if data is None:
                break
            try:
                # A response with success == False doesn't need to be translated
                # as the original response (to avoid the validation).
                if not data.get("success", True) and data.get("type") == "response":
                    protocol_message = dap_base_schema.from_dict(
                        data, update_ids_from_dap=update_ids_from_dap, cls=Response
                    )
                else:
                    protocol_message = dap_base_schema.from_dict(
                        data, update_ids_from_dap=update_ids_from_dap
                    )
                process_command(protocol_message)
            except Exception as e:
                _dap_log.exception("Error processing message.")
                seq = data.get("seq")
                if seq:
                    error_msg = {
                        "type": "response",
                        "request_seq": seq,
                        "success": False,
                        "command": data.get("command", "<unknown"),
                        "message": "Error processing message: %s" % (e,),
                    }
                    write_queue.put(error_msg)
    except ConnectionError:
        if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
            _dap_log.exception("ConnectionError (ignored).")
    except:
        _dap_log.exception("Error reading message.")
    finally:
        process_command(READER_THREAD_STOPPED)


def writer_thread_no_auto_seq(
    stream, queue, debug_prefix="write", update_ids_to_dap=False
):
    """
    Same as writer_thread but does not set the message 'seq' automatically
    (meant to be used when responses, which need the seq id set need to be handled).
    """
    try:
        while True:
            to_write = queue.get()
            if to_write is STOP_WRITER_THREAD:
                _dap_log.debug("STOP_WRITER_THREAD")
                stream.close()
                break

            if isinstance(to_write, dict):
                assert "seq" in to_write
                try:
                    to_write = json.dumps(to_write)
                except:
                    _dap_log.exception("Error serializing %s to json.", to_write)
                    continue

            else:
                to_json = getattr(to_write, "to_json", None)
                if to_json is not None:
                    # Some protocol message
                    assert to_write.seq >= 0
                    try:
                        to_write = to_json(update_ids_to_dap=update_ids_to_dap)
                    except:
                        _dap_log.exception("Error serializing %s to json.", to_write)
                        continue

            if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                _dap_log.debug(debug_prefix + ": %s\n", to_write)

            if to_write.__class__ == bytes:
                as_bytes = to_write
            else:
                as_bytes = to_write.encode("utf-8")

            stream.write(
                ("Content-Length: %s\r\n\r\n" % (len(as_bytes))).encode("ascii")
            )
            stream.write(as_bytes)
            stream.flush()
    except:
        _dap_log.exception("Error writing message.")
    finally:
        _dap_log.debug("Exit reader thread.")


def writer_thread(stream, queue, debug_prefix="write", update_ids_to_dap=False):
    """
    Same as writer_thread_no_auto_seq but sets the message 'seq' automatically.
    """
    _next_seq = partial(next, itertools.count())

    try:
        while True:
            to_write = queue.get()
            if to_write is STOP_WRITER_THREAD:
                _dap_log.debug("STOP_WRITER_THREAD")
                stream.close()
                break

            if isinstance(to_write, dict):
                to_write["seq"] = _next_seq()
                try:
                    to_write = json.dumps(to_write)
                except:
                    _dap_log.exception("Error serializing %s to json.", to_write)
                    continue

            else:
                to_json = getattr(to_write, "to_json", None)
                if to_json is not None:
                    # Some protocol message
                    to_write.seq = _next_seq()
                    try:
                        to_write = to_json(update_ids_to_dap=update_ids_to_dap)
                    except:
                        _dap_log.exception("Error serializing %s to json.", to_write)
                        continue

            if DebugInfoHolder.DEBUG_TRACE_LEVEL >= 2:
                _dap_log.debug(debug_prefix + ": %s\n", to_write)

            if to_write.__class__ == bytes:
                as_bytes = to_write
            else:
                as_bytes = to_write.encode("utf-8")

            stream.write(
                ("Content-Length: %s\r\n\r\n" % (len(as_bytes))).encode("ascii")
            )
            stream.write(as_bytes)
            stream.flush()
    except ConnectionResetError:
        pass  # No need to log this
    except:
        _dap_log.exception("Error writing message.")
    finally:
        _dap_log.debug("Exit reader thread.")
