#!/bin/bash

# Red Giant Protocol - Linode Deployment Script
# Supports: Linode Kubernetes Engine (LKE), Linode Instances, Linode Container Registry

set -e

source ../scripts/common.sh

CLUSTER_NAME=${CLUSTER_NAME:-red-giant-cluster}
REGION=${REGION:-us-east}
NODE_TYPE=${NODE_TYPE:-g6-standard-2}
NODE_COUNT=${NODE_COUNT:-3}

print_info "Starting Linode deployment..."
print_info "Region: $REGION"
print_info "Node type: $NODE_TYPE"

# Check Linode CLI
check_linode_cli() {
    if ! command -v linode-cli &> /dev/null; then
        print_error "Linode CLI not found. Installing..."
        pip3 install linode-cli
        print_info "Please run 'linode-cli configure' to set up your API token"
        exit 1
    fi
    
    # Test API connection
    if ! linode-cli regions list &> /dev/null; then
        print_error "Linode CLI not configured. Run 'linode-cli configure' first."
        exit 1
    fi
    
    print_success "Linode CLI configured"
}

# Deploy to Linode Kubernetes Engine (LKE)
deploy_lke() {
    print_info "Deploying to Linode Kubernetes Engine (LKE)..."
    
    # Create LKE cluster if it doesn't exist
    if ! linode-cli lke clusters-list --text --no-headers | grep -q "$CLUSTER_NAME"; then
        print_info "Creating LKE cluster: $CLUSTER_NAME"
        
        linode-cli lke cluster-create \
            --label $CLUSTER_NAME \
            --region $REGION \
            --k8s_version 1.28 \
            --node_pools.type $NODE_TYPE \
            --node_pools.count $NODE_COUNT \
            --node_pools.autoscaler.enabled true \
            --node_pools.autoscaler.min 1 \
            --node_pools.autoscaler.max 10
        
        print_success "LKE cluster created"
        
        # Wait for cluster to be ready
        print_info "Waiting for cluster to be ready..."
        while true; do
            STATUS=$(linode-cli lke clusters-list --text --no-headers --format="label,status" | grep "$CLUSTER_NAME" | cut -f2)
            if [ "$STATUS" = "ready" ]; then
                break
            fi
            print_info "Cluster status: $STATUS - waiting..."
            sleep 30
        done
        
        print_success "Cluster is ready!"
    else
        print_info "LKE cluster already exists"
    fi
    
    # Get cluster ID
    CLUSTER_ID=$(linode-cli lke clusters-list --text --no-headers --format="id,label" | grep "$CLUSTER_NAME" | cut -f1)
    
    # Download kubeconfig
    print_info "Downloading kubeconfig..."
    linode-cli lke kubeconfig-view $CLUSTER_ID --text --no-headers > kubeconfig-$CLUSTER_NAME.yaml
    export KUBECONFIG="$(pwd)/kubeconfig-$CLUSTER_NAME.yaml"
    
    # Verify kubectl connection
    kubectl cluster-info
    
    # Build and push image to Docker Hub (or use Linode's registry when available)
    build_and_push_image
    
    # Deploy to Kubernetes
    deploy_to_kubernetes
    
    # Get external IP
    print_info "Getting external IP..."
    while true; do
        EXTERNAL_IP=$(kubectl get service red-giant-service -o jsonpath='{.status.loadBalancer.ingress[0].ip}' 2>/dev/null || echo "")
        if [ ! -z "$EXTERNAL_IP" ]; then
            break
        fi
        print_info "Waiting for external IP..."
        sleep 10
    done
    
    print_success "LKE deployment completed!"
    print_info "Red Giant URL: http://$EXTERNAL_IP:8080"
    print_info "Kubeconfig saved to: kubeconfig-$CLUSTER_NAME.yaml"
}

# Deploy to Linode Instance (VPS)
deploy_instance() {
    print_info "Deploying to Linode Instance..."
    
    INSTANCE_LABEL="red-giant-server"
    ROOT_PASS=$(openssl rand -base64 32)
    
    # Create Linode instance if it doesn't exist
    if ! linode-cli linodes list --text --no-headers | grep -q "$INSTANCE_LABEL"; then
        print_info "Creating Linode instance: $INSTANCE_LABEL"
        
        # Create instance
        linode-cli linodes create \
            --label $INSTANCE_LABEL \
            --region $REGION \
            --type $NODE_TYPE \
            --image linode/ubuntu22.04 \
            --root_pass "$ROOT_PASS" \
            --authorized_keys "$(cat ~/.ssh/id_rsa.pub 2>/dev/null || echo '')" \
            --stackscript_id 1 \
            --stackscript_data '{"hostname":"red-giant-server"}'
        
        print_success "Linode instance created"
        print_info "Root password: $ROOT_PASS"
        
        # Wait for instance to be running
        print_info "Waiting for instance to boot..."
        while true; do
            STATUS=$(linode-cli linodes list --text --no-headers --format="label,status" | grep "$INSTANCE_LABEL" | cut -f2)
            if [ "$STATUS" = "running" ]; then
                break
            fi
            print_info "Instance status: $STATUS - waiting..."
            sleep 15
        done
        
        print_success "Instance is running!"
    else
        print_info "Linode instance already exists"
    fi
    
    # Get instance IP
    INSTANCE_IP=$(linode-cli linodes list --text --no-headers --format="label,ipv4" | grep "$INSTANCE_LABEL" | cut -f2 | cut -d',' -f1)
    
    print_info "Instance IP: $INSTANCE_IP"
    
    # Wait for SSH to be available
    print_info "Waiting for SSH to be available..."
    while ! nc -z $INSTANCE_IP 22; do
        sleep 5
    done
    
    # Install Red Giant on the instance
    install_on_instance $INSTANCE_IP
    
    print_success "Linode instance deployment completed!"
    print_info "Red Giant URL: http://$INSTANCE_IP:8080"
    print_info "SSH access: ssh root@$INSTANCE_IP"
}

# Install Red Giant on a Linode instance
install_on_instance() {
    local instance_ip=$1
    
    print_info "Installing Red Giant on instance: $instance_ip"
    
    # Create installation script
    cat > install_red_giant.sh << 'EOF'
#!/bin/bash
set -e

# Update system
apt-get update
apt-get upgrade -y

# Install dependencies
apt-get install -y wget curl git build-essential gcc

# Install Go
cd /tmp
wget https://go.dev/dl/go1.21.5.linux-amd64.tar.gz
tar -C /usr/local -xzf go1.21.5.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin' >> /root/.bashrc
export PATH=$PATH:/usr/local/go/bin

# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh

# Clone Red Giant repository
cd /opt
git clone https://github.com/your-username/red-giant-protocol.git red-giant
cd red-giant

# Build Red Giant with CGO
CGO_ENABLED=1 go build -o red-giant-server red_giant_server.go

# Create systemd service
cat > /etc/systemd/system/red-giant.service << 'EOL'
[Unit]
Description=Red Giant Protocol Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/red-giant
ExecStart=/opt/red-giant/red-giant-server
Restart=always
RestartSec=5
Environment=RED_GIANT_PORT=8080
Environment=RED_GIANT_HOST=0.0.0.0
Environment=RED_GIANT_WORKERS=8

[Install]
WantedBy=multi-user.target
EOL

# Enable and start service
systemctl daemon-reload
systemctl enable red-giant
systemctl start red-giant

# Configure firewall
ufw allow 8080/tcp
ufw allow 9090/tcp
ufw allow 22/tcp
ufw --force enable

echo "Red Giant installation completed!"
EOF
    
    # Copy and run installation script
    scp -o StrictHostKeyChecking=no install_red_giant.sh root@$instance_ip:/tmp/
    ssh -o StrictHostKeyChecking=no root@$instance_ip 'chmod +x /tmp/install_red_giant.sh && /tmp/install_red_giant.sh'
    
    # Clean up
    rm install_red_giant.sh
    
    print_success "Red Giant installed on instance"
}

# Build and push Docker image
build_and_push_image() {
    print_info "Building and pushing Docker image..."
    
    # Build image
    docker build -f ../../Dockerfile.production -t red-giant:latest ../../
    
    # Tag for Docker Hub (you can also use Linode's registry when available)
    docker tag red-giant:latest your-dockerhub-username/red-giant:latest
    
    # Push to Docker Hub
    docker push your-dockerhub-username/red-giant:latest
    
    print_success "Image pushed to Docker Hub"
}

# Deploy to Kubernetes cluster
deploy_to_kubernetes() {
    print_info "Deploying to Kubernetes..."
    
    # Apply Kubernetes manifests
    envsubst < lke-deployment.yaml | kubectl apply -f -
    
    # Wait for deployment
    kubectl rollout status deployment/red-giant-deployment
    
    print_success "Kubernetes deployment completed"
}

# Main deployment flow
main() {
    print_info "Choose Linode deployment method:"
    echo "1) Linode Kubernetes Engine (LKE) - Managed Kubernetes"
    echo "2) Linode Instance (VPS) - Traditional server"
    read -p "Enter choice (1-2): " choice
    
    check_linode_cli
    
    case $choice in
        1)
            deploy_lke
            ;;
        2)
            deploy_instance
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
    
    print_success "Linode deployment completed!"
}

main "$@"