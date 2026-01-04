#include "profiler.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace nicxlive::core::render {

namespace {
struct RenderProfiler {
    std::map<std::string, long long> accumUsec{};
    std::map<std::string, std::size_t> callCounts{};
    std::chrono::steady_clock::time_point lastReport{};
    std::size_t frameCount{0};

    void addSample(const std::string& label, std::chrono::steady_clock::duration sample) {
        accumUsec[label] += std::chrono::duration_cast<std::chrono::microseconds>(sample).count();
        callCounts[label] += 1;
    }

    void frameCompleted() {
        frameCount++;
        auto now = std::chrono::steady_clock::now();
        if (lastReport.time_since_epoch().count() == 0) {
            lastReport = now;
            return;
        }
        auto elapsed = now - lastReport;
        if (elapsed >= std::chrono::seconds(1)) {
            report(elapsed);
            accumUsec.clear();
            callCounts.clear();
            frameCount = 0;
            lastReport = now;
        }
    }

private:
    void report(std::chrono::steady_clock::duration interval) {
        double secondsElapsed = std::chrono::duration_cast<std::chrono::microseconds>(interval).count() / 1'000'000.0;
        std::cerr << "[RenderProfiler] " << std::fixed << std::setprecision(3) << secondsElapsed << "s window ("
                  << frameCount << " frames)" << std::endl;
        if (accumUsec.empty()) {
            std::cerr << "  (no instrumented passes recorded)" << std::endl;
            return;
        }
        std::vector<std::pair<std::string, long long>> entries(accumUsec.begin(), accumUsec.end());
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& entry : entries) {
            double totalMs = entry.second / 1000.0;
            auto countIt = callCounts.find(entry.first);
            std::size_t count = countIt == callCounts.end() ? 0 : countIt->second;
            double avgMs = count ? totalMs / static_cast<double>(count) : totalMs;
            std::cerr << "  " << std::left << std::setw(18) << entry.first
                      << " total=" << std::setw(8) << std::fixed << std::setprecision(3) << totalMs << " ms"
                      << "  avg=" << std::setw(6) << std::fixed << std::setprecision(3) << avgMs << " ms"
                      << "  calls=" << std::setw(6) << count << std::endl;
        }
    }
};

RenderProfiler& profiler() {
    static RenderProfiler inst;
    return inst;
}
} // namespace

RenderProfileScope::RenderProfileScope(const std::string& label)
    : label_(label), active_(true), start_(std::chrono::steady_clock::now()) {}

RenderProfileScope::RenderProfileScope(RenderProfileScope&& other) noexcept
    : label_(std::move(other.label_)), active_(other.active_), start_(other.start_) {
    other.active_ = false;
}

RenderProfileScope& RenderProfileScope::operator=(RenderProfileScope&& other) noexcept {
    if (this != &other) {
        stop();
        label_ = std::move(other.label_);
        active_ = other.active_;
        start_ = other.start_;
        other.active_ = false;
    }
    return *this;
}

RenderProfileScope::~RenderProfileScope() { stop(); }

void RenderProfileScope::stop() {
    if (!active_ || label_.empty()) return;
    auto duration = std::chrono::steady_clock::now() - start_;
    profiler().addSample(label_, duration);
    active_ = false;
}

RenderProfileScope profileScope(const std::string& label) {
    return RenderProfileScope(label);
}

void renderProfilerFrameCompleted() {
    profiler().frameCompleted();
}

} // namespace nicxlive::core::render
