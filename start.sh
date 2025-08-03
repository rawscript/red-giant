#!/bin/bash
# Red Giant Protocol - Quick Start Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[$(date +'%H:%M:%S')] $1${NC}"
}

warn() {
    echo -e "${YELLOW}[$(date +'%H:%M:%S')] $1${NC}"
}

error() {
    echo -e "${RED}[$(date +'%H:%M:%S')] $1${NC}"
    exit 1
}

# Check if Go is installed
check_go() {
    if ! command -v go &> /dev/null; then
        error "Go is not installed. Please install Go 1.21 or later."
    fi
    log "Go version: $(go version)"
}

# Start the server
start_server() {
    log "🚀 Starting Red Giant Protocol Server..."
    
    # Set default environment variables
    export RED_GIANT_PORT=${RED_GIANT_PORT:-8080}
    export RED_GIANT_HOST=${RED_GIANT_HOST:-0.0.0.0}
    export RED_GIANT_WORKERS=${RED_GIANT_WORKERS:-$(nproc 2>/dev/null || echo 4)}
    
    log "Configuration:"
    log "  - Port: $RED_GIANT_PORT"
    log "  - Host: $RED_GIANT_HOST"
    log "  - Workers: $RED_GIANT_WORKERS"
    
    # Start server
    go run red_giant_server.go
}

# Run client tests
test_client() {
    log "🧪 Running client tests..."
    
    # Wait for server to start
    sleep 2
    
    # Run tests
    go run client.go test
    go run client.go health
    go run client.go metrics
    go run client.go benchmark
}

# Docker deployment
deploy_docker() {
    log "🐳 Deploying with Docker..."
    
    if ! command -v docker &> /dev/null; then
        error "Docker is not installed"
    fi
    
    # Build and run
    docker build -f Dockerfile.production -t red-giant:latest .
    docker run -d --name red-giant-server -p 8080:8080 red-giant:latest
    
    log "Docker container started. Server available at http://localhost:8080"
}

# Docker Compose deployment
deploy_compose() {
    log "🐳 Deploying with Docker Compose..."
    
    if ! command -v docker-compose &> /dev/null; then
        error "Docker Compose is not installed"
    fi
    
    docker-compose -f docker-compose.production.yml up -d
    
    log "Services started:"
    log "  - Red Giant Server: http://localhost:8080"
    log "  - Nginx Proxy: http://localhost:80"
}

# Show usage
usage() {
    echo "Red Giant Protocol - Quick Start"
    echo ""
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  start       Start the server (default)"
    echo "  test        Run client tests"
    echo "  docker      Deploy with Docker"
    echo "  compose     Deploy with Docker Compose"
    echo "  stop        Stop running containers"
    echo "  clean       Clean up containers and images"
    echo ""
    echo "Environment Variables:"
    echo "  RED_GIANT_PORT     Server port (default: 8080)"
    echo "  RED_GIANT_HOST     Server host (default: 0.0.0.0)"
    echo "  RED_GIANT_WORKERS  Number of workers (default: CPU cores)"
    echo ""
    echo "Examples:"
    echo "  $0 start"
    echo "  RED_GIANT_PORT=9000 $0 start"
    echo "  $0 docker"
    echo "  $0 compose"
}

# Stop containers
stop_containers() {
    log "🛑 Stopping containers..."
    
    docker stop red-giant-server 2>/dev/null || true
    docker-compose -f docker-compose.production.yml down 2>/dev/null || true
    
    log "Containers stopped"
}

# Clean up
cleanup() {
    log "🧹 Cleaning up..."
    
    # Stop containers
    stop_containers
    
    # Remove containers
    docker rm red-giant-server 2>/dev/null || true
    
    # Remove images
    docker rmi red-giant:latest 2>/dev/null || true
    
    log "Cleanup completed"
}

# Main function
main() {
    echo "🚀 Red Giant Protocol - Quick Start"
    echo "=================================="
    
    case "${1:-start}" in
        start)
            check_go
            start_server
            ;;
        test)
            check_go
            test_client
            ;;
        docker)
            deploy_docker
            ;;
        compose)
            deploy_compose
            ;;
        stop)
            stop_containers
            ;;
        clean)
            cleanup
            ;;
        help|--help|-h)
            usage
            ;;
        *)
            echo "Unknown command: $1"
            usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"