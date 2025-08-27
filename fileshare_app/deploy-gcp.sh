#!/bin/bash

# Red Giant File Share App - Google Cloud Deployment Script

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_banner() {
    echo -e "${BLUE}"
    echo "🚀 Red Giant File Share App - Google Cloud Deployer"
    echo "=================================================="
    echo -e "${NC}"
}

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

check_dependencies() {
    local deps=("gcloud" "docker")
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            print_error "$dep is required but not installed"
            exit 1
        fi
    done
}

# Check Google Cloud authentication
check_gcloud_auth() {
    if ! gcloud auth list --filter=status:ACTIVE --format="value(account)" | head -n1 &> /dev/null; then
        print_error "GCP not authenticated. Run 'gcloud auth login' first."
        exit 1
    fi
}

# Deploy to Google Cloud Run
deploy_cloud_run() {
    print_info "Deploying to Google Cloud Run..."
    
    # Set project (optional, will use default if not set)
    if [ -n "$GCP_PROJECT" ]; then
        gcloud config set project "$GCP_PROJECT"
    fi
    
    # Build and push Docker image
    PROJECT_ID=$(gcloud config get-value project 2> /dev/null)
    if [ -z "$PROJECT_ID" ]; then
        print_error "Unable to determine GCP project ID"
        exit 1
    fi
    
    IMAGE_NAME="gcr.io/$PROJECT_ID/fileshare-app"
    
    print_info "Building Docker image..."
    docker build -t "$IMAGE_NAME" .
    
    print_info "Pushing Docker image to Google Container Registry..."
    docker push "$IMAGE_NAME"
    
    # Deploy to Cloud Run
    print_info "Deploying to Cloud Run..."
    gcloud run deploy fileshare-app \
        --image "$IMAGE_NAME" \
        --platform managed \
        --allow-unauthenticated \
        --port 3000 \
        --set-env-vars RED_GIANT_URL="$RED_GIANT_URL"
    
    SERVICE_URL=$(gcloud run services describe fileshare-app --platform managed --format 'value(status.url)')
    print_success "Deployment completed!"
    print_info "Your application is available at: $SERVICE_URL"
}

# Deploy to Google Kubernetes Engine
deploy_gke() {
    print_info "Deploying to Google Kubernetes Engine..."
    
    # This would require a more complex setup with Kubernetes manifests
    print_warning "GKE deployment requires additional Kubernetes configuration files."
    print_info "Please ensure you have a GKE cluster set up and kubectl configured."
    
    # Apply Kubernetes manifests
    kubectl apply -f k8s/
    
    print_success "GKE deployment completed!"
}

main() {
    print_banner
    
    # Check dependencies
    check_dependencies
    check_gcloud_auth
    
    # Check if RED_GIANT_URL is set
    if [ -z "$RED_GIANT_URL" ]; then
        print_warning "RED_GIANT_URL environment variable not set"
        print_info "Using default: http://redgiant-server:8080"
        export RED_GIANT_URL="http://redgiant-server:8080"
    fi
    
    echo "Choose deployment target:"
    echo "1) Google Cloud Run (recommended for simplicity)"
    echo "2) Google Kubernetes Engine (for advanced use cases)"
    echo ""
    
    read -p "Enter your choice (1 or 2): " choice
    
    case $choice in
        1)
            deploy_cloud_run
            ;;
        2)
            deploy_gke
            ;;
        *)
            print_error "Invalid choice. Please run the script again and select 1 or 2."
            exit 1
            ;;
    esac
}

# Run main function
main "$@"