# cppSwitchboard Production Deployment Guide

## Overview

This guide covers deploying cppSwitchboard applications to production environments, including configuration, security, monitoring, scaling, and maintenance best practices.

## Table of Contents

- [Pre-deployment Checklist](#pre-deployment-checklist)
- [Server Configuration](#server-configuration)
- [Security Hardening](#security-hardening)
- [Load Balancing](#load-balancing)
- [SSL/TLS Configuration](#ssltls-configuration)
- [Monitoring and Logging](#monitoring-and-logging)
- [Performance Tuning](#performance-tuning)
- [Deployment Strategies](#deployment-strategies)
- [Container Deployment](#container-deployment)
- [Troubleshooting](#troubleshooting)

## Pre-deployment Checklist

### Code Quality
- [ ] All tests passing (unit, integration, load tests)
- [ ] Code coverage meets requirements (>80%)
- [ ] Static analysis clean (no critical/high issues)
- [ ] Security scan completed
- [ ] Performance benchmarks meet requirements

### Configuration
- [ ] Production configuration files reviewed
- [ ] Environment variables properly set
- [ ] SSL certificates valid and configured
- [ ] Database connections tested
- [ ] External service endpoints verified

### Infrastructure
- [ ] Server resources allocated (CPU, memory, disk)
- [ ] Network security groups configured
- [ ] Load balancers configured
- [ ] Monitoring systems set up
- [ ] Backup procedures in place

### Documentation
- [ ] Deployment runbook created
- [ ] Rollback procedures documented
- [ ] Emergency contacts identified
- [ ] Configuration documented

## Server Configuration

### Production Server YAML Configuration

```yaml
# production.yaml
application:
  name: "MyApp Production"
  version: "1.2.3"
  environment: "production"

http1:
  enabled: true
  port: 8080
  bindAddress: "0.0.0.0"
  maxConnections: 10000
  keepAliveTimeout: 60

http2:
  enabled: true
  port: 8443
  bindAddress: "0.0.0.0"
  maxConnections: 10000
  maxConcurrentStreams: 100

ssl:
  enabled: true
  certificateFile: "/etc/ssl/certs/app.crt"
  privateKeyFile: "/etc/ssl/private/app.key"
  certificateChainFile: "/etc/ssl/certs/app-chain.crt"
  cipherSuite: "ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20:!aNULL:!MD5:!DSS"
  protocols: ["TLSv1.2", "TLSv1.3"]

general:
  maxConnections: 10000
  requestTimeout: 30
  enableLogging: true
  logLevel: "info"
  workerThreads: 16
  requestBodyMaxSize: 10485760  # 10MB

monitoring:
  debugLogging:
    enabled: false  # Disable in production for performance
    outputFile: "/var/log/app/debug.log"
    headers:
      enabled: false
    payload:
      enabled: false

  healthCheck:
    enabled: true
    endpoint: "/health"
    interval: 30

  metrics:
    enabled: true
    endpoint: "/metrics"
    port: 9090

security:
  cors:
    enabled: true
    allowedOrigins: ["https://yourdomain.com", "https://app.yourdomain.com"]
    allowedMethods: ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
    allowedHeaders: ["Content-Type", "Authorization"]
    maxAge: 86400

  rateLimit:
    enabled: true
    requestsPerMinute: 1000
    burstSize: 100

  headers:
    serverTokens: false
    xFrameOptions: "SAMEORIGIN"
    xContentTypeOptions: "nosniff"
    xXSSProtection: "1; mode=block"
    strictTransportSecurity: "max-age=31536000; includeSubDomains"

database:
  host: "${DB_HOST}"
  port: 5432
  name: "${DB_NAME}"
  username: "${DB_USER}"
  password: "${DB_PASSWORD}"
  connectionPool:
    minConnections: 5
    maxConnections: 20
    maxIdleTime: 300

cache:
  redis:
    enabled: true
    host: "${REDIS_HOST}"
    port: 6379
    password: "${REDIS_PASSWORD}"
    database: 0
    connectionPool:
      maxConnections: 10
```

### Environment Variables

```bash
# Production environment variables
export DB_HOST="prod-db.internal"
export DB_NAME="myapp_prod"
export DB_USER="app_user"
export DB_PASSWORD="$(cat /etc/secrets/db_password)"
export REDIS_HOST="prod-redis.internal"
export REDIS_PASSWORD="$(cat /etc/secrets/redis_password)"
export JWT_SECRET="$(cat /etc/secrets/jwt_secret)"
export API_KEY="$(cat /etc/secrets/api_key)"
export LOG_LEVEL="info"
export ENVIRONMENT="production"
```

### Systemd Service Configuration

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=MyApp HTTP Server
After=network.target
Wants=network.target

[Service]
Type=simple
User=appuser
Group=appgroup
WorkingDirectory=/opt/myapp
ExecStart=/opt/myapp/bin/myapp --config /etc/myapp/production.yaml
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=myapp

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/myapp /var/lib/myapp
CapabilityBoundingSet=CAP_NET_BIND_SERVICE

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

# Environment
Environment=NODE_ENV=production
EnvironmentFile=-/etc/myapp/environment

[Install]
WantedBy=multi-user.target
```

## Security Hardening

### Application Security

```cpp
// Security middleware configuration
class SecurityMiddleware : public Middleware {
public:
    bool process(HttpRequest& request, HttpResponse& response,
                 std::function<void()> next) override {
        
        // Add security headers
        response.setHeader("X-Frame-Options", "SAMEORIGIN");
        response.setHeader("X-Content-Type-Options", "nosniff");
        response.setHeader("X-XSS-Protection", "1; mode=block");
        response.setHeader("Referrer-Policy", "strict-origin-when-cross-origin");
        response.setHeader("Strict-Transport-Security", 
                          "max-age=31536000; includeSubDomains; preload");
        
        // Remove server identification
        response.removeHeader("Server");
        
        // Input validation
        if (!validateRequest(request)) {
            response.setStatus(400);
            response.setBody("{\"error\": \"Invalid request\"}");
            return false;
        }
        
        next();
        return true;
    }

private:
    bool validateRequest(const HttpRequest& request) {
        // Implement request validation
        std::string contentType = request.getHeader("Content-Type");
        if (contentType.find("application/json") == std::string::npos &&
            request.getMethod() != "GET") {
            return false;
        }
        
        // Check request size
        if (request.getBody().size() > 10 * 1024 * 1024) { // 10MB limit
            return false;
        }
        
        return true;
    }
};
```

### Network Security

```bash
# Firewall configuration (iptables)
#!/bin/bash

# Clear existing rules
iptables -F
iptables -X
iptables -t nat -F
iptables -t nat -X

# Default policies
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# Allow loopback
iptables -A INPUT -i lo -j ACCEPT

# Allow established connections
iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# Allow SSH (from specific IPs only)
iptables -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT

# Allow HTTP/HTTPS
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
iptables -A INPUT -p tcp --dport 443 -j ACCEPT

# Allow application ports (behind load balancer)
iptables -A INPUT -p tcp --dport 8080 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 8443 -s 10.0.0.0/8 -j ACCEPT

# Allow monitoring
iptables -A INPUT -p tcp --dport 9090 -s 10.0.0.0/8 -j ACCEPT

# Rate limiting
iptables -A INPUT -p tcp --dport 80 -m limit --limit 25/minute --limit-burst 100 -j ACCEPT
iptables -A INPUT -p tcp --dport 443 -m limit --limit 25/minute --limit-burst 100 -j ACCEPT

# Save rules
iptables-save > /etc/iptables/rules.v4
```

## Load Balancing

### Nginx Configuration

```nginx
# /etc/nginx/sites-available/myapp
upstream myapp_backend {
    least_conn;
    server 10.0.1.10:8080 max_fails=3 fail_timeout=30s;
    server 10.0.1.11:8080 max_fails=3 fail_timeout=30s;
    server 10.0.1.12:8080 max_fails=3 fail_timeout=30s;
    
    # Health check
    keepalive 32;
}

upstream myapp_ssl_backend {
    least_conn;
    server 10.0.1.10:8443 max_fails=3 fail_timeout=30s;
    server 10.0.1.11:8443 max_fails=3 fail_timeout=30s;
    server 10.0.1.12:8443 max_fails=3 fail_timeout=30s;
    
    keepalive 32;
}

# HTTP to HTTPS redirect
server {
    listen 80;
    server_name myapp.example.com;
    return 301 https://$server_name$request_uri;
}

# Main HTTPS server
server {
    listen 443 ssl http2;
    server_name myapp.example.com;
    
    # SSL Configuration
    ssl_certificate /etc/ssl/certs/myapp.crt;
    ssl_certificate_key /etc/ssl/private/myapp.key;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20:!aNULL:!MD5:!DSS;
    ssl_prefer_server_ciphers off;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 10m;
    
    # Security headers
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains; preload" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    
    # Logging
    access_log /var/log/nginx/myapp.access.log;
    error_log /var/log/nginx/myapp.error.log;
    
    # Rate limiting
    limit_req_zone $binary_remote_addr zone=api:10m rate=10r/s;
    limit_req zone=api burst=20 nodelay;
    
    # Static content
    location /static/ {
        alias /var/www/static/;
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
    
    # Health check
    location /health {
        proxy_pass http://myapp_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        access_log off;
    }
    
    # API endpoints
    location /api/ {
        proxy_pass http://myapp_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
        
        # Buffering
        proxy_buffering on;
        proxy_buffer_size 4k;
        proxy_buffers 8 4k;
        
        # WebSocket support
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
    
    # Default location
    location / {
        proxy_pass http://myapp_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## SSL/TLS Configuration

### Certificate Management

```bash
#!/bin/bash
# SSL certificate deployment script

CERT_DIR="/etc/ssl/certs"
KEY_DIR="/etc/ssl/private"
APP_NAME="myapp"

# Install certificate
sudo cp ${APP_NAME}.crt ${CERT_DIR}/
sudo cp ${APP_NAME}-chain.crt ${CERT_DIR}/
sudo cp ${APP_NAME}.key ${KEY_DIR}/

# Set permissions
sudo chown root:root ${CERT_DIR}/${APP_NAME}*.crt
sudo chown root:ssl-cert ${KEY_DIR}/${APP_NAME}.key
sudo chmod 644 ${CERT_DIR}/${APP_NAME}*.crt
sudo chmod 640 ${KEY_DIR}/${APP_NAME}.key

# Verify certificate
openssl x509 -in ${CERT_DIR}/${APP_NAME}.crt -text -noout

# Test SSL configuration
openssl s_client -connect localhost:8443 -servername myapp.example.com
```

### Automatic Certificate Renewal (Let's Encrypt)

```bash
#!/bin/bash
# /etc/cron.d/certbot-renewal

# Renew certificates monthly
0 2 1 * * root /usr/bin/certbot renew --quiet --deploy-hook "/usr/local/bin/deploy-certs.sh"
```

```bash
#!/bin/bash
# /usr/local/bin/deploy-certs.sh

# Copy renewed certificates
cp /etc/letsencrypt/live/myapp.example.com/fullchain.pem /etc/ssl/certs/myapp.crt
cp /etc/letsencrypt/live/myapp.example.com/privkey.pem /etc/ssl/private/myapp.key

# Reload services
systemctl reload nginx
systemctl reload myapp

# Verify renewal
curl -f https://myapp.example.com/health || echo "Health check failed after renewal"
```

## Monitoring and Logging

### Application Monitoring

```cpp
#include <cppSwitchboard/middleware/metrics.h>

class MetricsMiddleware : public Middleware {
public:
    bool process(HttpRequest& request, HttpResponse& response,
                 std::function<void()> next) override {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        next();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        
        // Record metrics
        recordRequestMetrics(request, response, duration.count());
        
        return true;
    }

private:
    void recordRequestMetrics(const HttpRequest& request, 
                             const HttpResponse& response,
                             long durationMs) {
        // Increment request counter
        incrementCounter("http_requests_total", {
            {"method", request.getMethod()},
            {"status", std::to_string(response.getStatus())},
            {"endpoint", sanitizeEndpoint(request.getPath())}
        });
        
        // Record duration histogram
        recordHistogram("http_request_duration_ms", durationMs, {
            {"method", request.getMethod()},
            {"endpoint", sanitizeEndpoint(request.getPath())}
        });
        
        // Record response size
        recordHistogram("http_response_size_bytes", response.getBody().size(), {
            {"method", request.getMethod()},
            {"endpoint", sanitizeEndpoint(request.getPath())}
        });
    }
    
    std::string sanitizeEndpoint(const std::string& path) {
        // Replace IDs and dynamic parts with placeholders
        std::regex idPattern(R"(/\d+)");
        return std::regex_replace(path, idPattern, "/{id}");
    }
};
```

### Logging Configuration

```yaml
# Log configuration in production.yaml
logging:
  level: "info"
  format: "json"
  outputs:
    - type: "file"
      path: "/var/log/myapp/app.log"
      maxSize: "100MB"
      maxBackups: 10
      maxAge: 30
      compress: true
    - type: "syslog"
      facility: "local0"
      tag: "myapp"
  
  # Request logging
  accessLog:
    enabled: true
    path: "/var/log/myapp/access.log"
    format: '%h %l %u %t "%r" %>s %O "%{Referer}i" "%{User-Agent}i" %D'
    
  # Error logging
  errorLog:
    enabled: true
    path: "/var/log/myapp/error.log"
    level: "error"
```

### Prometheus Metrics Endpoint

```cpp
class PrometheusHandler : public HttpHandler {
public:
    HttpResponse handle(const HttpRequest& request) override {
        std::ostringstream metrics;
        
        // System metrics
        metrics << "# HELP process_cpu_seconds_total Total user and system CPU time\n";
        metrics << "# TYPE process_cpu_seconds_total counter\n";
        metrics << "process_cpu_seconds_total " << getCPUTime() << "\n\n";
        
        metrics << "# HELP process_resident_memory_bytes Resident memory size\n";
        metrics << "# TYPE process_resident_memory_bytes gauge\n";
        metrics << "process_resident_memory_bytes " << getMemoryUsage() << "\n\n";
        
        // Application metrics
        metrics << "# HELP http_requests_total Total HTTP requests\n";
        metrics << "# TYPE http_requests_total counter\n";
        for (const auto& [labels, value] : getRequestCounters()) {
            metrics << "http_requests_total{" << labels << "} " << value << "\n";
        }
        metrics << "\n";
        
        metrics << "# HELP http_request_duration_seconds HTTP request duration\n";
        metrics << "# TYPE http_request_duration_seconds histogram\n";
        for (const auto& [labels, histogram] : getDurationHistograms()) {
            for (const auto& [bucket, count] : histogram.buckets) {
                metrics << "http_request_duration_seconds_bucket{" << labels 
                       << ",le=\"" << bucket << "\"} " << count << "\n";
            }
            metrics << "http_request_duration_seconds_sum{" << labels << "} " 
                   << histogram.sum << "\n";
            metrics << "http_request_duration_seconds_count{" << labels << "} " 
                   << histogram.count << "\n";
        }
        
        HttpResponse response(200);
        response.setHeader("Content-Type", "text/plain; version=0.0.4");
        response.setBody(metrics.str());
        return response;
    }

private:
    double getCPUTime() {
        // Implementation to get CPU time
        return 0.0;
    }
    
    size_t getMemoryUsage() {
        // Implementation to get memory usage
        return 0;
    }
    
    // ... other metric collection methods
};
```

## Performance Tuning

### Server Optimization

```yaml
# Performance-optimized configuration
general:
  workerThreads: 32  # 2x CPU cores
  ioThreads: 16      # I/O bound operations
  connectionPool:
    size: 100
    keepAlive: true
    timeout: 300
  
  bufferSize: 65536   # 64KB buffers
  sendBufferSize: 131072  # 128KB send buffer
  recvBufferSize: 131072  # 128KB receive buffer
  
  # TCP tuning
  tcpNoDelay: true
  tcpKeepAlive: true
  reusePort: true

http2:
  maxConcurrentStreams: 100
  initialWindowSize: 65536
  maxFrameSize: 16384
  headerTableSize: 4096
```

### System-level Tuning

```bash
#!/bin/bash
# System optimization script

# Increase file descriptor limits
echo "* soft nofile 65536" >> /etc/security/limits.conf
echo "* hard nofile 65536" >> /etc/security/limits.conf

# TCP tuning
echo "net.core.somaxconn = 4096" >> /etc/sysctl.conf
echo "net.core.netdev_max_backlog = 4096" >> /etc/sysctl.conf
echo "net.ipv4.tcp_max_syn_backlog = 4096" >> /etc/sysctl.conf
echo "net.ipv4.tcp_keepalive_time = 600" >> /etc/sysctl.conf
echo "net.ipv4.tcp_keepalive_intvl = 60" >> /etc/sysctl.conf
echo "net.ipv4.tcp_keepalive_probes = 3" >> /etc/sysctl.conf

# Apply changes
sysctl -p
```

## Deployment Strategies

### Blue-Green Deployment

```bash
#!/bin/bash
# Blue-green deployment script

BLUE_SERVERS=("10.0.1.10" "10.0.1.11" "10.0.1.12")
GREEN_SERVERS=("10.0.2.10" "10.0.2.11" "10.0.2.12")
LB_CONFIG="/etc/nginx/upstream.conf"

deploy_to_green() {
    echo "Deploying to green environment..."
    
    for server in "${GREEN_SERVERS[@]}"; do
        echo "Deploying to $server"
        ssh deploy@$server "
            cd /opt/myapp &&
            git pull origin main &&
            make build &&
            systemctl stop myapp &&
            systemctl start myapp &&
            sleep 10 &&
            curl -f http://localhost:8080/health
        "
    done
}

switch_traffic() {
    echo "Switching traffic to green..."
    
    # Update load balancer configuration
    sed -i 's/blue_backend/green_backend/' $LB_CONFIG
    nginx -s reload
    
    # Wait for connections to drain
    sleep 30
}

rollback() {
    echo "Rolling back to blue..."
    
    sed -i 's/green_backend/blue_backend/' $LB_CONFIG
    nginx -s reload
}

# Health check function
health_check() {
    local servers=("$@")
    for server in "${servers[@]}"; do
        if ! curl -f http://$server:8080/health; then
            echo "Health check failed for $server"
            return 1
        fi
    done
    return 0
}

# Main deployment flow
deploy_to_green

if health_check "${GREEN_SERVERS[@]}"; then
    switch_traffic
    echo "Deployment successful!"
else
    echo "Health checks failed, aborting deployment"
    exit 1
fi
```

## Container Deployment

### Dockerfile

```dockerfile
# Multi-stage build
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libnghttp2-dev \
    libssl-dev \
    libyaml-cpp-dev \
    libboost-system-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /app
COPY . .

# Build application
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Production image
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libnghttp2-14 \
    libssl3 \
    libyaml-cpp0.7 \
    libboost-system1.74.0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN groupadd -r appuser && useradd -r -g appuser appuser

# Copy application
COPY --from=builder /app/build/myapp /usr/local/bin/
COPY --from=builder /app/config/ /etc/myapp/

# Create directories
RUN mkdir -p /var/log/myapp /var/lib/myapp && \
    chown -R appuser:appuser /var/log/myapp /var/lib/myapp

# Set user
USER appuser

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Expose ports
EXPOSE 8080 8443 9090

# Start application
CMD ["/usr/local/bin/myapp", "--config", "/etc/myapp/production.yaml"]
```

### Docker Compose

```yaml
# docker-compose.prod.yml
version: '3.8'

services:
  myapp:
    build: .
    image: myapp:latest
    deploy:
      replicas: 3
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '1'
          memory: 1G
      restart_policy:
        condition: on-failure
        delay: 5s
        max_attempts: 3
    ports:
      - "8080:8080"
      - "8443:8443"
      - "9090:9090"
    environment:
      - DB_HOST=postgres
      - REDIS_HOST=redis
    volumes:
      - ./config:/etc/myapp:ro
      - ./logs:/var/log/myapp
      - ./ssl:/etc/ssl/certs:ro
    networks:
      - app-network
    depends_on:
      - postgres
      - redis

  postgres:
    image: postgres:14
    environment:
      POSTGRES_DB: myapp_prod
      POSTGRES_USER: app_user
      POSTGRES_PASSWORD_FILE: /run/secrets/db_password
    volumes:
      - postgres_data:/var/lib/postgresql/data
    secrets:
      - db_password
    networks:
      - app-network

  redis:
    image: redis:7
    command: redis-server --requirepass-file /run/secrets/redis_password
    volumes:
      - redis_data:/data
    secrets:
      - redis_password
    networks:
      - app-network

  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./ssl:/etc/ssl/certs:ro
    networks:
      - app-network
    depends_on:
      - myapp

volumes:
  postgres_data:
  redis_data:

networks:
  app-network:
    driver: overlay

secrets:
  db_password:
    external: true
  redis_password:
    external: true
```

## Troubleshooting

### Common Issues

#### High Memory Usage
```bash
# Monitor memory usage
ps aux --sort=-%mem | head -20
free -h
cat /proc/meminfo

# Check for memory leaks
valgrind --tool=memcheck --leak-check=full ./myapp
```

#### Connection Issues
```bash
# Check network connections
netstat -tulpn | grep :8080
ss -tulpn | grep :8080

# Check firewall
iptables -L -n
ufw status

# Test connectivity
curl -v http://localhost:8080/health
telnet localhost 8080
```

#### Performance Issues
```bash
# CPU profiling
perf record -g ./myapp
perf report

# I/O monitoring
iotop
iostat -x 1

# Network monitoring
iftop
nethogs
```

### Logging and Debugging

```bash
# Application logs
tail -f /var/log/myapp/app.log
journalctl -u myapp -f

# System logs
dmesg | tail -20
tail -f /var/log/syslog

# Performance monitoring
top -p $(pgrep myapp)
htop
```

## Conclusion

Successful production deployment of cppSwitchboard applications requires careful attention to configuration, security, monitoring, and performance optimization. This guide provides a comprehensive foundation for deploying robust, scalable applications in production environments.

Key points:
- Use proper configuration management with environment-specific settings
- Implement comprehensive security measures at all levels
- Set up monitoring and alerting for proactive issue detection
- Follow deployment best practices with proper testing and rollback procedures
- Optimize for performance based on your specific requirements

For more information, see:
- [Configuration Guide](CONFIGURATION.md)
- [Security Best Practices](SECURITY.md)
- [Monitoring Guide](MONITORING.md) 