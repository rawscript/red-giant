#!/bin/bash

# Red Giant Protocol - DigitalOcean Deployment Script
# Supports: Kubernetes, App Platform, Droplets

set -e

source ../scripts/common.sh

CLUSTER_NAME=${CLUSTER_NAME:-red-giant-cluster}
REGION=${REGION:-nyc1}
APP_NAME=${APP_NAME:-red-giant-app}

print_info "Starting DigitalOcean deployment..."
print_info "Region: $REGION"

# Deploy to DigitalOcean Kubernetes
deploy_kubernetes() {
    print_info "Deploying to DigitalOcean Kubernetes..."
    
    # Create cluster if it doesn't exist
    if ! doctl kubernetes cluster get $CLUSTER_NAME &> /dev/null; then
        print_info "Creating Kubernetes cluster..."
        doctl kubernetes cluster create $CLUSTER_NAME \
            --region $REGION \
            --node-pool "name=red-giant-pool;size=s-2vcpu-4gb;count=3;auto-scale=true;min-nodes=1;max-nodes=5"
        print_success "Kubernetes cluster created"
    fi
    
    # Get cluster credentials
    doctl kubernetes cluster kubeconfig save $CLUSTER_NAME
    
    # Build and push to DigitalOcean Container Registry
    print_info "Building and pushing image..."
    
    # Create registry if needed
    if ! doctl registry get &> /dev/null; then
        doctl registry create red-giant-registry
    fi
    
    # Login to registry
    doctl registry login
    
    # Build and push
    docker build -f ../../Dockerfile.production -t registry.digitalocean.com/red-giant-registry/red-giant:latest ../../
    docker push registry.digitalocean.com/red-giant-registry/red-giant:latest
    
    # Apply Kubernetes manifests
    envsubst < k8s-deployment.yaml | kubectl apply -f -
    
    # Wait for deployment
    kubectl rollout status deployment/red-giant-deployment
    
    # Get external IP
    print_info "Waiting for external IP..."
    while true; do
        EXTERNAL_IP=$(kubectl get service red-giant-service -o jsonpath='{.status.loadBalancer.ingress[0].ip}')
        if [ ! -z "$EXTERNAL_IP" ]; then
            break
        fi
        sleep 10
    done
    
    print_success "Kubernetes deployment completed!"
    print_info "Service URL: http://$EXTERNAL_IP:8080"
}

# Deploy to App Platform
deploy_app_platform() {
    print_info "Deploying to DigitalOcean App Platform..."
    
    # Create app spec
    envsubst < app-spec.yaml > app-spec-final.yaml
    
    # Deploy app
    doctl apps create --spec app-spec-final.yaml
    
    print_success "App Platform deployment initiated!"
    print_info "Check status: doctl apps list"
}

# Main deployment flow
main() {
    echo "Choose deployment method:"
    echo "1) Kubernetes (Full control, recommended)"
    echo "2) App Platform (Managed, simpler)"
    read -p "Enter choice (1-2): " choice
    
    case $choice in
        1)
            deploy_kubernetes
            ;;
        2)
            deploy_app_platform
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
    
    print_success "DigitalOcean deployment completed!"
}

main "$@"