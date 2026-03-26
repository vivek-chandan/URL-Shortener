#include "rate_limiter.h"

RateLimiter::RateLimiter(int max_requests, int window_seconds)
    : MAX_REQUESTS(max_requests), WINDOW_SECONDS(window_seconds) {}

std::chrono::system_clock::time_point RateLimiter::getCurrentTime() {
    return std::chrono::system_clock::now();
}

void RateLimiter::cleanupOldTimestamps(const std::string& ip) {
    auto it = records.find(ip);
    if (it == records.end()) return;

    auto now = getCurrentTime();
    auto cutoff = now - std::chrono::seconds(WINDOW_SECONDS);

    auto& timestamps = it->second.timestamps;
    timestamps.erase(
        std::remove_if(
            timestamps.begin(),
            timestamps.end(),
            [cutoff](const auto& ts) { return ts < cutoff; }
        ),
        timestamps.end()
    );
}

bool RateLimiter::isAllowed(const std::string& ip) {
    std::lock_guard<std::mutex> guard(mtx);

    cleanupOldTimestamps(ip);

    auto& record = records[ip];

    if (record.timestamps.size() < MAX_REQUESTS) {
        record.timestamps.push_back(getCurrentTime());
        return true;
    }

    return false;
}

int RateLimiter::getRemainingRequests(const std::string& ip) {
    std::lock_guard<std::mutex> guard(mtx);

    cleanupOldTimestamps(ip);

    auto it = records.find(ip);
    if (it == records.end()) {
        return MAX_REQUESTS;
    }

    int used = it->second.timestamps.size();
    return MAX_REQUESTS - used;
}

void RateLimiter::cleanup() {
    std::lock_guard<std::mutex> guard(mtx);

    auto now = getCurrentTime();
    auto cutoff = now - std::chrono::seconds(WINDOW_SECONDS);

    for (auto it = records.begin(); it != records.end();) {
        auto& timestamps = it->second.timestamps;

        timestamps.erase(
            std::remove_if(
                timestamps.begin(),
                timestamps.end(),
                [cutoff](const auto& ts) { return ts < cutoff; }
            ),
            timestamps.end()
        );

        if (timestamps.empty()) {
            it = records.erase(it);
        } else {
            ++it;
        }
    }
}