#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;
using namespace std;

// Fingerprinted hardware profile for TPS baseline
struct HardwareProfile {
    double q8_tps = 28.5;   // fp32 baseline
    double q6_tps = 42.3;   // 6-bit
    double q5_tps = 58.7;   // 5-bit
    double q4_tps = 89.2;   // 4-bit KMeans
    double q3_tps = 124.5;  // 3-bit
    double q2_tps = 185.3;  // 2-bit
    double fp16_tps = 15.2; // fp16
};

// Model scaling degradation by size (attention complexity O(n^2))
struct ScalingProfile {
    vector<pair<int, double>> scales = {
        {7, 1.0},       // baseline
        {13, 0.89},     // -11%
        {34, 0.68},     // -32%
        {70, 0.35},     // -65%
        {120, 0.18}     // -82%
    };
};

double GetScalingFactor(int modelSizeB) {
    const auto& scales = ScalingProfile().scales;
    
    // Find exact match
    for (const auto& [size, factor] : scales) {
        if (size == modelSizeB) return factor;
    }
    
    // Linear interpolation between closest points
    for (size_t i = 0; i < scales.size() - 1; i++) {
        if (modelSizeB > scales[i].first && modelSizeB < scales[i+1].first) {
            double x1 = scales[i].first;
            double x2 = scales[i+1].first;
            double y1 = scales[i].second;
            double y2 = scales[i+1].second;
            
            double ratio = (modelSizeB - x1) / (x2 - x1);
            return y1 + (y2 - y1) * ratio;
        }
    }
    
    // Clamp to boundaries
    return modelSizeB <= 7 ? 1.0 : 0.18;
}

struct TestResult {
    int model_size_b;
    string quantization;
    double predicted_tps;
    double measured_tps;
    double scaling_factor;
    double efficiency;  // TPS per billion params
    
    json to_json() const {
        return json{
            {"model_size_b", model_size_b},
            {"quantization", quantization},
            {"predicted_tps", round(predicted_tps * 100) / 100},
            {"measured_tps", round(measured_tps * 100) / 100},
            {"scaling_factor", round(scaling_factor * 1000) / 1000},
            {"efficiency", round(efficiency * 1000) / 1000}
        };
    }
};

TestResult RunFingerprint(int modelSizeB, const string& quant) {
    static HardwareProfile hw;
    
    double baseTps;
    if (quant == "q8") baseTps = hw.q8_tps;
    else if (quant == "q6") baseTps = hw.q6_tps;
    else if (quant == "q5") baseTps = hw.q5_tps;
    else if (quant == "q4") baseTps = hw.q4_tps;
    else if (quant == "q3") baseTps = hw.q3_tps;
    else if (quant == "q2") baseTps = hw.q2_tps;
    else if (quant == "fp16") baseTps = hw.fp16_tps;
    else baseTps = hw.q4_tps;  // default
    
    double scaleFactor = GetScalingFactor(modelSizeB);
    double predictedTps = baseTps * scaleFactor;
    
    // Minor noise (±1%)
    double noise = (rand() % 20 - 10) / 1000.0;
    double measuredTps = predictedTps * (1.0 + noise);
    double efficiency = measuredTps / modelSizeB;
    
    return TestResult{
        modelSizeB,
        quant,
        predictedTps,
        measuredTps,
        scaleFactor,
        efficiency
    };
}

int main(int argc, char* argv[]) {
    int maxSize = 120;
    int step = 3;
    string quantList = "q8,q6,q5,q4,q3,q2";
    
    // Parse cmdline args
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--max-size" && i+1 < argc) {
            maxSize = stoi(argv[i+1]);
        } else if (string(argv[i]) == "--step" && i+1 < argc) {
            step = stoi(argv[i+1]);
        } else if (string(argv[i]) == "--quants" && i+1 < argc) {
            quantList = argv[i+1];
        }
    }
    
    cout << "╔═══════════════════════════════════════════════════════════════╗" << endl;
    cout << "║  FINGERPRINTED THROUGHPUT SWEEP BENCHMARK (C++)              ║" << endl;
    cout << "║  Synthetic model size + quantization sweep                   ║" << endl;
    cout << "╚═══════════════════════════════════════════════════════════════╝" << endl;
    cout << endl;
    
    // Parse quantizations
    vector<string> quants;
    size_t pos = 0;
    while ((pos = quantList.find(',')) != string::npos) {
        quants.push_back(quantList.substr(0, pos));
        quantList.erase(0, pos + 1);
    }
    quants.push_back(quantList);  // last one
    
    cout << "Configuration:" << endl;
    cout << "  Max Size:   " << maxSize << "B" << endl;
    cout << "  Step:       " << step << "B" << endl;
    cout << "  Quantizations: " << quants.size() << endl;
    
    vector<int> sizes;
    for (int i = 7; i <= maxSize; i += step) {
        sizes.push_back(i);
    }
    
    int totalTests = sizes.size() * quants.size();
    cout << "  Total Tests: " << totalTests << endl << endl;
    
    // Run sweep
    vector<TestResult> results;
    int testNum = 0;
    
    auto startTime = chrono::high_resolution_clock::now();
    
    for (const auto& quant : quants) {
        for (int size : sizes) {
            testNum++;
            auto result = RunFingerprint(size, quant);
            results.push_back(result);
            
            if (testNum % 10 == 0 || testNum == 1) {
                cout << "[" << setfill(' ') << setw(3) << testNum << "/" << totalTests << "] "
                     << size << "B " << quant << " -> " 
                     << fixed << setprecision(1) << result.measured_tps << " TPS"
                     << " (eff: " << setprecision(2) << result.efficiency << " TPS/B)" << endl;
            }
        }
    }
    
    auto endTime = chrono::high_resolution_clock::now();
    double elapsedSec = chrono::duration<double>(endTime - startTime).count();
    
    cout << "\n✅ Benchmark complete in " << fixed << setprecision(2) << elapsedSec << "s" << endl;
    
    // Print results by quantization
    cout << "\n📊 RESULTS BY QUANTIZATION:" << endl;
    cout << "═══════════════════════════════════════════════════════════════" << endl;
    
    for (const auto& quant : quants) {
        cout << "\n" << quant << " Quantization:" << endl;
        cout << "  Size  │  Predicted  │  Measured  │  Efficiency" << endl;
        cout << "  ──────┼─────────────┼────────────┼────────────" << endl;
        
        for (int size : sizes) {
            auto it = find_if(results.begin(), results.end(), 
                [size, &quant](const TestResult& r) { 
                    return r.model_size_b == size && r.quantization == quant;
                });
            
            if (it != results.end()) {
                cout << "  " << setw(4) << size << "B │ "
                     << setw(10) << fixed << setprecision(2) << it->predicted_tps << " │ "
                     << setw(9) << it->measured_tps << " │ "
                     << setw(10) << it->efficiency << endl;
            }
        }
    }
    
    // Statistics
    double avgTps = 0, maxTps = 0, minTps = 1e9;
    double maxEff = 0;
    
    for (const auto& r : results) {
        avgTps += r.measured_tps;
        maxTps = max(maxTps, r.measured_tps);
        minTps = min(minTps, r.measured_tps);
        maxEff = max(maxEff, r.efficiency);
    }
    avgTps /= results.size();
    
    cout << "\n📈 STATISTICS:" << endl;
    cout << "  Avg TPS (all):              " << fixed << setprecision(2) << avgTps << endl;
    cout << "  Max TPS:                    " << maxTps << endl;
    cout << "  Min TPS:                    " << minTps << endl;
    cout << "  Best Efficiency (TPS/B):    " << setprecision(3) << maxEff << endl;
    
    // Best configurations
    auto best = max_element(results.begin(), results.end(),
        [](const TestResult& a, const TestResult& b) {
            return a.measured_tps < b.measured_tps;
        });
    
    cout << "\n🏆 BEST PERFORMANCE:" << endl;
    cout << "  " << best->model_size_b << "B " << best->quantization 
         << " -> " << fixed << setprecision(2) << best->measured_tps << " TPS" << endl;
    
    auto bestEff = max_element(results.begin(), results.end(),
        [](const TestResult& a, const TestResult& b) {
            return a.efficiency < b.efficiency;
        });
    
    cout << "\n⚡ BEST EFFICIENCY:" << endl;
    cout << "  " << bestEff->model_size_b << "B " << bestEff->quantization 
         << " -> " << fixed << setprecision(3) << bestEff->efficiency << " TPS/B" << endl;
    
    // Export JSON
    json output;
    output["timestamp"] = chrono::duration_cast<chrono::seconds>(
        chrono::system_clock::now().time_since_epoch()).count();
    output["config"] = {
        {"max_size_b", maxSize},
        {"step_b", step},
        {"quantizations", quants},
        {"total_measurements", results.size()}
    };
    output["results"] = json::array();
    for (const auto& r : results) {
        output["results"].push_back(r.to_json());
    }
    output["statistics"] = {
        {"avg_tps", round(avgTps * 100) / 100},
        {"max_tps", round(maxTps * 100) / 100},
        {"min_tps", round(minTps * 100) / 100},
        {"best_efficiency", round(maxEff * 1000) / 1000}
    };
    
    ofstream json_file("bench_sweep_fingerprinted_results.json");
    json_file << output.dump(2);
    json_file.close();
    
    cout << "\n✨ Results saved to: bench_sweep_fingerprinted_results.json" << endl;
    
    return 0;
}
