#include "logging/logger.h"
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <cmath>
#include "telemetry.h"

static Logger s_stressLogger("OCStress");

// Simple CPU matmul and memory bandwidth stress harness.
// Aborts if telemetry temps exceed user thresholds passed via args.
// Usage: rawrxd_stress --cpu-max 85 --gpu-max 95 --seconds 120 --size 512

struct Args {
    int cpuMax = 85;
    int gpuMax = 95;
    int seconds = 60;
    int size = 512;
};

static Args Parse(int argc, char* argv[]) {
    Args a; for (int i=1;i<argc;i++) {
        std::string s = argv[i];
        if (s == "--cpu-max" && i+1 < argc) a.cpuMax = std::stoi(argv[++i]);
        else if (s == "--gpu-max" && i+1 < argc) a.gpuMax = std::stoi(argv[++i]);
        else if (s == "--seconds" && i+1 < argc) a.seconds = std::stoi(argv[++i]);
        else if (s == "--size" && i+1 < argc) a.size = std::stoi(argv[++i]);
    }
    return a;
}

static void MatMul(const std::vector<float>& A, const std::vector<float>& B, std::vector<float>& C, int N) {
    for (int i=0;i<N;i++) {
        const float* arow = &A[i*N];
        for (int j=0;j<N;j++) {
            float sum = 0.0f;
            for (int k=0;k<N;k++) sum += arow[k] * B[k*N + j];
            C[i*N + j] = sum;
        }
    }
}

int main(int argc, char* argv[]) {
    Args args = Parse(argc, argv);
    s_stressLogger.info("RawrXD Stress Harness");
    s_stressLogger.info("Target runtime: {}s size={} threshold CPU={}C GPU={}C", 
                        args.seconds, args.size, args.cpuMax, args.gpuMax);

    telemetry::Initialize();

    int N = args.size;
    std::vector<float> A(N*N), B(N*N), C(N*N);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> dist(-1.f,1.f);
    for (auto &v : A) v = dist(rng);
    for (auto &v : B) v = dist(rng);

    auto start = std::chrono::steady_clock::now();
    int iters = 0;
    double worstCpuTemp = -1; double worstGpuTemp = -1;

    while (true) {
        MatMul(A,B,C,N);
        iters++;
        // Light transform to keep compiler from optimizing everything out
        for (int i=0;i<N;i++) C[i] = std::sin(C[i]);

        telemetry::TelemetrySnapshot snap; telemetry::Poll(snap);
        if (snap.cpuTempValid && snap.cpuTempC > worstCpuTemp) worstCpuTemp = snap.cpuTempC;
        if (snap.gpuTempValid && snap.gpuTempC > worstGpuTemp) worstGpuTemp = snap.gpuTempC;

        if ((snap.cpuTempValid && snap.cpuTempC >= args.cpuMax) || (snap.gpuTempValid && snap.gpuTempC >= args.gpuMax)) {
            s_stressLogger.info << "ABORT: Thermal threshold exceeded (CPU="
                      << (snap.cpuTempValid? snap.cpuTempC : -1) << "C GPU="
                      << (snap.gpuTempValid? snap.gpuTempC : -1) << "C) after " << iters << " iterations\n";
            break;
        }

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed >= args.seconds) {
            s_stressLogger.info("Completed duration: {}s iterations={}", elapsed, iters);
            break;
        }
        if (iters % 5 == 0) {
            int cpu = snap.cpuTempValid ? snap.cpuTempC : -1;
            int gpu = snap.gpuTempValid ? snap.gpuTempC : -1;
            s_stressLogger.info("[Status] iter={} CPU={}C GPU={}C", iters, cpu, gpu);
        }
    }

    s_stressLogger.info("Peak CPU temp: {}C Peak GPU temp: {}C", worstCpuTemp, worstGpuTemp);
    telemetry::Shutdown();
    return 0;
}
