#!/bin/bash

# Red Giant Protocol - Interactive Setup Wizard
# Guides users through the best deployment option for their needs

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

print_banner() {
    clear
    echo -e "${PURPLE}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║                                                              ║"
    echo "║    🚀 Red Giant Protocol - Interactive Setup Wizard         ║"
    echo "║                                                              ║"
    echo "║    High-performance data transmission system                 ║"
    echo "║    Deploy anywhere in minutes!                               ║"
    echo "║                                                              ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Ask user about their deployment needs
ask_deployment_type() {
    echo -e "${CYAN}What type of deployment are you looking for?${NC}"
    echo ""
    echo "1) 🌩️  Cloud deployment (AWS, GCP, Azure, etc.)"
    echo "2) 🐳 Docker/Container deployment"
    echo "3) 🖥️  Traditional server installation"
    echo "4) 🧪 Local development/testing"
    echo "5) 📊 Full monitoring stack"
    echo ""
    read -p "Enter your choice (1-5): " deployment_type
}

# Ask about cloud provider
ask_cloud_provider() {
    echo ""
    echo -e "${CYAN}Which cloud provider would you like to use?${NC}"
    echo ""
    echo "1) 🟠 AWS (Amazon Web Services)"
    echo "2) 🔵 Google Cloud Platform"
    echo "3) 🟦 Microsoft Azure"
    echo "4) 🌊 DigitalOcean"
    echo "5) 🟣 Heroku"
    echo "6) 🚀 Fly.io"
    echo "7) 🚂 Railway"
    echo "8) ⚡ Vercel"
    echo "9) ☸️  Any Kubernetes cluster"
    echo ""
    read -p "Enter your choice (1-9): " cloud_choice
}

# Ask about server type
ask_server_type() {
    echo ""
    echo -e "${CYAN}What type of server are you using?${NC}"
    echo ""
    echo "1) 🐧 Linux server (Ubuntu, CentOS, etc.)"
    echo "2) 🍎 macOS"
    echo "3) 🪟 Windows (WSL)"
    echo "4) 🐳 Docker-compatible system"
    echo ""
    read -p "Enter your choice (1-4): " server_choice
}

# Check prerequisites
check_prerequisites() {
    local required_tools=("$@")
    local missing_tools=()
    
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_warning "Missing required tools: ${missing_tools[*]}"
        echo ""
        echo "Would you like me to help you install them? (y/n)"
        read -p "> " install_tools
        
        if [ "$install_tools" = "y" ] || [ "$install_tools" = "Y" ]; then
            install_missing_tools "${missing_tools[@]}"
        else
            print_error "Please install the required tools and run this wizard again."
            exit 1
        fi
    fi
}

# Install missing tools
install_missing_tools() {
    local tools=("$@")
    
    print_info "Installing missing tools..."
    
    for tool in "${tools[@]}"; do
        case "$tool" in
            "docker")
                echo "Installing Docker..."
                curl -fsSL https://get.docker.com -o get-docker.sh
                sh get-docker.sh
                rm get-docker.sh
                ;;
            "kubectl")
                echo "Installing kubectl..."
                curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
                chmod +x kubectl
                sudo mv kubectl /usr/local/bin/
                ;;
            "aws")
                echo "Installing AWS CLI..."
                curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
                unzip awscliv2.zip
                sudo ./aws/install
                rm -rf aws awscliv2.zip
                ;;
            "gcloud")
                echo "Installing Google Cloud SDK..."
                curl https://sdk.cloud.google.com | bash
                ;;
            *)
                print_warning "Don't know how to install $tool automatically. Please install it manually."
                ;;
        esac
    done
}

# Deploy based on user choice
deploy_cloud() {
    case $cloud_choice in
        1)
            print_info "Deploying to AWS..."
            check_prerequisites "aws" "docker"
            ./deploy.sh aws
            ;;
        2)
            print_info "Deploying to Google Cloud..."
            check_prerequisites "gcloud" "docker"
            ./deploy.sh gcp
            ;;
        3)
            print_info "Deploying to Azure..."
            check_prerequisites "az" "docker"
            ./deploy.sh azure
            ;;
        4)
            print_info "Deploying to DigitalOcean..."
            check_prerequisites "doctl" "docker"
            ./deploy.sh digitalocean
            ;;
        5)
            print_info "Deploying to Heroku..."
            check_prerequisites "heroku" "git"
            ./deploy.sh heroku
            ;;
        6)
            print_info "Deploying to Fly.io..."
            check_prerequisites "flyctl"
            ./deploy.sh fly
            ;;
        7)
            print_info "Deploying to Railway..."
            check_prerequisites "railway"
            ./deploy.sh railway
            ;;
        8)
            print_info "Deploying to Vercel..."
            check_prerequisites "vercel"
            ./deploy.sh vercel
            ;;
        9)
            print_info "Deploying to Kubernetes..."
            check_prerequisites "kubectl" "docker"
            ./deploy.sh kubernetes
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
}

# Deploy to server
deploy_server() {
    case $server_choice in
        1|2|3)
            print_info "Installing on your server..."
            ./install.sh
            ;;
        4)
            print_info "Deploying with Docker..."
            check_prerequisites "docker"
            ./deploy.sh docker
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
}

# Deploy for development
deploy_development() {
    print_info "Setting up for local development..."
    
    if command -v go &> /dev/null; then
        print_info "Starting Red Giant server..."
        echo "Run this command to start the server:"
        echo "  go run red_giant_universal.go"
        echo ""
        echo "Or build and run:"
        echo "  go build -o red-giant red_giant_universal.go"
        echo "  ./red-giant"
    else
        print_warning "Go is not installed. Installing..."
        ./install.sh
    fi
}

# Deploy monitoring stack
deploy_monitoring() {
    print_info "Setting up full monitoring stack..."
    check_prerequisites "docker" "docker-compose"
    
    cd deploy/monitoring
    docker-compose -f docker-compose.monitoring.yml up -d
    
    print_success "Monitoring stack deployed!"
    print_info "Access points:"
    echo "  • Red Giant: http://localhost:8080"
    echo "  • Grafana: http://localhost:3000 (admin/admin)"
    echo "  • Prometheus: http://localhost:9091"
    echo "  • Alertmanager: http://localhost:9093"
}

# Show final instructions
show_final_instructions() {
    echo ""
    print_success "🎉 Red Giant Protocol deployment completed!"
    echo ""
    print_info "What's next?"
    echo ""
    echo "📖 Documentation: Check README.md for detailed usage"
    echo "🧪 Test the API: curl http://your-server:8080/health"
    echo "📊 Monitor: Access metrics at http://your-server:9090/metrics"
    echo "💬 Support: Create an issue on GitHub if you need help"
    echo ""
    print_info "Happy high-performance data transmission! 🚀"
}

# Main wizard flow
main() {
    print_banner
    
    print_info "This wizard will help you deploy Red Giant Protocol in the best way for your needs."
    echo ""
    
    ask_deployment_type
    
    case $deployment_type in
        1)
            ask_cloud_provider
            deploy_cloud
            ;;
        2)
            print_info "Deploying with Docker..."
            check_prerequisites "docker"
            ./deploy.sh docker
            ;;
        3)
            ask_server_type
            deploy_server
            ;;
        4)
            deploy_development
            ;;
        5)
            deploy_monitoring
            ;;
        *)
            print_error "Invalid choice"
            exit 1
            ;;
    esac
    
    show_final_instructions
}

main "$@"