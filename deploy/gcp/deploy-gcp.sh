#!/bin/bash

# Red Giant Protocol - Google Cloud Deployment Script
# Supports: GKE, Cloud Run, Compute Engine

set -e

source ../scripts/common.sh

PROJECT_ID=${PROJECT_ID:-$(gcloud config get-value project)}
REGION=${REGION:-us-central1}
CLUSTER_NAME=${CLUSTER_NAME:-red-giant-cluster}
SERVICE_NAME=${SERVICE_NAME:-red-giant-service}

print_info "Starting Google Cloud deployment..."
print_info "Project: $PROJECT_ID"
print_info "Region: $REGION"

# Enable required APIs
enable_apis() {
    print_info "Enabling required APIs..."
    
    gcloud services enable \
        container.googleapis.com \
        cloudbuild.googleapis.com \
        run.googleapis.com \
        compute.googleapis.com \
        --project=$PROJECT_ID
    
    print_success "APIs enabled"
}

# Build image with Cloud Build
build_image() {
    print_info "Building image with Cloud Build..."
    
    # Submit build
    gcloud builds submit \
        --tag gcr.io/$PROJECT_ID/red-giant:latest \
        --file ../../Dockerfile.production \
        ../../ \
        --project=$PROJECT_ID
    
    print_success "Image built: gcr.io/$PROJECT_ID/red-giant:latest"
}

# Deploy to Cloud Run
deploy_cloud_run() {
    print_info "Deploying to Cloud Run..."
    
    gcloud run deploy $SERVICE_NAME \
        --image gcr.io/$PROJECT_ID/red-giant:latest \
        --platform managed \
        --region $REGION \
        --allow-unauthenticated \
        --port 8080 \
        --memory 2Gi \
        --cpu 2 \
        --concurrency 1000 \
        --max-instances 10 \
        --set-env-vars "RED_GIANT_PORT=8080,RED_GIANT_HOST=0.0.0.0,RED_GIANT_WORKERS=8,CLOUD_PROVIDER=gcp,REGION=$REGION" \
        --project=$PROJECT_ID
    
    # Get service URL
    SERVICE_URL=$(gcloud run services describe $SERVICE_NAME --platform managed --region $REGION --format 'value(status.url)' --project=$PROJECT_ID)
    
    print_success "Cloud Run deployment completed!"
    print_info "Service URL: $SERVICE_URL"
}

# Deploy to GKE
deploy_gke() {
    print_info "Deploying to GKE..."
    
    # Create GKE cluster if it doesn't exist
    if ! gcloud container clusters describe $CLUSTER_NAME --region $REGION --project=$PROJECT_ID &> /dev/null; then
        print_info "Creating GKE cluster..."
        gcloud container clusters create $CLUSTER_NAME \
            --region $REGION \
            --machine-type e2-standard-4 \
            --num-nodes 2 \
            --enable-autoscaling \
            --min-nodes 1 \
            --max-nodes 5 \
            --enable-autorepair \
            --enable-autoupgrade \
            --project=$PROJECT_ID
        print_success "GKE cluster created"
    fi
    
    # Get cluster credentials
    gcloud container clusters get-credentials $CLUSTER_NAME --region $REGION --project=$PROJECT_ID
    
    # Apply Kubernetes manifests
    envsubst < gke-deployment.yaml | kubectl apply -f -
    
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
    
    print_success "GKE deployment completed!"
    print_info "Service URL: http://$EXTERNAL_IP:8080"
}

# Main deployment flow
main() {
    if [ -z "$PROJECT_ID" ]; then
        print_error "PROJECT_ID not set. Run 'gcloud config set project YOUR_PROJECT_ID' first."
        exit 1
    fi
    
    enable_apis
    build_image
    
    # Choose deployment method
    echo "Choose deployment method:"
    echo "1) Cloud Run (Serverless, recommended)"
    echo "2) GKE (Kubernetes)"
    read -p "Enter choice (1-2): " choice
    
    case $choice in
        1)
            deploy_cloud_run
            ;;
        2)
            deploy_gke
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
    
    print_success "Google Cloud deployment completed!"
}

main "$@"