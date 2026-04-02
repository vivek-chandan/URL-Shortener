#pragma once

#include "httplib.h"
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

    // HTTP server
    httplib::Server server_;

    // Components
    std::shared_ptr<Database> db_;
    std::shared_ptr<URLShortener> shortener_;
    std::shared_ptr<RateLimiter> rate_limiter_;
    std::shared_ptr<LRUCache<std::string, std::string>> url_cache_;

    // Helpers
    void initializeLogging();
    std::string getClientIP(const httplib::Request& req);
    void log(const std::string& level, const std::string& message);

public:
    URLShortenerServer(int port,
                       const std::string& db_path,
                       size_t cache_size);

    ~URLShortenerServer();

    bool initialize();
    void start();
    void stop();
};