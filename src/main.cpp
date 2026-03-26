#include <iostream>
#include <fstream>
#include <sstream>
#include "server.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Load configuration from JSON file
 * @param config_path Path to config.json
 * @param port Reference to store port
 * @param cache_size Reference to store cache size
 * @param db_path Reference to store database path
 * @return true if successful, false otherwise
 */
bool loadConfig(const std::string& config_path, int& port, size_t& cache_size, std::string& db_path) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open config file: " << config_path << std::endl;
        return false;
    }

    try {
        json config;
        config_file >> config;

        port = config.value("port", 8080);
        cache_size = config.value("cache_size", 1000);
        db_path = config.value("database_path", "urls.db");

        std::cout << "Config loaded:" << std::endl;
        std::cout << "  Port: " << port << std::endl;
        std::cout << "  Cache Size: " << cache_size << std::endl;
        std::cout << "  Database: " << db_path << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Print usage information
 * @param program_name Name of the program
 */
void printUsage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config PATH    Path to config file (default: config/config.json)" << std::endl;
    std::cout << "  -p, --port PORT      Server port (overrides config)" << std::endl;
    std::cout << "  -h, --help           Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << "     URL Shortener Backend - C++ Edition\n";
    std::cout << "===========================================\n" << std::endl;

    // Parse command line arguments
    std::string config_path = "config/config.json";
    int port = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        }
        else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }

    // Load configuration
    size_t cache_size = 1000;
    std::string db_path = "urls.db";
    int config_port = 8080;

    if (!loadConfig(config_path, config_port, cache_size, db_path)) {
        std::cerr << "Failed to load configuration, using defaults" << std::endl;
    }

    // Command line port overrides config
    if (port != -1) {
        config_port = port;
    }

    // Create and initialize server
    auto server = std::make_shared<URLShortenerServer>(config_port, db_path, cache_size);

    std::cout << "Initializing server..." << std::endl;
    if (!server->initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    std::cout << "\n✓ Server initialized successfully!" << std::endl;
    std::cout << "\nAvailable Endpoints:" << std::endl;
    std::cout << "  POST http://localhost:" << config_port << "/shorten" << std::endl;
    std::cout << "  GET  http://localhost:" << config_port << "/{code}" << std::endl;
    std::cout << "  GET  http://localhost:" << config_port << "/analytics/{code}" << std::endl;
    std::cout << "  GET  http://localhost:" << config_port << "/health" << std::endl;
    std::cout << "  GET  http://localhost:" << config_port << "/stats" << std::endl;
    std::cout << "\nServer is running on port " << config_port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "===========================================\n" << std::endl;

    try {
        // Start server (blocking call)
        server->start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        server->stop();
        return 1;
    }

    return 0;
}
