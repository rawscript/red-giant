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
    echo "  mobile       - GSM/Cellular network communication"
    echo ""
    echo "Options:"
    echo "  --port PORT     Port to run on (default: 8080)"
    echo "  --host HOST     Host to bind to (default: 0.0.0.0)"
    echo "  --workers N     Number of workers (default: auto)"
    echo "  --mobile-net    Set mobile network type (e.g., 2G, 3G, 4G, 5G, GSM)"
    echo ""
    echo "Examples:"
    echo "  ./deploy_unified.sh adaptive"
    echo "  ./deploy_unified.sh telemedicine --port 8443"
    echo "  ./deploy_unified.sh peer"
    echo "  ./deploy_unified.sh mobile --mobile-net 5G"
}

# Parse arguments
VARIANT=""
PORT="8080"
HOST="0.0.0.0"
WORKERS=""
MOBILE_NETWORK_TYPE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        adaptive|telemedicine|peer|universal|mobile)
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
        --mobile-net)
            MOBILE_NETWORK_TYPE="$2"
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
if [[ -n "$MOBILE_NETWORK_TYPE" ]]; then
    export MOBILE_NETWORK_TYPE="$MOBILE_NETWORK_TYPE"
fi

log "🚀 Deploying Red Giant Protocol - $VARIANT variant"
log "Configuration:"
log "  - Variant: $VARIANT"
log "  - Port: $PORT"
log "  - Host: $HOST"
log "  - Workers: ${WORKERS:-auto}"
if [[ "$VARIANT" == "mobile" ]]; then
    log "  - Mobile Network Type: ${MOBILE_NETWORK_TYPE:-4G}"
    log "  - Mobile Port: ${RED_GIANT_MOBILE_PORT:-9090}"
fi

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
        log "🌐 Starting Red Giant P2P Peer Client"
        if [[ ! -f "red_giant_peer.go" ]]; then
            error "red_giant_peer.go not found"
        fi
        exec go run red_giant_peer.go
        ;;
    "mobile")
        log "📱 Starting Red Giant Mobile GSM/Cellular Server"
        if [[ ! -f "red_giant_mobile_server.go" ]]; then
            error "red_giant_mobile_server.go not found"
        fi

        # Set mobile-specific environment
        export MOBILE_NETWORK_TYPE="${MOBILE_NETWORK_TYPE:-4G}"
        export RED_GIANT_MOBILE_PORT="${PORT:-9090}" # Using a different default port for mobile

        log "📱 Mobile Network Type: $MOBILE_NETWORK_TYPE"
        log "📡 Mobile Port: $RED_GIANT_MOBILE_PORT"

        exec go run red_giant_mobile_server.go
        ;;
    *)
        error "Unknown variant: $VARIANT"
        ;;
esac