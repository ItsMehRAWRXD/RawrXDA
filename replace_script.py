import re

with open(r"d:\rawrxd\src\tools\gpu_benchmark.cpp", "r", encoding="utf-8") as f:
    text = f.read()

text = text.replace(
    'Microsoft::WRL::ComPtr<ID3D12Resource> gpuMatrix, gpuVector, gpuOutput;',
    'Microsoft::WRL::ComPtr<ID3D12Resource> gpuMatrix, gpuVector, gpuOutput, gpuGamma;'
)
text = text.replace(
    'bool outOk = bridge.UploadGGUFTensor(oi, ob, gpuOutput);',
    'bool outOk = bridge.UploadGGUFTensor(oi, ob, gpuOutput);\n\n'
    '        TensorInfo gi; gi.name = "gamma"; gi.shape = {cfg.cols, 1, 1, 1};\n'
    '        gi.type = GGMLType::F32; gi.size_bytes = cfg.cols * sizeof(float);\n'
    '        std::vector<uint8_t> gb(reinterpret_cast<const uint8_t*>(gamma.data()),\n'
    '                                reinterpret_cast<const uint8_t*>(gamma.data()) + gi.size_bytes);\n'
    '        bool gamOk = bridge.UploadGGUFTensor(gi, gb, gpuGamma);\n'
    '        printf("[GPU Upload] Gamma (%llu bytes): %s\\n", (unsigned long long)gi.size_bytes,\n'
    '               gamOk ? "OK" : "FAILED");'
)
text = text.replace(
    'if (!matOk || !vecOk || !outOk) {',
    'if (!matOk || !vecOk || !gamOk || !outOk) {'
)
text = text.replace(
    'bool dispatchOk = bridge.DispatchMatVecQ4(gpuMatrix.Get(), gpuVector.Get(),',
    'bool dispatchOk = bridge.DispatchRMSNorm(gpuVector.Get(), gpuGamma.Get(), cfg.cols, 1e-5f);\n'
    '            if (dispatchOk) dispatchOk = bridge.DispatchMatVecQ4(gpuMatrix.Get(), gpuVector.Get(),'
)
text = text.replace(
    'printf("[GPU Dispatch] Test MatVec+Softmax dispatch: %s\\n", dispatchOk ? "OK" : "FAILED");',
    'printf("[GPU Dispatch] Test RMSNorm+MatVec+Softmax dispatch: %s\\n", dispatchOk ? "OK" : "FAILED");'
)
text = text.replace(
    'bool ok = bridge.DispatchMatVecQ4(gpuMatrix.Get(), gpuVector.Get(),',
    'bool ok = bridge.DispatchRMSNorm(gpuVector.Get(), gpuGamma.Get(), cfg.cols, 1e-5f);\n'
    '            if (ok) ok = bridge.DispatchMatVecQ4(gpuMatrix.Get(), gpuVector.Get(),'
)
text = text.replace(
    'cpuMatVecQ4(q4matrix.data(), vec.data(), cpuRef.data(), cfg.rows, cfg.cols);',
    'std::vector<float> cpuVecCopy = vec;\n'
    '                    cpuRMSNorm(cpuVecCopy.data(), gamma.data(), cfg.cols, 1e-5f);\n'
    '                    cpuMatVecQ4(q4matrix.data(), cpuVecCopy.data(), cpuRef.data(), cfg.rows, cfg.cols);'
)
text = text.replace(
    'cpuMatVecQ4(q4matrix.data(), vec.data(), output.data(), cfg.rows, cfg.cols);',
    'std::vector<float> cpuVecCopy = vec;\n'
    '            cpuRMSNorm(cpuVecCopy.data(), gamma.data(), cfg.cols, 1e-5f);\n'
    '            cpuMatVecQ4(q4matrix.data(), cpuVecCopy.data(), output.data(), cfg.rows, cfg.cols);'
)
text = text.replace(
    'MatVecQ4+Softmax',
    'RMSNorm+MatVecQ4+Softmax'
)

with open(r"d:\rawrxd\src\tools\gpu_benchmark.cpp", "w", encoding="utf-8", newline='\n') as f:
    f.write(text)
print("Replaced!")
