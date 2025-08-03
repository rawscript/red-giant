#!/bin/bash

# Common functions for deployment scripts

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 is required but not installed"
        exit 1
    fi
}

wait_for_deployment() {
    local url=$1
    local max_attempts=${2:-30}
    local attempt=1
    
    print_info "Waiting for deployment to be ready..."
    
    while [ $attempt -le $max_attempts ]; do
        if curl -s -f "$url/health" > /dev/null 2>&1; then
            print_success "Deployment is ready!"
            return 0
        fi
        
        print_info "Attempt $attempt/$max_attempts - waiting..."
        sleep 10
        ((attempt++))
    done
    
    print_error "Deployment failed to become ready after $max_attempts attempts"
    return 1
}