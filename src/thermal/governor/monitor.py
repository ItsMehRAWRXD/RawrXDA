import mmap
import struct
import time
import ctypes

class SovereignGovernorStatus(ctypes.Structure):
    _fields_ = [
        ("allowedMask", ctypes.c_uint32),
        ("throttledMask", ctypes.c_uint32),
        ("maxTempSeen", ctypes.c_int32),
        ("state", ctypes.c_uint32),
        ("lastUpdate", ctypes.c_uint64),
    ]

MMF_NAME = "Global\\SOVEREIGN_GOVERNOR_STATUS"
MMF_SIZE = ctypes.sizeof(SovereignGovernorStatus)

def monitor():
    print(f"Opening MMF: {MMF_NAME} (Size: {MMF_SIZE})")
    try:
        shm = mmap.mmap(0, MMF_SIZE, MMF_NAME, access=mmap.ACCESS_READ)
    except FileNotFoundError:
        print("MMF not found. Is Governor running?")
        return

    while True:
        shm.seek(0)
        buf = shm.read(MMF_SIZE)
        data = SovereignGovernorStatus.from_buffer_copy(buf)
        
        status_str = "NORMAL"
        if data.state == 1: status_str = "WARNING"
        if data.state == 2: status_str = "CRITICAL"
        
        delta = int((time.time() * 1000) - data.lastUpdate)
        
        print(f"\rStatus: {status_str} | MaxTemp: {data.maxTempSeen}C | Allowed: {data.allowedMask:05b} | Throttled: {data.throttledMask:05b} | Age: {delta}ms    ", end="")
        time.sleep(0.5)

if __name__ == "__main__":
    monitor()
