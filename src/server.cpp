#include "server.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>

using json = nlohmann::json;

// Constructor
URLShortenerServer::URLShortenerServer(int port,
                                       const std::string& db_path,
                                       size_t cache_size)
    : port_(port), cache_size_(cache_size)
{
    db_ = std::make_shared<Database>(db_path);
    shortener_ = std::make_shared<URLShortener>(db_);
    rate_limiter_ = std::make_shared<RateLimiter>(100, 60);
    url_cache_ = std::make_shared<LRUCache<std::string, std::string>>(cache_size_);

    initializeLogging();
}

// Destructor
URLShortenerServer::~URLShortenerServer() {
    if (db_) {
        db_->close();
    }
}

// Logging initialization
void URLShortenerServer::initializeLogging() {
    std::ofstream log_file("logs/server.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << "\n=== Server Started ===\n";
        log_file.close();
    }
}

// Get client IP
std::string URLShortenerServer::getClientIP(const crow::request& req) {
    if (req.headers.count("x-forwarded-for")) {
        std::string forwarded = req.get_header_value("x-forwarded-for");
        size_t pos = forwarded.find(',');
        return forwarded.substr(0, pos);
    }
    return req.remote_ip_address;
}

// Logging function
void URLShortenerServer::log(const std::string& level,
                             const std::string& message)
{
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);

    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
        << "[" << level << "] " << message;

    std::string log_msg = oss.str();

    std::cout << log_msg << std::endl;

    std::ofstream log_file("logs/server.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << log_msg << std::endl;
    }
}

// Initialize components
bool URLShortenerServer::initialize() {
    if (!db_->initialize()) {
        log("ERROR", "Failed to initialize database");
        return false;
    }

    log("INFO", "Database initialized successfully");

    int deleted = shortener_->cleanupExpiredURLs();
    if (deleted > 0) {
        log("INFO", "Cleaned expired URLs: " + std::to_string(deleted));
    }

    return true;
}

// Start server
void URLShortenerServer::start() {

    // Root endpoint with API usage information
    CROW_ROUTE(app_, "/")
    ([this]() {
        json resp = {
            {"service", "URL Shortener"},
            {"status", "running"},
            {"endpoints", {
                {{"method", "GET"}, {"path", "/health"}, {"description", "Service health check"}},
                {{"method", "GET"}, {"path", "/stats"}, {"description", "Basic service metadata"}},
                {{"method", "POST"}, {"path", "/shorten"}, {"description", "Create a short URL"}},
                {{"method", "GET"}, {"path", "/analytics/{code}"}, {"description", "Get click analytics for a code"}},
                {{"method", "GET"}, {"path", "/{code}"}, {"description", "Redirect to original URL"}}
            }},
            {"example", {
                {"curl", "curl -X POST http://localhost:" + std::to_string(port_) + "/shorten -H 'Content-Type: application/json' -d '{\\\"url\\\":\\\"https://example.com\\\"}'"}
            }}
        };

        return crow::response(200, resp.dump());
    });

    // Health endpoint
    CROW_ROUTE(app_, "/health")
    ([this]() {
        return crow::response(200, "{\"status\":\"ok\"}");
    });

    // Stats endpoint
    CROW_ROUTE(app_, "/stats")
    ([this]() {
        return crow::response(200, "{\"service\":\"URL Shortener\"}");
    });

    // Analytics endpoint
    CROW_ROUTE(app_, "/analytics/<string>")
    ([this](const crow::request& req, const std::string& code) {
        std::string client_ip = getClientIP(req);
        if (!rate_limiter_->isAllowed(client_ip)) {
            return crow::response(429, "Rate limit exceeded");
        }

        int clicks;
        std::string created_at, last_accessed;

        if (!shortener_->getCodeInfo(code, clicks, created_at, last_accessed)) {
            return crow::response(404, "{\"error\":\"Short URL not found\"}");
        }

        json resp = {
            {"code", code},
            {"clicks", clicks},
            {"created_at", created_at},
            {"last_accessed", last_accessed}
        };

        return crow::response(200, resp.dump());
    });

    // Shorten URL endpoint
    CROW_ROUTE(app_, "/shorten").methods("POST"_method)
    ([this](const crow::request& req) {
        std::string client_ip = getClientIP(req);
        if (!rate_limiter_->isAllowed(client_ip)) {
            return crow::response(429, "Rate limit exceeded");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("url")) {
            return crow::response(400, "{\"error\":\"Invalid JSON\"}");
        }

        std::string long_url = body["url"].s();
        int expiry_days = 0;
        if (body.has("expiry_days")) {
            expiry_days = body["expiry_days"].i();
        }

        if (!URLShortener::isValidURL(long_url)) {
            return crow::response(400, "{\"error\":\"Invalid URL\"}");
        }

        std::string short_code;
        if (!shortener_->shortenURL(long_url, expiry_days, short_code)) {
            return crow::response(500, "{\"error\":\"Failed to shorten URL\"}");
        }

        url_cache_->put(short_code, long_url);

        std::string short_url =
            "http://localhost:" + std::to_string(port_) + "/" + short_code;

        json resp = {
            {"short_url", short_url},
            {"code", short_code}
        };

        return crow::response(200, resp.dump());
    });

    // Redirect endpoint (KEEP LAST)
    CROW_ROUTE(app_, "/<string>")
    ([this](const crow::request& req, const std::string& code) {

        std::string client_ip = getClientIP(req);
        if (!rate_limiter_->isAllowed(client_ip)) {
            return crow::response(429, "Rate limit exceeded");
        }

        std::string long_url;

        // Check cache
        if (url_cache_->get(code, long_url)) {
            std::string dummy;
            db_->incrementClickCount(code, dummy);

            crow::response resp(302);
            resp.set_header("Location", long_url);
            return resp;
        }

        // Check database
        if (!shortener_->getLongURL(code, long_url)) {
            return crow::response(404, "{\"error\":\"Short URL not found\"}");
        }

        url_cache_->put(code, long_url);

        std::string dummy;
        db_->incrementClickCount(code, dummy);

        crow::response resp(302);
        resp.set_header("Location", long_url);
        return resp;
    });

    log("INFO", "Starting server on port " + std::to_string(port_));
    app_.port(port_).multithreaded().run();
}

// Stop server
void URLShortenerServer::stop() {
    log("INFO", "Stopping server");
    app_.stop();
}