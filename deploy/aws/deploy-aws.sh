#!/bin/bash

# Red Giant Protocol - AWS Deployment Script
# Supports: ECS Fargate, EKS, EC2, and Lambda

set -e

source ../scripts/common.sh

AWS_REGION=${AWS_REGION:-us-east-1}
CLUSTER_NAME=${CLUSTER_NAME:-red-giant-cluster}
SERVICE_NAME=${SERVICE_NAME:-red-giant-service}
ECR_REPO=${ECR_REPO:-red-giant}

print_info "Starting AWS deployment..."
print_info "Region: $AWS_REGION"
print_info "Cluster: $CLUSTER_NAME"

# Create ECR repository if it doesn't exist
create_ecr_repo() {
    print_info "Creating ECR repository..."
    
    if ! aws ecr describe-repositories --repository-names $ECR_REPO --region $AWS_REGION &> /dev/null; then
        aws ecr create-repository \
            --repository-name $ECR_REPO \
            --region $AWS_REGION \
            --image-scanning-configuration scanOnPush=true
        print_success "ECR repository created"
    else
        print_info "ECR repository already exists"
    fi
}

# Build and push Docker image
build_and_push() {
    print_info "Building and pushing Docker image..."
    
    # Get ECR login token
    aws ecr get-login-password --region $AWS_REGION | docker login --username AWS --password-stdin $(aws sts get-caller-identity --query Account --output text).dkr.ecr.$AWS_REGION.amazonaws.com
    
    # Build image
    docker build -f ../../Dockerfile.production -t $ECR_REPO:latest ../../
    
    # Tag for ECR
    ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)
    ECR_URI="$ACCOUNT_ID.dkr.ecr.$AWS_REGION.amazonaws.com/$ECR_REPO:latest"
    docker tag $ECR_REPO:latest $ECR_URI
    
    # Push to ECR
    docker push $ECR_URI
    print_success "Image pushed to ECR: $ECR_URI"
    
    echo $ECR_URI > ecr_uri.txt
}

# Deploy to ECS Fargate
deploy_ecs() {
    print_info "Deploying to ECS Fargate..."
    
    # Create ECS cluster
    if ! aws ecs describe-clusters --clusters $CLUSTER_NAME --region $AWS_REGION | grep -q "ACTIVE"; then
        aws ecs create-cluster \
            --cluster-name $CLUSTER_NAME \
            --capacity-providers FARGATE \
            --default-capacity-provider-strategy capacityProvider=FARGATE,weight=1 \
            --region $AWS_REGION
        print_success "ECS cluster created"
    fi
    
    # Register task definition
    ECR_URI=$(cat ecr_uri.txt)
    envsubst < task-definition.json > task-definition-final.json
    
    aws ecs register-task-definition \
        --cli-input-json file://task-definition-final.json \
        --region $AWS_REGION
    
    # Create or update service
    if aws ecs describe-services --cluster $CLUSTER_NAME --services $SERVICE_NAME --region $AWS_REGION | grep -q "ACTIVE"; then
        aws ecs update-service \
            --cluster $CLUSTER_NAME \
            --service $SERVICE_NAME \
            --task-definition red-giant-task \
            --region $AWS_REGION
        print_info "Service updated"
    else
        aws ecs create-service \
            --cluster $CLUSTER_NAME \
            --service-name $SERVICE_NAME \
            --task-definition red-giant-task \
            --desired-count 2 \
            --launch-type FARGATE \
            --network-configuration "awsvpcConfiguration={subnets=[$(aws ec2 describe-subnets --filters Name=default-for-az,Values=true --query 'Subnets[0:2].SubnetId' --output text | tr '\t' ',')],securityGroups=[$(aws ec2 create-security-group --group-name red-giant-sg --description 'Red Giant Security Group' --query 'GroupId' --output text)],assignPublicIp=ENABLED}" \
            --region $AWS_REGION
        print_success "Service created"
    fi
    
    print_success "ECS deployment completed!"
}

# Main deployment flow
main() {
    create_ecr_repo
    build_and_push
    deploy_ecs
    
    print_success "AWS deployment completed!"
    print_info "Your Red Giant server is deploying to ECS Fargate"
    print_info "Check status: aws ecs describe-services --cluster $CLUSTER_NAME --services $SERVICE_NAME --region $AWS_REGION"
}

main "$@"