#pragma once

#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

class RateLimiter {
public:
    RateLimiter(int max_requests, int window_seconds);
    RateLimiter() = default;

    bool isAllowed(const std::string& ip);
    int getRemainingRequests(const std::string& ip);
    void cleanup();

private:
    struct IPRecord {
        std::vector<std::chrono::system_clock::time_point> timestamps;
    };

    std::unordered_map<std::string, IPRecord> records;
    std::mutex mtx;

    int MAX_REQUESTS = 100;
    int WINDOW_SECONDS = 60;

    std::chrono::system_clock::time_point getCurrentTime();
    void cleanupOldTimestamps(const std::string& ip);
};