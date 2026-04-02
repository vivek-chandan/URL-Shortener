#pragma once

#include <string>
#include <memory>

class Database;

/**
 * @brief URL Shortening logic using Base62 encoding
 * 
 * Handles:
 * - Base62 encoding/decoding
 * - Short code generation
 * - URL validation
 */
class URLShortener {
private:
    static constexpr const char* BASE62_CHARS = 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static constexpr size_t BASE62_SIZE = 62;

    std::shared_ptr<Database> db_;

    /**
     * @brief Generate random Base62 short code
     * @param length Desired code length
     * @return Random Base62 string
     */
    std::string generateRandomCode(size_t length);

    /**
     * @brief Generate Base62 string from integer ID
     * @param id The database ID
     * @return Base62 encoded string
     */
    std::string encodeBase62(int id);

    /**
     * @brief Decode Base62 string to integer
     * @param code The Base62 string
     * @return Decoded integer, -1 on failure
     */
    int decodeBase62(const std::string& code);

public:
    /**
     * @brief Constructor
     * @param db Shared pointer to Database instance
     */
    explicit URLShortener(std::shared_ptr<Database> db);

    /**
     * @brief Validate URL format
     * @param url The URL to validate
     * @return true if valid URL, false otherwise
     */
    static bool isValidURL(const std::string& url);

    /**
     * @brief Create shortened URL
     * @param long_url The original URL
     * @param short_code Reference to store generated short code
     * @return true if successful, false otherwise
     */
    bool shortenURL(const std::string& long_url, std::string& short_code);

    /**
     * @brief Get original URL by short code
     * @param short_code The short code
     * @param long_url Reference to store the original URL
     * @return true if found, false otherwise
     */
    bool getLongURL(const std::string& short_code, std::string& long_url);

    /**
     * @brief Get short code info
     * @param short_code The short code
     * @param clicks Reference to store click count
     * @param created_at Reference to store creation timestamp
     * @param last_accessed Reference to store last accessed timestamp
     * @return true if found, false otherwise
     */
    bool getCodeInfo(const std::string& short_code, int& clicks, 
                     std::string& created_at, std::string& last_accessed);

    /**
     * @brief Delete short URL
     * @param short_code The short code to delete
     * @return true if successful, false otherwise
     */
    bool deleteShortURL(const std::string& short_code);

};
