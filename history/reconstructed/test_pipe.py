import time
import win32pipe, win32file, pywintypes

pipe_name = r'\\.\pipe\RawrXD_IPC'

def client():
    print(f"Connecting to {pipe_name}...")
    try:
        handle = win32file.CreateFile(
            pipe_name,
            win32file.GENERIC_READ | win32file.GENERIC_WRITE,
            0,
            None,
            win32file.OPEN_EXISTING,
            0,
            None
        )
    except pywintypes.error as e:
        print(f"Error connecting: {e}")
        return

    print("Connected. Sending prompt...")
    data = b"Hello Titan Engine!"
    win32file.WriteFile(handle, data)
    
    print("Waiting for response...")
    resp_code, resp_data = win32file.ReadFile(handle, 4096)
    print(f"Received: {resp_data.decode('utf-8')}")

if __name__ == '__main__':
    try:
        client()
    except Exception as e:
        print(e)
