#pragma once

#include <crow.h>
#include <memory>
#include <string>

#include "cache.h"
#include "database.h"
#include "shortener.h"
#include "rate_limiter.h"

class URLShortenerServer : public std::enable_shared_from_this<URLShortenerServer>
{
private:
    // Configuration
    int port_;
    size_t cache_size_;

    // Crow App
    crow::SimpleApp app_;

    // Components
    std::shared_ptr<Database> db_;
    std::shared_ptr<URLShortener> shortener_;
    std::shared_ptr<RateLimiter> rate_limiter_;
    std::shared_ptr<LRUCache<std::string, std::string>> url_cache_;

    // Helpers
    void initializeLogging();
    std::string getClientIP(const crow::request& req);
    void log(const std::string& level, const std::string& message);

    // Route handlers
    crow::response handleShorten(const crow::request& req);
    crow::response handleRedirect(const std::string& code);
    crow::response handleAnalytics(const std::string& code);
    crow::response handleStats();
    crow::response handleHealth();

public:
    URLShortenerServer(int port,
                       const std::string& db_path,
                       size_t cache_size);

    ~URLShortenerServer();

    bool initialize();
    void start();
    void stop();
};