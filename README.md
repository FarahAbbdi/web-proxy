# Web Proxy

## Overview
Implemented a **caching HTTP/1.1 web proxy** in C. Focused on socket programming, caching, and HTTP compliance.

## Implementation Stages
- **Stage 1 – Simple Proxy:** Set up TCP sockets, forwarded HTTP `GET` requests, streamed responses back to clients.
- **Stage 2 – Naive Caching:** Added fixed-size LRU cache (10 entries, 100 KiB each).
- **Stage 3 – Valid Caching:** Respected `Cache-Control` directives (e.g., `no-cache`, `private`).
- **Stage 4 – Expiration:** Implemented `max-age` handling and stale entry detection.
  
## Testing
- **curl:** Verified proxy forwarding, caching, and header compliance.
- **Provided server & public HTTP sites:** Tested correctness under real-world conditions.
- **CI Integration:** Automated builds and regression testing with GitHub Actions.
- **Valgrind:** Ensured memory safety and absence of leaks.

## Key Features
- **Proxying:** Forwarded client requests, streamed large responses safely.
- **Caching:** Byte-level key matching, eviction policy, cache hits/misses logged.
- **Compliance:** Properly handled `Cache-Control`, expiration, stale entries.
- **Robustness:** Supported IPv4/IPv6, large payloads, graceful error handling.

## Tools & Practices
- **Languages:** C / Rust (POSIX sockets)
- **Build:** Makefile, GitHub CI
- **Testing Tools:** curl, telnet, Valgrind
- **Practices:** Version control, modular code, logging, error handling

## Usage

```bash
./htproxy -p <port> [-c]
```

- `-p <port>`: Port number to listen on
- `-c`: Enable caching (optional)

## Quick Start

```bash
# Build the proxy
make htproxy

# Start proxy with caching
./htproxy -p 8080 -c

# In another terminal, test with curl
curl --proxy http://localhost:8080 http://example.com
```

## Testing Examples

### Basic Functionality Test

```bash
# Terminal 1: Start proxy
./htproxy -p 8080 -c

# Terminal 2: Make requests
curl -x localhost:8080 http://example.com
curl -x localhost:8080 http://info.cern.ch
curl -x localhost:8080 http://detectportal.firefox.com
```

### Cache Testing

```bash
# First request (cache miss)
curl -x localhost:8080 http://example.com

# Second request (cache hit)
curl -x localhost:8080 http://example.com
```

### Speed Benchmarking
You can measure the performance difference between cached and non-cached responses:

```bash
# First request (miss, slower)
command time -p curl -s -x http://localhost:8080 http://example.com > /dev/null

# Second request (hit, faster)
command time -p curl -s -x http://localhost:8080 http://example.com > /dev/null
```

**Example Output**
```bash
Miss: real 0.83s
Hit:  real 0.01s
≈98.8% faster on repeated requests
```

## Log Output

```
Accepted                                    # New connection
Request tail last line                      # Last header line
GETting example.com /                       # Forwarding request
Response body length 1256                   # Response size
Serving example.com / from cache            # Cache hit
Evicting old.com /path from cache          # LRU eviction
Not caching example.com /nocache           # Cache-Control blocked
Stale entry for example.com /old           # Expired cache
```

## Cache Rules

Responses are **not cached** if they contain:

- `Cache-Control: private`
- `Cache-Control: no-store`
- `Cache-Control: no-cache`
- `Cache-Control: max-age=0`
- `Cache-Control: must-revalidate`
- `Cache-Control: proxy-revalidate`

## Credits

**COMP30023 - Computer Systems**
University of Melbourne

**Team Members :**
- Adam Eldaly
- Farah Abdi
