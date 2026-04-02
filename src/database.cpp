#include "database.h"
#include <sqlite3.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>

Database::Database(const std::string& db_path) 
    : db_(nullptr), db_path_(db_path) {}

Database::~Database() {
    close();
}

std::string Database::getCurrentTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool Database::initialize() {
    std::lock_guard<std::mutex> guard(db_lock_);
    
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Create table
    const char* create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS urls (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            long_url TEXT NOT NULL,
            short_code TEXT UNIQUE NOT NULL,
            click_count INTEGER DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_accessed TIMESTAMP
        );
    )";

    char* err_msg = nullptr;
    rc = sqlite3_exec(db_, create_table_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

int Database::insertURL(const std::string& long_url, const std::string& short_code) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return -1;

    std::string created_at = getCurrentTimestamp();

    std::string sql = "INSERT INTO urls (long_url, short_code, created_at) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_text(stmt, 1, long_url.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, short_code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, created_at.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to insert URL: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

bool Database::getURLByCode(const std::string& short_code, URLRecord& record) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return false;

    std::string sql = "SELECT id, long_url, short_code, click_count, created_at, last_accessed FROM urls WHERE short_code = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, short_code.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        record.id = sqlite3_column_int(stmt, 0);
        record.long_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.short_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.click_count = sqlite3_column_int(stmt, 3);
        record.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            record.last_accessed = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        } else {
            record.last_accessed = "";
        }

        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

bool Database::getURLByID(int id, URLRecord& record) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return false;

    std::string sql = "SELECT id, long_url, short_code, click_count, created_at, last_accessed FROM urls WHERE id = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        record.id = sqlite3_column_int(stmt, 0);
        record.long_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.short_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.click_count = sqlite3_column_int(stmt, 3);
        record.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            record.last_accessed = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        } else {
            record.last_accessed = "";
        }

        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

bool Database::getActiveCodeByLongURL(const std::string& long_url, std::string& short_code) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return false;

    std::string sql =
        "SELECT short_code FROM urls "
        "WHERE long_url = ? "
        "ORDER BY id ASC LIMIT 1;";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, long_url.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            short_code = reinterpret_cast<const char*>(text);
            sqlite3_finalize(stmt);
            return true;
        }
    }

    sqlite3_finalize(stmt);
    return false;
}

bool Database::incrementClickCount(const std::string& short_code, std::string& long_url) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return false;

    // Fetch long URL inside the same critical section to avoid nested locking.
    {
        std::string fetch_sql = "SELECT long_url FROM urls WHERE short_code = ?;";
        sqlite3_stmt* fetch_stmt;

        int fetch_rc = sqlite3_prepare_v2(db_, fetch_sql.c_str(), -1, &fetch_stmt, nullptr);
        if (fetch_rc != SQLITE_OK) return false;

        sqlite3_bind_text(fetch_stmt, 1, short_code.c_str(), -1, SQLITE_STATIC);
        fetch_rc = sqlite3_step(fetch_stmt);

        if (fetch_rc == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(fetch_stmt, 0);
            long_url = text ? reinterpret_cast<const char*>(text) : "";
        } else {
            sqlite3_finalize(fetch_stmt);
            return false;
        }

        sqlite3_finalize(fetch_stmt);
    }

    std::string now = getCurrentTimestamp();
    std::string sql = "UPDATE urls SET click_count = click_count + 1, last_accessed = ? WHERE short_code = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, short_code.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::getAnalytics(const std::string& short_code, URLRecord& record) {
    return getURLByCode(short_code, record);
}

bool Database::deleteURL(const std::string& short_code) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return false;

    std::string sql = "DELETE FROM urls WHERE short_code = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, short_code.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::codeExists(const std::string& short_code) {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return false;

    std::string sql = "SELECT 1 FROM urls WHERE short_code = ? LIMIT 1;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, short_code.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_ROW;
}

int Database::getTotalURLCount() {
    std::lock_guard<std::mutex> guard(db_lock_);

    if (!db_) return 0;

    std::string sql = "SELECT COUNT(*) FROM urls;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 0;

    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

void Database::close() {
    std::lock_guard<std::mutex> guard(db_lock_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
