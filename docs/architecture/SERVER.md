# HTTP Server Architecture

## Overview

AnodeServer uses Boost.Beast for HTTP protocol handling on top of Boost.Asio for asynchronous I/O. The server is designed for high concurrency with a single-threaded event loop.

## File Structure

```
src/server/
├── HttpServer.hpp/cpp      # TCP acceptor and server lifecycle
├── HttpSession.hpp/cpp     # HTTP request/response handling
├── RequestHandler.hpp/cpp  # Business logic and query execution
├── Logger.hpp/cpp          # Centralized logging with request correlation
└── Profiler.hpp/cpp        # Performance measurement and statistics
```

## Component Diagram

```
┌────────────────────────────────────────────────────────────┐
│                       main.cpp                              │
│  • Parse CLI arguments                                      │
│  • Initialize RequestHandler with dataset                  │
│  • Create io_context and HttpServer                        │
│  • Setup signal handlers                                   │
│  • Run event loop                                          │
└──────────────────────────┬─────────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────────┐
│                      HttpServer                             │
│────────────────────────────────────────────────────────────│
│  - m_acceptor: tcp::acceptor                               │
│  - m_ioc: io_context&                                      │
│────────────────────────────────────────────────────────────│
│  + run()      → Start accepting connections                │
│  + stop()     → Close acceptor                             │
│  - doAccept() → Async accept loop                          │
└──────────────────────────┬─────────────────────────────────┘
                           │ creates
                           ▼
┌────────────────────────────────────────────────────────────┐
│                      HttpSession                            │
│────────────────────────────────────────────────────────────│
│  - m_stream: beast::tcp_stream                             │
│  - m_buffer: beast::flat_buffer                            │
│  - m_request: http::request<string_body>                   │
│────────────────────────────────────────────────────────────│
│  + run()           → Start session                         │
│  - doRead()        → Async read request                    │
│  - onRead()        → Process request                       │
│  - sendResponse()  → Async write response                  │
│  - onWrite()       → Handle write completion               │
│  - doClose()       → Graceful shutdown                     │
│  - handleRequest() → Route to handler                      │
└──────────────────────────┬─────────────────────────────────┘
                           │ calls
                           ▼
┌────────────────────────────────────────────────────────────┐
│                    RequestHandler                           │
│────────────────────────────────────────────────────────────│
│  - m_dataset: DataFramePtr (singleton)                     │
│  - m_datasetPath: string                                   │
│  - m_originalRows: size_t                                  │
│────────────────────────────────────────────────────────────│
│  + instance()         → Singleton access                   │
│  + loadDataset()      → Load CSV into DataFrame            │
│  + handleHealth()     → /api/health                        │
│  + handleDatasetInfo()→ /api/dataset/info                  │
│  + handleQuery()      → /api/dataset/query                 │
│  - executeOperation() → Run single operation               │
└────────────────────────────────────────────────────────────┘
```

## Request Flow

```
1. TCP Connection
   │
   ▼
2. HttpServer::doAccept()
   │  Creates HttpSession with socket
   ▼
3. HttpSession::run()
   │  Dispatches to executor
   ▼
4. HttpSession::doRead()
   │  beast::http::async_read()
   ▼
5. HttpSession::onRead()
   │  Check for errors/EOF
   ▼
6. HttpSession::handleRequest()
   │  Route based on method + target
   │  ├─ OPTIONS → CORS preflight response
   │  ├─ GET /api/health → handleHealth()
   │  ├─ GET /api/dataset/info → handleDatasetInfo()
   │  ├─ POST /api/dataset/query → handleQuery()
   │  ├─ GET /api/nodes → handleListNodes()
   │  ├─ GET /api/graphs → handleListGraphs()
   │  ├─ POST /api/graph → handleCreateGraph()
   │  ├─ GET /api/graph/:slug → handleGetGraph()
   │  ├─ PUT /api/graph/:slug → handleUpdateGraph()
   │  ├─ DELETE /api/graph/:slug → handleDeleteGraph()
   │  ├─ GET /api/graph/:slug/versions → handleGetGraphVersions()
   │  └─ POST /api/graph/:slug/execute → handleExecuteGraph()
   ▼
7. HttpSession::sendResponse()
   │  beast::http::async_write()
   │  IMPORTANT: Response kept alive via shared_ptr in lambda
   ▼
8. HttpSession::onWrite()
   │  ├─ need_eof → doClose()
   │  └─ keep-alive → doRead() (loop)
```

## Session Lifecycle

The session uses `std::enable_shared_from_this` to ensure the session object stays alive during async operations:

```cpp
class HttpSession : public std::enable_shared_from_this<HttpSession> {
    void sendResponse(http::response<http::string_body> response) {
        // Response must be kept alive until write completes
        auto sp = std::make_shared<http::response<...>>(std::move(response));
        bool needEof = sp->need_eof();

        http::async_write(
            m_stream,
            *sp,
            [self = shared_from_this(), sp, needEof](...) {
                // sp captured → response stays alive
                // self captured → session stays alive
                self->onWrite(needEof, ec, bytes);
            });
    }
};
```

## CORS Support

Cross-Origin Resource Sharing is enabled for all origins:

```cpp
// Response headers
res.set(http::field::access_control_allow_origin, "*");
res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
res.set(http::field::access_control_allow_headers, "Content-Type");

// OPTIONS preflight handler
if (req.method() == http::verb::options) {
    // Return 200 OK with CORS headers
}
```

## Error Handling

| Error Type | Handling |
|------------|----------|
| Connection error | Log and close session |
| Parse error (JSON) | Return 400 Bad Request |
| Unknown endpoint | Return 404 Not Found |
| Operation error | Return 500 with error message |
| No dataset loaded | Return 503 Service Unavailable |

## Configuration

Command-line arguments:

```bash
./anodeServer [options]
  -p, --port PORT        # Port to listen on (default: 8080)
  -a, --address ADDR     # Address to bind to (default: 0.0.0.0)
  -d, --dataset PATH     # Path to CSV dataset
  -g, --graphs-db PATH   # Path to graphs SQLite database (default: ./graphs.db)
  -l, --log-level LVL    # Log level: debug, info, warn, error (default: info)
  --no-profiler          # Disable profiler
  -h, --help             # Show help
```

## Threading Model

The server uses a single-threaded model with async I/O:

```
┌─────────────────────────────────────────┐
│            io_context.run()              │
│  ┌─────────────────────────────────────┐│
│  │         Event Loop                   ││
│  │  • Accept completion handlers       ││
│  │  • Read completion handlers         ││
│  │  • Write completion handlers        ││
│  │  • Timer handlers                   ││
│  └─────────────────────────────────────┘│
└─────────────────────────────────────────┘
         Single thread, no locks
```

Benefits:
- No mutex contention
- Predictable performance
- Simple debugging

## Graceful Shutdown

```cpp
// Signal handler setup
std::signal(SIGINT, signal_handler);
std::signal(SIGTERM, signal_handler);

// Shutdown sequence
shutdown_handler = [&]() {
    server.stop();   // Close acceptor
    ioc.stop();      // Stop event loop
};
```

## Logger

Centralized logging with request ID correlation for tracing requests through the system.

### Log Levels
- `DEBUG` - Detailed information for debugging
- `INFO` - General operational information (default)
- `WARN` - Warning conditions
- `ERROR` - Error conditions

### Features
- Timestamp with milliseconds
- Request ID correlation between request and response logs
- Response time measurement
- Body size formatting (o, ko, Mo)
- Automatic body truncation for large payloads

### Usage
```cpp
#include "server/Logger.hpp"

LOG_INFO("Server started");
LOG_DEBUG("Processing query");
LOG_ERROR("Failed to parse JSON");

// Request/Response logging with correlation
uint64_t reqId = Logger::instance().logRequest("POST", "/api/query", body);
// ... process request ...
Logger::instance().logResponse(reqId, 200, responseBody, responseSize);
```

### Output Example
```
[2025-12-10 16:11:32.510] [INFO ] [REQ-2] POST /api/dataset/query | Body: {"operations":[...]}
[2025-12-10 16:11:32.598] [INFO ] [REQ-2] RESPONSE 200 | Size: 1.51 Mo | Time: 87.9ms
```

## Profiler

Performance measurement tool that tracks execution times for operations.

### Features
- Named timers with automatic aggregation
- Statistics: count, total, avg, min, max
- RAII-based ScopedTimer for automatic timing
- Formatted stats output on shutdown

### Usage
```cpp
#include "server/Profiler.hpp"

// RAII timer - automatically stops when scope ends
{
    ScopedTimer timer("handleQuery");
    // ... code to measure ...
}

// Manual timer
size_t id = Profiler::instance().start("operation");
// ... code ...
double ms = Profiler::instance().stop(id);

// Get all stats
std::cout << Profiler::instance().formatStats();
```

### Output Example
```
========== PROFILER STATS ==========
Operation                          Count   Total(ms)    Avg(ms)    Min(ms)    Max(ms)
----------------------------------------------------------------------------------------
handleQuery                          150     1234.56       8.23       2.10      45.30
op:pivot                              50      890.12      17.80       5.20      89.00
op:groupbytree                        50      234.56       4.69       1.50      12.30
loadDataset                            1      156.00     156.00     156.00     156.00
=====================================
```
