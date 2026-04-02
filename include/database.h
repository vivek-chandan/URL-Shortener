#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

struct sqlite3;

/**
 * @brief URL record structure
 */
struct URLRecord {
    int id;
    std::string long_url;
    std::string short_code;
    int click_count;
    std::string created_at;
    std::string last_accessed;
};

/**
 * @brief Database abstraction layer for SQLite
 * 
 * Handles all database operations including:
 * - URL storage and retrieval
 * - Click count tracking
 * - Thread-safe operations
 */
class Database {
private:
    sqlite3* db_;
    std::mutex db_lock_;
    std::string db_path_;

    // Helper methods
    std::string getCurrentTimestamp();
    bool executeSQL(const std::string& sql);

public:
    /**
     * @brief Constructor
     * @param db_path Path to SQLite database file
     */
    explicit Database(const std::string& db_path);

    /**
     * @brief Destructor - closes database
     */
    ~Database();

    /**
     * @brief Initialize database and create schema
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Insert new URL record
     * @param long_url The original long URL
     * @param short_code The generated short code
     * @return Database ID of inserted record, -1 on failure
     */
    int insertURL(const std::string& long_url, const std::string& short_code);

    /**
     * @brief Retrieve URL record by short code
     * @param short_code The short code to look up
     * @param record Reference to store the result
     * @return true if found, false otherwise
     */
    bool getURLByCode(const std::string& short_code, URLRecord& record);

    /**
     * @brief Get URL record by ID
     * @param id The database ID
     * @param record Reference to store the result
     * @return true if found, false otherwise
     */
    bool getURLByID(int id, URLRecord& record);

    /**
     * @brief Get short code by long URL
     * @param long_url The original long URL
     * @param short_code Reference to store found short code
     * @return true if mapping exists, false otherwise
     */
    bool getActiveCodeByLongURL(const std::string& long_url, std::string& short_code);

    /**
     * @brief Increment click count for a URL
     * @param short_code The short code
     * @param long_url Reference to store the long URL (for caching)
     * @return true if successful, false otherwise
     */
    bool incrementClickCount(const std::string& short_code, std::string& long_url);

    /**
     * @brief Get analytics for a URL
     * @param short_code The short code
     * @param record Reference to store the result
     * @return true if found, false otherwise
     */
    bool getAnalytics(const std::string& short_code, URLRecord& record);

    /**
     * @brief Delete URL by short code
     * @param short_code The short code to delete
     * @return true if successful, false otherwise
     */
    bool deleteURL(const std::string& short_code);

    /**
     * @brief Check if short code already exists
     * @param short_code The short code to check
     * @return true if exists, false otherwise
     */
    bool codeExists(const std::string& short_code);

    /**
     * @brief Get total number of URLs in database
     * @return Total count
     */
    int getTotalURLCount();

    /**
     * @brief Close database connection
     */
    void close();
};
