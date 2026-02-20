# Build & Configuration

## Prerequisites

### System

- C++23 compiler (GCC 13+ or Clang 17+)
- CMake >= 3.14
- pkg-config

### Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install \
    build-essential cmake pkg-config \
    libboost-system-dev \
    libpqxx-dev \
    libpq-dev \
    libsqlite3-dev
```

The following dependencies are automatically downloaded by CMake (FetchContent) if not present:
- **nlohmann/json** >= 3.2.0
- **SQLite3** (amalgamation if not installed)
- **Catch2** (testing framework)
- **hosseinmoein/DataFrame** (DataFrame library)

## Compilation

```bash
cd <project-root>
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Produced executables:
- `anodeServer` - Main server
- `dataframe_tests` - DataFrame tests
- `node_tests` - Node system tests
- `postgres_tests` - PostgreSQL tests

## PostgreSQL Configuration

### Configuration File

Create a `postgres.conf` file at the project root:

```
host=localhost
port=5432
dbname=anode
user=postgres
password=secret
```

See `postgres.conf.example` for a template.

### Starting the Server

```bash
# With configuration file
./anodeServer --postgres @../postgres.conf

# With direct connection string
./anodeServer --postgres "host=localhost port=5432 dbname=anode user=postgres password=secret"

# Without PostgreSQL (local-only mode)
./anodeServer
```

The server listens by default on `http://localhost:8080`.

## Client (Frontend)

```bash
cd src/client
npm install
npm run dev
```

The development client is accessible at `http://localhost:5173`.

## Tests

```bash
cd build

# All tests
ctest

# Individual tests
./dataframe_tests
./node_tests
./postgres_tests
```

For PostgreSQL integration tests:

```bash
export POSTGRES_TEST_CONN="host=localhost port=5432 dbname=test user=postgres password=secret"
./postgres_tests
```
