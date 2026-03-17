import os, ctypes, sys, json
from fastapi import FastAPI, Request
import uvicorn

MODELS_ROOT = os.environ.get('UNLOCK_ROOT', r'D:\Franken\BackwardsUnlock')
KEY_PATH = os.environ.get('CAMELLIA_KEY', os.path.join(os.path.dirname(__file__), 'camellia256.key'))
DLL_PATH = os.path.join(os.path.dirname(__file__), 'CamelliaReal.dll')

if not os.path.exists(DLL_PATH):
    print('ERROR: Camellia DLL missing. Run Camellia256-Encrypt-Real.ps1 first.')
    sys.exit(1)

cam = ctypes.CDLL(DLL_PATH)
cam.Camellia256_Decrypt.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_ubyte)]
cam.Camellia256_Decrypt.restype = None

key = open(KEY_PATH,'rb').read()

app = FastAPI(title='Unlocked-Decryption-Server', version='1.0')

def decrypt_cam(path):
    raw = open(path,'rb').read()
    ctr = raw[:16]
    cipher = raw[16:]
    out = bytearray(len(cipher))
    block = bytearray(16)
    ks = bytearray(16)
    ctr_work = bytearray(ctr)
    for i in range(0,len(cipher),16):
        for j in range(16):
            block[j] = ctr_work[j]
        # Camellia block -> ks
        cam.Camellia256_Decrypt((ctypes.c_ubyte*16).from_buffer(block),(ctypes.c_ubyte*16).from_buffer(ks),(ctypes.c_ubyte*len(key)).from_buffer(bytearray(key)))
        for j in range(16):
            if i+j < len(cipher):
                out[i+j] = cipher[i+j] ^ ks[j]
        # increment ctr
        for c in range(15,-1,-1):
            ctr_work[c] = (ctr_work[c] + 1) & 0xFF
            if ctr_work[c] != 0: break
    return bytes(out)

@app.post('/v1/chat/completions')
async def chat(req: Request):
    body = await req.json()
    model = body.get('model','1b')
    model_dir = os.path.join(MODELS_ROOT, model)
    cam_file = None
    for fn in os.listdir(model_dir):
        if fn.endswith('.camellia'): cam_file = os.path.join(model_dir, fn); break
    if not cam_file:
        return {'error': f'Encrypted file not found for model {model}'}
    plain = decrypt_cam(cam_file)
    # Stub inference (not loading actual GGUF to run tokens)
    prompt = body.get('messages',[{'content':'','role':'user'}])[-1].get('content','')
    answer = f"[stub-response to: {prompt[:64]}...]"
    return { 'choices':[{'message':{'role':'assistant','content':answer}}] }

if __name__ == '__main__':
    uvicorn.run(app, host='0.0.0.0', port=11435)
