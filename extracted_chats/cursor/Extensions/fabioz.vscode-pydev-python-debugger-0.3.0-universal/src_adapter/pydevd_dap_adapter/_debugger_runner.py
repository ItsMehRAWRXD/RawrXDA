# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
import os
import socket
import sys
from typing import Optional, Union, List

_cache_localhost = None


def get_localhost():
    """
    Should return 127.0.0.1 in ipv4 and ::1 in ipv6

    localhost is not used because on windows vista/windows 7, there can be issues where the resolving doesn't work
    properly and takes a lot of time (had this issue on the pyunit server).

    Using the IP directly solves the problem.
    """
    # TODO: Needs better investigation!

    global _cache_localhost
    if _cache_localhost is None:
        try:
            for addr_info in socket.getaddrinfo("localhost", 80, 0, 0, socket.SOL_TCP):
                config = addr_info[4]
                if config[0] == "127.0.0.1":
                    _cache_localhost = "127.0.0.1"
                    return _cache_localhost
        except:
            # Ok, some versions of Python don't have getaddrinfo or SOL_TCP... Just consider it 127.0.0.1 in this case.
            _cache_localhost = "127.0.0.1"
        else:
            _cache_localhost = "localhost"

    return _cache_localhost


def get_socket_names(n_sockets, close=False):
    socket_names = []
    sockets = []
    for _ in range(n_sockets):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((get_localhost(), 0))
        socket_name = sock.getsockname()

        sockets.append(sock)
        socket_names.append(socket_name)

    if close:
        for s in sockets:
            s.close()
    return socket_names


def get_socket_name(close=False):
    return get_socket_names(1, close)[0]


def get_pydevd_file():
    from pathlib import Path

    this_dir = Path(__file__).absolute().parent
    pydevd = str(this_dir.parent.parent / "PyDev.Debugger" / "pydevd.py")
    assert os.path.exists(pydevd), "%s does not exist." % (pydevd,)
    return pydevd


class DebuggerRunner(object):
    def __init__(self, pydevd_py_debugger_file: Optional[str] = None):
        self._pydevd_py_debugger_file = pydevd_py_debugger_file

    def get_possibly_custom_pydevd_file(self):
        if self._pydevd_py_debugger_file:
            return self._pydevd_py_debugger_file
        return get_pydevd_file()

    def get_command_line(
        self,
        port: int,
        module: Optional[str],
        filename: Optional[Union[str, List[str]]],
        multiprocess: bool,
    ):
        localhost = get_localhost()
        ret = [self.get_possibly_custom_pydevd_file()]

        ret += [
            "--client",
            localhost,
            "--port",
            str(port),
        ]

        if multiprocess:
            ret.append("--multiprocess")

        ret += ["--debug-mode", "debugpy-dap"]
        ret += ["--json-dap-http"]

        if module:
            ret += ["--module", "--file", module]
        else:
            assert isinstance(filename, (list, str)), (
                "Expected list or str. Found: %s" % (type(filename),)
            )
            if not isinstance(filename, list):
                filename = [filename]
            ret += ["--file"] + filename

        return [sys.executable, "-u"] + ret
