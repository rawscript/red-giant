#!/bin/bash

# Red Giant Protocol - Fly.io Deployment Script

set -e

source ../scripts/common.sh

APP_NAME=${APP_NAME:-red-giant}
REGION=${REGION:-ord}

print_info "Deploying to Fly.io..."
print_info "App name: $APP_NAME"
print_info "Region: $REGION"

# Initialize Fly app
init_app() {
    print_info "Initializing Fly.io app..."
    
    if flyctl apps list | grep -q $APP_NAME; then
        print_info "App $APP_NAME already exists"
    else
        flyctl apps create $APP_NAME
        print_success "Fly.io app created: $APP_NAME"
    fi
}

# Generate fly.toml configuration
generate_config() {
    print_info "Generating fly.toml configuration..."
    
    cat > fly.toml <<EOF
app = "$APP_NAME"
primary_region = "$REGION"

[build]
  dockerfile = "Dockerfile.production"

[env]
  RED_GIANT_PORT = "8080"
  RED_GIANT_HOST = "0.0.0.0"
  RED_GIANT_WORKERS = "4"
  CLOUD_PROVIDER = "fly"

[http_service]
  internal_port = 8080
  force_https = true
  auto_stop_machines = true
  auto_start_machines = true
  min_machines_running = 1
  processes = ["app"]

[[http_service.checks]]
  interval = "10s"
  grace_period = "5s"
  method = "get"
  path = "/health"
  protocol = "http"
  timeout = "2s"
  tls_skip_verify = false

[[services]]
  internal_port = 9090
  protocol = "tcp"
  auto_stop_machines = true
  auto_start_machines = true
  min_machines_running = 0

  [[services.ports]]
    port = 9090
    handlers = ["http"]

[metrics]
  port = 9090
  path = "/metrics"

[[vm]]
  cpu_kind = "shared"
  cpus = 2
  memory_mb = 2048

[deploy]
  strategy = "rolling"
EOF
    
    print_success "fly.toml generated"
}

# Deploy application
deploy_app() {
    print_info "Deploying to Fly.io..."
    
    flyctl deploy --app $APP_NAME
    
    print_success "Deployment completed!"
}

# Main deployment flow
main() {
    init_app
    generate_config
    deploy_app
    
    APP_URL="https://$APP_NAME.fly.dev"
    
    print_success "Fly.io deployment completed!"
    print_info "App URL: $APP_URL"
    print_info "Status: flyctl status --app $APP_NAME"
    print_info "Logs: flyctl logs --app $APP_NAME"
}

main "$@"