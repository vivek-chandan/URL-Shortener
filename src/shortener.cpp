#include "shortener.h"
#include "database.h"
#include <regex>
#include <algorithm>
#include <random>

static const std::string BASE62 =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

std::string encodeBase62(int num) {
    std::string result;

    while (num > 0) {
        result += BASE62[num % 62];
        num /= 62;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

URLShortener::URLShortener(std::shared_ptr<Database> db) 
    : db_(db) {}

std::string URLShortener::encodeBase62(int id) {
    if (id == 0) return "a";

    std::string result;
    while (id > 0) {
        result = BASE62_CHARS[id % BASE62_SIZE] + result;
        id /= BASE62_SIZE;
    }
    return result;
}


int URLShortener::decodeBase62(const std::string& code) {
    int id = 0;
    for (char c : code) {
        int digit = -1;
        for (size_t i = 0; i < BASE62_SIZE; i++) {
            if (BASE62_CHARS[i] == c) {
                digit = i;
                break;
            }
        }
        if (digit == -1) return -1;
        id = id * BASE62_SIZE + digit;
    }
    return id;
}

bool URLShortener::isValidURL(const std::string& url) {
    // Simple URL validation - check for http(s) protocol
    std::regex url_regex(R"(^https?://[^\s/$.?#].[^\s]*$)", std::regex::icase);
    return std::regex_match(url, url_regex);
}

bool URLShortener::shortenURL(const std::string& long_url, std::string& short_code) {
    // Validate URL
    if (!isValidURL(long_url)) {
        return false;
    }

    // Idempotent behavior: reuse existing active code for same long URL.
    if (db_->getActiveCodeByLongURL(long_url, short_code)) {
        return true;
    }

    // Generate collision-safe random 6-char Base62 code for new URLs.
    constexpr size_t kCodeLength = 6;
    constexpr int kMaxAttempts = 20;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        short_code = generateRandomCode(kCodeLength);
        if (db_->codeExists(short_code)) {
            continue;
        }

        int db_id = db_->insertURL(long_url, short_code);
        if (db_id != -1) {
            return true;
        }
    }

    return false;
}

std::string URLShortener::generateRandomCode(size_t length) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(BASE62_SIZE - 1));

    std::string code;
    code.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        code.push_back(BASE62_CHARS[dist(rng)]);
    }
    return code;
}

bool URLShortener::getLongURL(const std::string& short_code, std::string& long_url) {
    URLRecord record;
    if (!db_->getURLByCode(short_code, record)) {
        return false;
    }

    long_url = record.long_url;
    return true;
}

bool URLShortener::getCodeInfo(const std::string& short_code, int& clicks, 
                               std::string& created_at, std::string& last_accessed) {
    URLRecord record;
    if (!db_->getAnalytics(short_code, record)) {
        return false;
    }

    clicks = record.click_count;
    created_at = record.created_at;
    last_accessed = record.last_accessed;
    return true;
}

bool URLShortener::deleteShortURL(const std::string& short_code) {
    return db_->deleteURL(short_code);
}
