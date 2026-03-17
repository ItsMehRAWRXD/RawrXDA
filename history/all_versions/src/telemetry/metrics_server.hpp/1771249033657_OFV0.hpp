// telemetry/metrics_server.hpp
// Prometheus-compatible metrics endpoint via Windows HTTP API
#pragma once
#include <windows.h>
#include <atomic>
#include <string>
#include <sstream>
#include <cstdint>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include "async_logger.hpp"

namespace RawrXD::Telemetry {

    // Atomically-updatable double using CAS
    class AtomicDouble {
        std::atomic<uint64_t> bits_{0};
    public:
        void store(double v) { bits_.store(bit_cast_to_uint64(v), std::memory_order_relaxed); }
        double load() const { return bit_cast_to_double(bits_.load(std::memory_order_relaxed)); }
        void add(double v) {
            uint64_t old_bits = bits_.load(std::memory_order_relaxed);
            double old_val, new_val;
            do {
                old_val = bit_cast_to_double(old_bits);
                new_val = old_val + v;
            } while (!bits_.compare_exchange_weak(old_bits, bit_cast_to_uint64(new_val),
                                                   std::memory_order_relaxed));
        }
    private:
        static uint64_t bit_cast_to_uint64(double d) {
            uint64_t r; memcpy(&r, &d, sizeof(r)); return r;
        }
        static double bit_cast_to_double(uint64_t u) {
            double r; memcpy(&r, &u, sizeof(r)); return r;
        }
    };

    struct HistogramBucket {
        double le;
        std::atomic<uint64_t> count{0};
    };

    class MetricsExporter {
        // Counters
        std::atomic<uint64_t> inference_count_{0};
        std::atomic<uint64_t> token_count_{0};
        std::atomic<uint64_t> error_count_{0};
        std::atomic<uint64_t> cache_hits_{0};
        std::atomic<uint64_t> cache_misses_{0};

        // Gauges
        AtomicDouble inference_latency_sum_;
        AtomicDouble gpu_utilization_;
        AtomicDouble ram_usage_gb_;
        AtomicDouble vram_usage_gb_;
        std::atomic<uint64_t> active_sessions_{0};

        // Histogram buckets for inference latency (ms)
        HistogramBucket latency_buckets_[8] = {
            {1.0}, {5.0}, {10.0}, {25.0}, {50.0}, {100.0}, {250.0}, {1000.0}
        };

        // HTTP server state
        std::thread server_thread_;
        std::atomic<bool> running_{false};
        uint16_t port_;

    public:
        explicit MetricsExporter(uint16_t port = 9090) : port_(port) {}

        ~MetricsExporter() { stop(); }

        void start() {
            if (running_.exchange(true)) return;
            RAWR_LOG_INFO("Metrics server starting on port %u", port_);
            server_thread_ = std::thread([this]() { serveLoop(); });
        }

        void stop() {
            if (!running_.exchange(false)) return;
            if (server_thread_.joinable()) server_thread_.join();
        }

        void recordInference(size_t tokens, double latency_ms) {
            inference_count_.fetch_add(1, std::memory_order_relaxed);
            token_count_.fetch_add(tokens, std::memory_order_relaxed);
            inference_latency_sum_.add(latency_ms);
            for (auto& bucket : latency_buckets_) {
                if (latency_ms <= bucket.le) {
                    bucket.count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        void recordError() { error_count_.fetch_add(1, std::memory_order_relaxed); }
        void recordCacheHit() { cache_hits_.fetch_add(1, std::memory_order_relaxed); }
        void recordCacheMiss() { cache_misses_.fetch_add(1, std::memory_order_relaxed); }
        void setGpuUtilization(double pct) { gpu_utilization_.store(pct); }
        void setRamUsage(double gb) { ram_usage_gb_.store(gb); }
        void setVramUsage(double gb) { vram_usage_gb_.store(gb); }
        void sessionOpened() { active_sessions_.fetch_add(1, std::memory_order_relaxed); }
        void sessionClosed() { active_sessions_.fetch_sub(1, std::memory_order_relaxed); }

        std::string scrape() const {
            std::ostringstream ss;
            ss << "# HELP rawrxd_inference_total Total inference requests\n"
               << "# TYPE rawrxd_inference_total counter\n"
               << "rawrxd_inference_total " << inference_count_.load() << "\n\n"

               << "# HELP rawrxd_tokens_total Total tokens generated\n"
               << "# TYPE rawrxd_tokens_total counter\n"
               << "rawrxd_tokens_total " << token_count_.load() << "\n\n"

               << "# HELP rawrxd_errors_total Total errors\n"
               << "# TYPE rawrxd_errors_total counter\n"
               << "rawrxd_errors_total " << error_count_.load() << "\n\n"

               << "# HELP rawrxd_cache_hits_total KV cache hits\n"
               << "# TYPE rawrxd_cache_hits_total counter\n"
               << "rawrxd_cache_hits_total " << cache_hits_.load() << "\n\n"

               << "# HELP rawrxd_cache_misses_total KV cache misses\n"
               << "# TYPE rawrxd_cache_misses_total counter\n"
               << "rawrxd_cache_misses_total " << cache_misses_.load() << "\n\n"

               << "# HELP rawrxd_inference_latency_seconds Inference latency histogram\n"
               << "# TYPE rawrxd_inference_latency_seconds histogram\n";

            for (const auto& bucket : latency_buckets_) {
                ss << "rawrxd_inference_latency_seconds_bucket{le=\""
                   << bucket.le / 1000.0 << "\"} " << bucket.count.load() << "\n";
            }
            ss << "rawrxd_inference_latency_seconds_bucket{le=\"+Inf\"} "
               << inference_count_.load() << "\n"
               << "rawrxd_inference_latency_seconds_sum "
               << inference_latency_sum_.load() / 1000.0 << "\n"
               << "rawrxd_inference_latency_seconds_count "
               << inference_count_.load() << "\n\n"

               << "# HELP rawrxd_gpu_utilization GPU utilization percentage\n"
               << "# TYPE rawrxd_gpu_utilization gauge\n"
               << "rawrxd_gpu_utilization " << gpu_utilization_.load() << "\n\n"

               << "# HELP rawrxd_ram_usage_gb RAM usage in GB\n"
               << "# TYPE rawrxd_ram_usage_gb gauge\n"
               << "rawrxd_ram_usage_gb " << ram_usage_gb_.load() << "\n\n"

               << "# HELP rawrxd_vram_usage_gb VRAM usage in GB\n"
               << "# TYPE rawrxd_vram_usage_gb gauge\n"
               << "rawrxd_vram_usage_gb " << vram_usage_gb_.load() << "\n\n"

               << "# HELP rawrxd_active_sessions Active sessions\n"
               << "# TYPE rawrxd_active_sessions gauge\n"
               << "rawrxd_active_sessions " << active_sessions_.load() << "\n";

            return ss.str();
        }

    private:
        void serveLoop() {
            // Minimal TCP server for /metrics endpoint using Winsock
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                RAWR_LOG_ERROR("WSAStartup failed");
                return;
            }

            SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listen_sock == INVALID_SOCKET) {
                RAWR_LOG_ERROR("socket() failed");
                WSACleanup();
                return;
            }

            int reuse = 1;
            setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuse), sizeof(reuse));

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port_);

            if (bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                RAWR_LOG_ERROR("bind() failed on port %u", port_);
                closesocket(listen_sock);
                WSACleanup();
                return;
            }

            listen(listen_sock, 5);
            RAWR_LOG_INFO("Metrics server listening on :%u", port_);

            // Non-blocking with 100ms timeout
            DWORD timeout_ms = 100;
            setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

            while (running_.load(std::memory_order_relaxed)) {
                SOCKET client = accept(listen_sock, nullptr, nullptr);
                if (client == INVALID_SOCKET) continue;

                // Read request (we only need to know they connected)
                char req[1024];
                recv(client, req, sizeof(req), 0);

                // Generate metrics
                std::string body = scrape();
                std::ostringstream resp;
                resp << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: text/plain; version=0.0.4\r\n"
                     << "Content-Length: " << body.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << body;

                std::string response = resp.str();
                send(client, response.c_str(), static_cast<int>(response.size()), 0);
                closesocket(client);
            }

            closesocket(listen_sock);
            WSACleanup();
        }
    };

} // namespace RawrXD::Telemetry
