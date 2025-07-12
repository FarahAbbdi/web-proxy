# HTTP Proxy Server

A simple caching web proxy for HTTP/1.1 requests

## Features

- Basic HTTP proxy forwarding
- Simple caching (10 entries, 100KB each)
- Smart caching with Cache-Control header support
- Cache expiration handling

## Build

```bash
make htproxy
```

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
