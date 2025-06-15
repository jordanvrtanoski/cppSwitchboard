# cppSwitchboard Troubleshooting Guide

## Overview

This guide provides solutions to common issues encountered when using cppSwitchboard, debugging techniques, and troubleshooting procedures for production environments.

## Table of Contents

- [Common Issues](#common-issues)
- [Build and Compilation Issues](#build-and-compilation-issues)
- [Runtime Issues](#runtime-issues)
- [Configuration Issues](#configuration-issues)
- [Performance Issues](#performance-issues)
- [SSL/TLS Issues](#ssltls-issues)
- [Debugging Techniques](#debugging-techniques)
- [Logging and Monitoring](#logging-and-monitoring)
- [Memory Issues](#memory-issues)
- [Network Issues](#network-issues)
- [Getting Help](#getting-help)

## Common Issues

### Server Won't Start

**Symptoms:**
- Application exits immediately after startup
- "Address already in use" error
- Permission denied errors

**Solutions:**

1. **Port Already in Use:**
```bash
# Check what's using the port
sudo netstat -tlnp | grep :8080
sudo lsof -i :8080

# Kill the process using the port
sudo kill -9 <PID>

# Or use a different port in configuration
```

2. **Permission Issues:**
```bash
# For ports < 1024, run as root or use capabilities
sudo setcap 'cap_net_bind_service=+ep' /path/to/your/app

# Or run on port > 1024 and use reverse proxy
```

3. **Configuration File Issues:**
```bash
# Validate YAML syntax
python3 -c "import yaml; yaml.safe_load(open('config.yaml'))"

# Check file permissions
ls -la config.yaml
```

### High Memory Usage

**Symptoms:**
- Gradual memory increase over time
- Out of memory errors
- System becomes unresponsive

**Solutions:**

1. **Enable Memory Debugging:**
```cpp
// Compile with debug symbols
g++ -g -O0 -fsanitize=address your_app.cpp

// Use Valgrind
valgrind --leak-check=full --track-origins=yes ./your_app
```

2. **Monitor Memory Usage:**
```bash
# Real-time memory monitoring
watch -n 1 'ps aux | grep your_app'

# Detailed memory analysis
pmap -d <PID>
```

3. **Configuration Tuning:**
```yaml
general:
  maxConnections: 1000  # Reduce if too high
  workerThreads: 4      # Match CPU cores
  requestTimeout: 10    # Prevent hanging connections
```

### SSL/TLS Connection Failures

**Symptoms:**
- SSL handshake failures
- Certificate validation errors
- Connection timeouts on HTTPS

**Solutions:**

1. **Certificate Validation:**
```bash
# Check certificate validity
openssl x509 -in certificate.crt -text -noout

# Verify certificate chain
openssl verify -CAfile ca-bundle.crt certificate.crt

# Test SSL connection
openssl s_client -connect localhost:443 -servername yourdomain.com
```

2. **Common Certificate Issues:**
```yaml
ssl:
  # Ensure correct file paths
  certificateFile: "/path/to/cert.pem"
  privateKeyFile: "/path/to/private.key"
  
  # Include intermediate certificates
  certificateChainFile: "/path/to/chain.pem"
  
  # Use modern cipher suites
  cipherSuite: "ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM"
  protocols: ["TLSv1.2", "TLSv1.3"]
```

## Build and Compilation Issues

### Missing Dependencies

**Error:** `fatal error: nghttp2/nghttp2.h: No such file or directory`

**Solution:**
```bash
# Ubuntu/Debian
sudo apt-get install libnghttp2-dev libssl-dev libyaml-cpp-dev libboost-system-dev

# CentOS/RHEL
sudo yum install nghttp2-devel openssl-devel yaml-cpp-devel boost-system-devel

# macOS
brew install nghttp2 openssl yaml-cpp boost
```

### CMake Configuration Issues

**Error:** `Could not find a package configuration file provided by "yaml-cpp"`

**Solution:**
```bash
# Install yaml-cpp development package
sudo apt-get install libyaml-cpp-dev

# Or specify custom installation path
cmake -DCMAKE_PREFIX_PATH=/usr/local ..
```

### Linker Errors

**Error:** `undefined reference to 'nghttp2_session_server_new'`

**Solution:**
```bash
# Ensure proper linking order
g++ -o myapp main.cpp -lcppSwitchboard -lnghttp2 -lssl -lcrypto -lyaml-cpp -lboost_system

# Or use pkg-config
g++ -o myapp main.cpp `pkg-config --cflags --libs cppSwitchboard`
```

## Runtime Issues

### Request Handling Failures

**Symptoms:**
- 500 Internal Server Error responses
- Handler exceptions
- Incomplete responses

**Debugging Steps:**

1. **Enable Debug Logging:**
```cpp
DebugLoggingConfig debugConfig;
debugConfig.enabled = true;
debugConfig.headers.enabled = true;
debugConfig.payload.enabled = true;
debugConfig.outputFile = "/tmp/debug.log";

DebugLogger logger(debugConfig);
```

2. **Check Handler Implementation:**
```cpp
// Ensure proper exception handling
server->get("/test", [](const HttpRequest& request) -> HttpResponse {
    try {
        // Your handler logic
        return HttpResponse::ok("Success");
    } catch (const std::exception& e) {
        std::cerr << "Handler error: " << e.what() << std::endl;
        return HttpResponse::internalServerError("Handler error");
    }
});
```

3. **Validate Request Data:**
```cpp
// Check for required headers/parameters
if (request.getHeader("Content-Type").empty()) {
    return HttpResponse::badRequest("Content-Type header required");
}

if (request.getBody().empty()) {
    return HttpResponse::badRequest("Request body required");
}
```

### Connection Issues

**Symptoms:**
- Timeouts on client connections
- Connections refused
- Slow response times

**Solutions:**

1. **Connection Pool Tuning:**
```yaml
general:
  maxConnections: 5000
  requestTimeout: 30
  keepAliveTimeout: 60
  workerThreads: 8
```

2. **Network Debugging:**
```bash
# Check network connectivity
telnet localhost 8080
curl -v http://localhost:8080/health

# Monitor network traffic
sudo tcpdump -i any -n port 8080
```

3. **File Descriptor Limits:**
```bash
# Check current limits
ulimit -n

# Increase limits (in /etc/security/limits.conf)
* soft nofile 65536
* hard nofile 65536
```

## Configuration Issues

### YAML Parsing Errors

**Error:** `YAML parsing error at line 23: expected key`

**Solutions:**

1. **Validate YAML Syntax:**
```bash
# Use online YAML validator or
python3 -c "import yaml; print(yaml.safe_load(open('config.yaml')))"
```

2. **Common YAML Issues:**
```yaml
# Incorrect indentation
http1:
  enabled: true
port: 8080  # Wrong indentation

# Correct indentation
http1:
  enabled: true
  port: 8080

# Missing quotes for special characters
password: "my@password!"  # Use quotes for special chars
```

### Environment Variable Substitution

**Issue:** Environment variables not being substituted

**Solution:**
```yaml
# Ensure proper syntax
database:
  host: "${DB_HOST}"      # Correct
  port: $DB_PORT          # Also correct
  name: "{DB_NAME}"       # Incorrect - missing $
```

```bash
# Verify environment variables are set
echo $DB_HOST
env | grep DB_
```

## Performance Issues

### High CPU Usage

**Symptoms:**
- Server becomes unresponsive
- High CPU utilization
- Slow response times

**Solutions:**

1. **Profiling:**
```bash
# CPU profiling with perf
perf record -g ./your_app
perf report

# Or use gprof
g++ -pg -o your_app main.cpp
./your_app
gprof your_app gmon.out > profile.txt
```

2. **Thread Pool Optimization:**
```yaml
general:
  workerThreads: 4  # Match CPU cores
  maxConnections: 1000  # Reduce if too high
```

3. **Request Processing:**
```cpp
// Avoid expensive operations in handlers
server->get("/data", [](const HttpRequest& request) -> HttpResponse {
    // Use connection pooling for database
    // Cache frequently accessed data
    // Implement async processing for heavy operations
    return HttpResponse::ok("Data");
});
```

### Memory Leaks

**Detection:**
```bash
# Compile with AddressSanitizer
g++ -fsanitize=address -g -o your_app main.cpp

# Use Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./your_app
```

**Common Causes:**
- Circular references in shared_ptr
- Not properly closing file handles
- Memory allocated in handlers not freed

## Debugging Techniques

### Using GDB

```bash
# Compile with debug symbols
g++ -g -O0 -o your_app main.cpp

# Run with GDB
gdb ./your_app
(gdb) set args --config production.yaml
(gdb) run
(gdb) bt  # Backtrace when crash occurs
(gdb) info locals  # Show local variables
```

### Debug Logging

```cpp
#ifdef DEBUG
    std::cout << "Processing request: " << request.getPath() << std::endl;
    std::cout << "Headers: " << request.getHeaders().size() << std::endl;
#endif
```

### Core Dump Analysis

```bash
# Enable core dumps
ulimit -c unlimited

# Analyze core dump
gdb ./your_app core
(gdb) bt
(gdb) info registers
(gdb) x/10i $pc  # Examine instructions
```

## Logging and Monitoring

### Log Analysis

```bash
# Monitor logs in real-time
tail -f /var/log/myapp/server.log

# Search for errors
grep -i error /var/log/myapp/server.log

# Analyze log patterns
awk '/ERROR/ {print $1, $2, $NF}' /var/log/myapp/server.log
```

### Metrics Collection

```cpp
// Custom metrics
#include <prometheus/counter.h>
#include <prometheus/histogram.h>

auto& request_counter = prometheus::BuildCounter()
    .Name("http_requests_total")
    .Help("Total HTTP requests")
    .Register(registry);

auto& response_time = prometheus::BuildHistogram()
    .Name("http_request_duration_seconds")
    .Help("HTTP request duration")
    .Register(registry);
```

## Memory Issues

### Memory Debugging Tools

```bash
# AddressSanitizer
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
./your_app

# Valgrind
valgrind --tool=memcheck --leak-check=full ./your_app

# Heaptrack
heaptrack ./your_app
heaptrack_gui heaptrack.your_app.1234.gz
```

### Memory Optimization

```cpp
// Use object pools for frequently allocated objects
class ObjectPool {
    std::vector<std::unique_ptr<Object>> pool;
    std::mutex mutex;
    
public:
    std::unique_ptr<Object> acquire() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pool.empty()) {
            auto obj = std::move(pool.back());
            pool.pop_back();
            return obj;
        }
        return std::make_unique<Object>();
    }
    
    void release(std::unique_ptr<Object> obj) {
        std::lock_guard<std::mutex> lock(mutex);
        pool.push_back(std::move(obj));
    }
};
```

## Network Issues

### Connection Debugging

```bash
# Test basic connectivity
telnet localhost 8080

# HTTP request testing
curl -v -X GET http://localhost:8080/health

# SSL testing
curl -v -k https://localhost:8443/health

# Connection tracing
strace -e trace=network ./your_app
```

### Firewall Issues

```bash
# Check firewall rules
sudo iptables -L -n
sudo ufw status

# Open required ports
sudo ufw allow 8080/tcp
sudo ufw allow 8443/tcp
```

### DNS Issues

```bash
# Test DNS resolution
nslookup yourdomain.com
dig yourdomain.com

# Check /etc/hosts
cat /etc/hosts
```

## Getting Help

### Information to Provide

When seeking help, include:

1. **Version Information:**
```bash
./your_app --version
g++ --version
cmake --version
```

2. **System Information:**
```bash
uname -a
lsb_release -a  # Linux
cat /etc/os-release
```

3. **Build Information:**
```bash
# CMake configuration
cmake --system-information

# Compiler flags used
echo $CXXFLAGS
```

4. **Runtime Environment:**
```bash
# Environment variables
env | grep -E "(PATH|LD_LIBRARY_PATH|PKG_CONFIG_PATH)"

# Shared libraries
ldd ./your_app
```

5. **Configuration:**
```yaml
# Sanitized configuration (remove sensitive data)
# Include relevant sections
```

6. **Logs:**
```
# Include relevant log excerpts
# Enable debug logging if needed
```

### Community Resources

- **GitHub Issues:** Report bugs and request features
- **Documentation:** Check the latest documentation
- **Stack Overflow:** Tag questions with `cppswitchboard`
- **Discord/Slack:** Join the community chat

### Creating Minimal Reproducible Examples

```cpp
// Minimal example that demonstrates the issue
#include <cppSwitchboard/http_server.h>

int main() {
    cppSwitchboard::ServerConfig config;
    config.http1.enabled = true;
    config.http1.port = 8080;
    
    auto server = cppSwitchboard::HttpServer::create(config);
    
    server->get("/test", [](const cppSwitchboard::HttpRequest& request) {
        // Minimal handler that reproduces the issue
        return cppSwitchboard::HttpResponse::ok("Test");
    });
    
    server->start();
    return 0;
}
```

## Emergency Procedures

### Server Crash Recovery

1. **Immediate Actions:**
```bash
# Check if process is running
ps aux | grep your_app

# Restart service
sudo systemctl restart myapp

# Check logs
journalctl -u myapp -f
```

2. **Root Cause Analysis:**
```bash
# Check core dumps
ls -la /var/lib/systemd/coredump/
sudo coredumpctl list
sudo coredumpctl debug <dump-id>

# Analyze logs
grep -i "segfault\|abort\|crash" /var/log/syslog
```

3. **Temporary Workarounds:**
```bash
# Reduce load
# Enable maintenance mode
# Route traffic to backup servers
```

### Data Corruption

1. **Stop Service Immediately:**
```bash
sudo systemctl stop myapp
```

2. **Assess Damage:**
```bash
# Check data integrity
# Verify backups
# Estimate recovery time
```

3. **Recovery Steps:**
```bash
# Restore from backup
# Verify data consistency
# Gradual service restoration
```

This troubleshooting guide should help you identify and resolve common issues with cppSwitchboard applications. For persistent issues, consider enabling debug logging and profiling tools to gather more detailed information. 