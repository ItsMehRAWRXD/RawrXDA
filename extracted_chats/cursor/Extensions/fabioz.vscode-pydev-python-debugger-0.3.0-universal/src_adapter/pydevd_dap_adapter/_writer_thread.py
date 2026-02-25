# Copyright Fabio Zadrozny
# License: Proprietary, requires a PyDev Debugger for VSCode license to be used.
# May be modified for your own internal use but any modifications may not be distributed.
import threading


class AbstractWriterThread(threading.Thread):
    def __init__(self):
        """ """
        threading.Thread.__init__(self)
        self.process = None  # Set after the process is created.
        self.daemon = True
        self.finished_ok = False
        self.finished_initialization = False
        self._next_breakpoint_id = 0

    def run(self):
        self.start_socket()

    def get_environ(self):
        return None

    def get_cwd(self):
        return None
