# URL Shortener (C++)

Simple URL Shortener backend project in C++ for students.

## Tech Stack
- C++17
- Crow (HTTP server)
- SQLite3 (database)
- nlohmann/json (JSON)
- Custom LRU Cache (`unordered_map` + `list`)
- CMake

## Project Structure

```text
url-shortener/
├── src/
│   ├── main.cpp
│   ├── server.cpp
│   ├── database.cpp
│   ├── cache.cpp
│   ├── shortener.cpp
│   ├── rate_limiter.cpp
├── include/
│   ├── server.h
│   ├── database.h
│   ├── cache.h
│   ├── shortener.h
│   ├── rate_limiter.h
├── config/
│   └── config.json
├── logs/
├── CMakeLists.txt
└── README.md
```

## Features
1. `POST /shorten` creates short URL
2. `GET /{code}` redirects to original URL (302)
3. `GET /analytics/{code}` returns clicks, created time, last accessed
4. URL expiry support (`expiry_days`)
5. Rate limiting (`100 requests per IP per minute`)
6. In-memory LRU cache
7. SQLite database storage
8. Logging to `logs/server.log`
9. Config file support
10. Modular code structure
11. CMake build

## Database Schema

Table: `urls`
- `id INTEGER PRIMARY KEY AUTOINCREMENT`
- `long_url TEXT NOT NULL`
- `short_code TEXT UNIQUE`
- `click_count INTEGER DEFAULT 0`
- `created_at TIMESTAMP`
- `last_accessed TIMESTAMP`
- `expiry TIMESTAMP`

## API

### 1) Create short URL
`POST /shorten`

Request:
```json
{
  "url": "https://example.com/long/url",
  "expiry_days": 30
}
```

Response:
```json
{
  "short_url": "http://localhost:8080/abc123"
}
```

### 2) Redirect
`GET /{code}`
- Returns `302` redirect to long URL

### 3) Analytics
`GET /analytics/{code}`

Response:
```json
{
  "clicks": 10,
  "last_accessed": "timestamp",
  "created_at": "timestamp"
}
```

## Config

File: `config/config.json`

```json
{
  "port": 8080,
  "cache_size": 1000,
  "rate_limit": 100
}
```

## Build and Run

### 1) Install dependencies (Ubuntu)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsqlite3-dev libboost-system-dev nlohmann-json3-dev
```

### 2) Prepare Crow
Place Crow headers so this include works:
- `#include "crow/crow_all.h"`

Example:
- `third_party/crow/include/crow/crow_all.h`

### 3) Build
```bash
mkdir -p build
cd build
cmake ..
make
```

### 4) Run
```bash
./url-shortener
```

Server starts on `http://localhost:8080`.

## Quick Test

Create short URL:
```bash
curl -X POST http://localhost:8080/shorten \
  -H "Content-Type: application/json" \
  -d '{"url":"https://example.com/long/url","expiry_days":30}'
```

Get analytics:
```bash
curl http://localhost:8080/analytics/<code>
```
