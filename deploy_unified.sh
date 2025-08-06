
#!/bin/bash
# Red Giant Protocol - Unified Deployment Script

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

show_usage() {
    echo "🚀 Red Giant Protocol - Unified Deployment"
    echo ""
    echo "Usage: ./deploy_unified.sh <variant> [options]"
    echo ""
    echo "Variants:"
    echo "  adaptive     - Universal high-performance server (recommended)"
    echo "  telemedicine - HIPAA-compliant medical server"
    echo "  peer         - P2P client interface"
    echo "  universal    - Basic universal server"
    echo ""
    echo "Options:"
    echo "  --port PORT     Port to run on (default: 8080)"
    echo "  --host HOST     Host to bind to (default: 0.0.0.0)"
    echo "  --workers N     Number of workers (default: auto)"
    echo ""
    echo "Examples:"
    echo "  ./deploy_unified.sh adaptive"
    echo "  ./deploy_unified.sh telemedicine --port 8443"
    echo "  ./deploy_unified.sh peer"
}

# Parse arguments
VARIANT=""
PORT="8080"
HOST="0.0.0.0"
WORKERS=""

while [[ $# -gt 0 ]]; do
    case $1 in
        adaptive|telemedicine|peer|universal)
            VARIANT="$1"
            shift
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        --host)
            HOST="$2"
            shift 2
            ;;
        --workers)
            WORKERS="$2"
            shift 2
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            error "Unknown option: $1"
            ;;
    esac
done

if [[ -z "$VARIANT" ]]; then
    show_usage
    exit 1
fi

# Set environment variables
export RED_GIANT_PORT="$PORT"
export RED_GIANT_HOST="$HOST"
if [[ -n "$WORKERS" ]]; then
    export RED_GIANT_WORKERS="$WORKERS"
fi

log "🚀 Deploying Red Giant Protocol - $VARIANT variant"
log "Configuration:"
log "  - Variant: $VARIANT"
log "  - Port: $PORT"
log "  - Host: $HOST"
log "  - Workers: ${WORKERS:-auto}"

# Deploy based on variant
case "$VARIANT" in
    adaptive)
        log "🎯 Starting Adaptive Multi-Format Server..."
        log "Supports: JSON, XML, Images, Video, Audio, Binary, Streaming"
        go run red_giant_adaptive.go
        ;;
    telemedicine)
        log "🏥 Starting HIPAA-Compliant Medical Server..."
        log "Features: End-to-end encryption, audit logging, PHI protection"
        go run red_giant_telemedicine.go
        ;;
    peer)
        log "🌐 Starting P2P Client Interface..."
        if [[ $# -eq 0 ]]; then
            go run red_giant_peer.go
        else
            go run red_giant_peer.go "$@"
        fi
        ;;
    universal)
        log "⚡ Starting Universal Server..."
        go run red_giant_universal.go
        ;;
    *)
        error "Invalid variant: $VARIANT"
        ;;
esac
