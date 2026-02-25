# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
import os

from pydevd_dap_adapter import _dap_log
from pydevd_dap_adapter.constants import IS_JYTHON, IS_WINDOWS, IS_WASM, DEFAULT_TIMEOUT
import socket as socket_module


AF_INET, AF_INET6, SOCK_STREAM, SHUT_WR, SOL_SOCKET, IPPROTO_TCP, socket = (
    socket_module.AF_INET,
    socket_module.AF_INET6,
    socket_module.SOCK_STREAM,
    socket_module.SHUT_WR,
    socket_module.SOL_SOCKET,
    socket_module.IPPROTO_TCP,
    socket_module.socket,
)

if IS_WINDOWS and not IS_JYTHON:
    SO_EXCLUSIVEADDRUSE = socket_module.SO_EXCLUSIVEADDRUSE
if not IS_WASM:
    SO_REUSEADDR = socket_module.SO_REUSEADDR


def create_server_socket(host, port):
    try:
        server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
        if IS_WINDOWS and not IS_JYTHON:
            server.setsockopt(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1)
        elif not IS_WASM:
            server.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)

        server.bind((host, port))
        server.settimeout(None)
    except Exception:
        server.close()
        raise

    return server


def wait_for_connection(
    server_socket: socket_module.socket, timeout: int = DEFAULT_TIMEOUT
) -> socket_module.socket:
    from pydevd_dap_adapter.run_in_thread import run_in_thread

    def wait_for_it():
        server_socket.listen(1)
        new_socket, _addr = server_socket.accept()
        _dap_log.info("Connection accepted")
        return new_socket

    _dap_log.debug("Waiting for connection for %s seconds." % (timeout,))
    fut = run_in_thread(wait_for_it)
    return fut.result(timeout)


def start_client(host: str, port: int) -> socket_module.socket:
    """connects to a host/port"""
    _dap_log.info("Connecting to %s:%s", host, port)

    address_family = AF_INET
    for res in socket_module.getaddrinfo(host, port, 0, SOCK_STREAM):
        if res[0] == AF_INET:
            address_family = res[0]
            # Prefer IPv4 addresses for backward compat.
            break
        if res[0] == AF_INET6:
            # Don't break after this - if the socket is dual-stack prefer IPv4.
            address_family = res[0]

    s = socket(address_family, SOCK_STREAM)

    #  Set TCP keepalive on an open socket.
    #  It activates after 1 second (TCP_KEEPIDLE,) of idleness,
    #  then sends a keepalive ping once every 3 seconds (TCP_KEEPINTVL),
    #  and closes the connection after 5 failed ping (TCP_KEEPCNT), or 15 seconds
    try:
        s.setsockopt(SOL_SOCKET, socket_module.SO_KEEPALIVE, 1)
    except (AttributeError, OSError):
        pass  # May not be available everywhere.
    try:
        s.setsockopt(socket_module.IPPROTO_TCP, socket_module.TCP_KEEPIDLE, 1)
    except (AttributeError, OSError):
        pass  # May not be available everywhere.
    try:
        s.setsockopt(socket_module.IPPROTO_TCP, socket_module.TCP_KEEPINTVL, 3)
    except (AttributeError, OSError):
        pass  # May not be available everywhere.
    try:
        s.setsockopt(socket_module.IPPROTO_TCP, socket_module.TCP_KEEPCNT, 5)
    except (AttributeError, OSError):
        pass  # May not be available everywhere.

    try:
        # 10 seconds default timeout
        timeout = int(os.environ.get("PYDEVD_CONNECT_TIMEOUT", 10))
        s.settimeout(timeout)
        s.connect((host, port))
        s.settimeout(None)  # no timeout after connected
        _dap_log.info("Connected.")
        return s
    except:
        _dap_log.exception("Could not connect to %s: %s", host, port)
        raise
