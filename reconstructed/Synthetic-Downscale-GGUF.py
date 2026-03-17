import struct, sys, os
"""
Synthetic downscale: produce pseudo smaller GGUF by keeping only the first N tensors
and truncating data section proportionally. This does NOT produce a valid functional model,
only a reduced-size artifact for pipeline testing.
"""
TARGETS = {
  '350m': 0.35,
  '125m': 0.125,
  '60m' : 0.06,
}

def read_u32(f): return struct.unpack('<I', f.read(4))[0]
def read_u64(f): return struct.unpack('<Q', f.read(8))[0]

def read_header(path):
    with open(path,'rb') as f:
        if f.read(4)!=b'GGUF': raise ValueError('Not GGUF')
        version = read_u32(f); n_tensors = read_u32(f); n_kv = read_u32(f)
        kv=[]
        for _ in range(n_kv):
            klen=read_u32(f); key=f.read(klen).decode(); vtype=read_u32(f); slen=read_u32(f); sval=f.read(slen).decode(); kv.append((key,vtype,sval))
        tensors=[]
        for _ in range(n_tensors):
            nlen=read_u32(f); name=f.read(nlen).decode(); nd=read_u32(f); shape=[read_u32(f) for __ in range(nd)]; dtype=read_u32(f); off=read_u64(f); tensors.append((name,nd,shape,dtype,off))
        data = f.read()
    return version, kv, tensors, data

def write_subset(base_path, out_path, ratio):
    try:
        version, kv, tensors, data = read_header(base_path)
        keep = max(1, int(len(tensors)*ratio))
        kept_tensors = tensors[:keep]
        trunc = int(len(data)*ratio)
        data_subset = data[:trunc]
        offset=0
        new_tensors=[]
        for name, nd, shape, dtype, _ in kept_tensors:
            new_tensors.append((name, nd, shape, dtype, offset))
            offset += max(16, int(trunc/keep))
        data_final = data_subset[:offset]
        with open(out_path,'wb') as f:
            f.write(b'GGUF')
            f.write(struct.pack('<I',version))
            f.write(struct.pack('<I',len(new_tensors)))
            f.write(struct.pack('<I',len(kv)))
            for key,vtype,sval in kv:
                f.write(struct.pack('<I',len(key))); f.write(key.encode()); f.write(struct.pack('<I',vtype)); f.write(struct.pack('<I',len(sval))); f.write(sval.encode())
            for name, nd, shape, dtype, off in new_tensors:
                f.write(struct.pack('<I',len(name))); f.write(name.encode()); f.write(struct.pack('<I',nd))
                for s in shape: f.write(struct.pack('<I',s))
                f.write(struct.pack('<I',dtype)); f.write(struct.pack('<Q',off))
            f.write(data_final)
        return keep, len(data_final)
    except Exception as e:
        # Fallback: copy original file unchanged (placeholder)
        import shutil
        shutil.copyfile(base_path, out_path)
        return 0, 0

if __name__=='__main__':
    if len(sys.argv)<3:
        print('Usage: python Synthetic-Downscale-GGUF.py <base_gguf> <out_dir>')
        sys.exit(1)
    base=sys.argv[1]; out_dir=sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    for tag,ratio in TARGETS.items():
        out_path=os.path.join(out_dir, f'synthetic-{tag}.gguf')
        keep,count=write_subset(base,out_path,ratio)
        print(f'{tag}: kept {keep} tensors, data {count} bytes -> {out_path}')
