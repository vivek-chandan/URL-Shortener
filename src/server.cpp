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
std::string URLShortenerServer::getClientIP(const httplib::Request& req) {
    if (req.has_header("x-forwarded-for")) {
        std::string forwarded = req.get_header_value("x-forwarded-for");
        size_t pos = forwarded.find(',');
        return forwarded.substr(0, pos);
    }
    return req.remote_addr;
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

    return true;
}

// Start server
void URLShortenerServer::start() {

    // Root endpoint with API usage information
    server_.Get("/", [this](const httplib::Request&, httplib::Response& res) {
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

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    });

    // Health endpoint
    server_.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // Stats endpoint
    server_.Get("/stats", [this](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("{\"service\":\"URL Shortener\"}", "application/json");
    });

    // Analytics endpoint
    server_.Get(R"(/analytics/([A-Za-z0-9]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string code = req.matches[1];
        std::string client_ip = getClientIP(req);
        if (!rate_limiter_->isAllowed(client_ip)) {
            res.status = 429;
            res.set_content("Rate limit exceeded", "text/plain");
            return;
        }

        int clicks;
        std::string created_at, last_accessed;

        if (!shortener_->getCodeInfo(code, clicks, created_at, last_accessed)) {
            res.status = 404;
            res.set_content("{\"error\":\"Short URL not found\"}", "application/json");
            return;
        }

        json resp = {
            {"code", code},
            {"clicks", clicks},
            {"created_at", created_at},
            {"last_accessed", last_accessed}
        };

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    });

    // Shorten URL endpoint
    server_.Post("/shorten", [this](const httplib::Request& req, httplib::Response& res) {
        std::string client_ip = getClientIP(req);
        if (!rate_limiter_->isAllowed(client_ip)) {
            res.status = 429;
            res.set_content("Rate limit exceeded", "text/plain");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
            return;
        }

        if (!body.contains("url") || !body["url"].is_string()) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
            return;
        }

        std::string long_url = body["url"].get<std::string>();

        if (!URLShortener::isValidURL(long_url)) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid URL\"}", "application/json");
            return;
        }

        std::string short_code;
        if (!shortener_->shortenURL(long_url, short_code)) {
            res.status = 500;
            res.set_content("{\"error\":\"Failed to shorten URL\"}", "application/json");
            return;
        }

        url_cache_->put(short_code, long_url);

        std::string short_url =
            "http://localhost:" + std::to_string(port_) + "/" + short_code;

        json resp = {
            {"short_url", short_url},
            {"code", short_code}
        };

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    });

    // Redirect endpoint (KEEP LAST)
    server_.Get(R"(/([A-Za-z0-9]+))", [this](const httplib::Request& req, httplib::Response& res) {

        std::string code = req.matches[1];

        std::string client_ip = getClientIP(req);
        if (!rate_limiter_->isAllowed(client_ip)) {
            res.status = 429;
            res.set_content("Rate limit exceeded", "text/plain");
            return;
        }

        std::string long_url;

        // Check cache
        if (url_cache_->get(code, long_url)) {
            std::string dummy;
            db_->incrementClickCount(code, dummy);

            res.status = 302;
            res.set_header("Location", long_url);
            return;
        }

        // Check database
        if (!shortener_->getLongURL(code, long_url)) {
            res.status = 404;
            res.set_content("{\"error\":\"Short URL not found\"}", "application/json");
            return;
        }

        url_cache_->put(code, long_url);

        std::string dummy;
        db_->incrementClickCount(code, dummy);

        res.status = 302;
        res.set_header("Location", long_url);
    });

    log("INFO", "Starting server on port " + std::to_string(port_));
    server_.new_task_queue = [] {
        return new httplib::ThreadPool(8);
    };
    server_.listen("0.0.0.0", port_);
}

// Stop server
void URLShortenerServer::stop() {
    log("INFO", "Stopping server");
    server_.stop();
}