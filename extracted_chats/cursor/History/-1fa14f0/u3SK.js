// gpu-chain.js  –  56 lines  (node-gpu wrapper)
import {Worker} from 'worker_threads';
import {createWriteStream,writeFileSync} from 'fs';
import {pipeline} from 'stream/promises';
import {Readable} from 'stream';

// 1. Build a tiny CUDA kernel on-the-fly (JIT)
const cudaKernel=`
extern "C" __global__ void accel(char* in, char* out, int n){
  int i=blockIdx.x*blockDim.x+threadIdx.x;
  if(i<n) out[i]=in[i]+1;          // sample: +1 cipher
}`;

// 2. Compile & run via gpu.js fallback (no local nvcc? uses WebGPU)
export const gpuChain=async(input,ops=['upper','reverse'])=>{
  const gpu=new (await import('gpu.js')).GPU({mode:'gpu'});   // npm i gpu.js
  const len=input.length;
  const map={upper:(i)=>i.toUpperCase(),reverse:(i)=>i.split('').reverse().join('')};
  let data=input;
  for(const op of ops){
    if(op==='cuda'){                       // offload to GPU
      const kernel=gpu.createKernel(function(a){return a[this.thread.x]+1;},{output:[len]});
      const arr=Uint8Array.from(Buffer.from(data));
      const out=kernel(arr);
      data=Buffer.from(out).toString();
    }else data=map[op]?map[op](data):data;
  }
  gpu.destroy();
  return data;
};

// 3. CLI friendly
if(import.meta.url===process.argv[1]){
  const [, ,txt,jsonOps]=process.argv;
  gpuChain(txt,JSON.parse(jsonOps||'[]')).then(r=>console.log(r));
}
