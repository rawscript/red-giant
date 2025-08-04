#!/bin/bash

# Red Giant Protocol - Universal Deployment Script
# Supports: AWS, GCP, Azure, DigitalOcean, Kubernetes, Docker

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_banner() {
    echo -e "${BLUE}"
    echo "🚀 Red Giant Protocol - Universal Deployer"
    echo "=========================================="
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
    local deps=("$@")
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            print_error "$dep is required but not installed"
            exit 1
        fi
    done
}

deploy_aws() {
    print_info "Deploying to AWS..."
    check_dependencies "aws" "docker"
    
    # Check AWS credentials
    if ! aws sts get-caller-identity &> /dev/null; then
        print_error "AWS credentials not configured. Run 'aws configure' first."
        exit 1
    fi
    
    cd deploy/aws
    ./deploy-aws.sh
    print_success "AWS deployment completed!"
}

deploy_gcp() {
    print_info "Deploying to Google Cloud..."
    check_dependencies "gcloud" "docker"
    
    # Check GCP authentication
    if ! gcloud auth list --filter=status:ACTIVE --format="value(account)" | head -n1 &> /dev/null; then
        print_error "GCP not authenticated. Run 'gcloud auth login' first."
        exit 1
    fi
    
    cd deploy/gcp
    ./deploy-gcp.sh
    print_success "Google Cloud deployment completed!"
}

deploy_azure() {
    print_info "Deploying to Azure..."
    check_dependencies "az" "docker"
    
    # Check Azure login
    if ! az account show &> /dev/null; then
        print_error "Azure not authenticated. Run 'az login' first."
        exit 1
    fi
    
    cd deploy/azure
    ./deploy-azure.sh
    print_success "Azure deployment completed!"
}

deploy_digitalocean() {
    print_info "Deploying to DigitalOcean..."
    check_dependencies "doctl" "docker"
    
    # Check DigitalOcean authentication
    if ! doctl auth list | grep -q "current context"; then
        print_error "DigitalOcean not authenticated. Run 'doctl auth init' first."
        exit 1
    fi
    
    cd deploy/digitalocean
    ./deploy-do.sh
    print_success "DigitalOcean deployment completed!"
}

deploy_linode() {
    print_info "Deploying to Linode..."
    check_dependencies "linode-cli" "docker"
    
    # Check Linode authentication
    if ! linode-cli regions list &> /dev/null; then
        print_error "Linode CLI not configured. Run 'linode-cli configure' first."
        exit 1
    fi
    
    cd deploy/linode
    ./deploy-linode.sh
    print_success "Linode deployment completed!"
}

deploy_kubernetes() {
    print_info "Deploying to Kubernetes..."
    check_dependencies "kubectl" "docker"
    
    # Check kubectl connection
    if ! kubectl cluster-info &> /dev/null; then
        print_error "kubectl not connected to a cluster"
        exit 1
    fi
    
    cd deploy/kubernetes
    ./deploy-k8s.sh
    print_success "Kubernetes deployment completed!"
}

deploy_docker() {
    print_info "Building and running with Docker..."
    check_dependencies "docker"
    
    # Build the image
    docker build -f Dockerfile.production -t red-giant:latest .
    
    # Run the container
    docker run -d \
        --name red-giant-server \
        -p 8080:8080 \
        -p 9090:9090 \
        --restart unless-stopped \
        red-giant:latest
    
    print_success "Docker deployment completed!"
    print_info "Red Giant is running at http://localhost:8080"
}

deploy_heroku() {
    print_info "Deploying to Heroku..."
    check_dependencies "heroku" "git"
    
    cd deploy/heroku
    ./deploy-heroku.sh
    print_success "Heroku deployment completed!"
}

deploy_railway() {
    print_info "Deploying to Railway..."
    check_dependencies "railway"
    
    cd deploy/railway
    ./deploy-railway.sh
    print_success "Railway deployment completed!"
}

deploy_fly() {
    print_info "Deploying to Fly.io..."
    check_dependencies "flyctl"
    
    cd deploy/fly
    ./deploy-fly.sh
    print_success "Fly.io deployment completed!"
}

show_usage() {
    echo "Usage: $0 <platform>"
    echo ""
    echo "Supported platforms:"
    echo "  aws           - Amazon Web Services (ECS/EKS)"
    echo "  gcp           - Google Cloud Platform (GKE/Cloud Run)"
    echo "  azure         - Microsoft Azure (AKS/Container Instances)"
    echo "  digitalocean  - DigitalOcean (Kubernetes/Droplets)"
    echo "  linode        - Linode (LKE/Instances)"
    echo "  kubernetes    - Any Kubernetes cluster"
    echo "  docker        - Local Docker deployment"
    echo "  heroku        - Heroku platform"
    echo "  railway       - Railway platform"
    echo "  fly           - Fly.io platform"
    echo ""
    echo "Examples:"
    echo "  $0 aws"
    echo "  $0 linode"
    echo "  $0 docker"
    echo "  $0 kubernetes"
}

main() {
    print_banner
    
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    
    platform="$1"
    
    case "$platform" in
        "aws")
            deploy_aws
            ;;
        "gcp")
            deploy_gcp
            ;;
        "azure")
            deploy_azure
            ;;
        "digitalocean"|"do")
            deploy_digitalocean
            ;;
        "linode")
            deploy_linode
            ;;
        "kubernetes"|"k8s")
            deploy_kubernetes
            ;;
        "docker")
            deploy_docker
            ;;
        "heroku")
            deploy_heroku
            ;;
        "railway")
            deploy_railway
            ;;
        "fly")
            deploy_fly
            ;;
        *)
            print_error "Unknown platform: $platform"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"