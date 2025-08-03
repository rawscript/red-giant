#!/bin/bash

# Red Giant Protocol - Universal Installer
# Works on: Linux, macOS, Windows (WSL)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_banner() {
    echo -e "${BLUE}"
    echo "🚀 Red Giant Protocol - Universal Installer"
    echo "==========================================="
    echo "High-performance data transmission system"
    echo -e "${NC}"
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

# Detect OS and architecture
detect_system() {
    OS=$(uname -s | tr '[:upper:]' '[:lower:]')
    ARCH=$(uname -m)
    
    case $ARCH in
        x86_64) ARCH="amd64" ;;
        aarch64|arm64) ARCH="arm64" ;;
        armv7l) ARCH="arm" ;;
        *) print_error "Unsupported architecture: $ARCH"; exit 1 ;;
    esac
    
    print_info "Detected system: $OS/$ARCH"
}

# Install dependencies
install_dependencies() {
    print_info "Installing dependencies..."
    
    case $OS in
        linux)
            if command -v apt-get &> /dev/null; then
                # Debian/Ubuntu
                sudo apt-get update
                sudo apt-get install -y wget curl git build-essential
                
                # Install Go if not present
                if ! command -v go &> /dev/null; then
                    wget -q https://go.dev/dl/go1.21.5.linux-$ARCH.tar.gz
                    sudo tar -C /usr/local -xzf go1.21.5.linux-$ARCH.tar.gz
                    echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
                    export PATH=$PATH:/usr/local/go/bin
                    rm go1.21.5.linux-$ARCH.tar.gz
                fi
                
            elif command -v yum &> /dev/null; then
                # RHEL/CentOS/Fedora
                sudo yum update -y
                sudo yum install -y wget curl git gcc gcc-c++ make
                
                # Install Go
                if ! command -v go &> /dev/null; then
                    wget -q https://go.dev/dl/go1.21.5.linux-$ARCH.tar.gz
                    sudo tar -C /usr/local -xzf go1.21.5.linux-$ARCH.tar.gz
                    echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
                    export PATH=$PATH:/usr/local/go/bin
                    rm go1.21.5.linux-$ARCH.tar.gz
                fi
                
            elif command -v apk &> /dev/null; then
                # Alpine Linux
                sudo apk update
                sudo apk add --no-cache wget curl git build-base go
            fi
            ;;
            
        darwin)
            # macOS
            if command -v brew &> /dev/null; then
                brew install go git
            else
                print_error "Homebrew not found. Please install Homebrew first: https://brew.sh"
                exit 1
            fi
            ;;
            
        *)
            print_error "Unsupported operating system: $OS"
            exit 1
            ;;
    esac
    
    print_success "Dependencies installed"
}

# Download and install Red Giant
install_red_giant() {
    print_info "Installing Red Giant Protocol..."
    
    # Create installation directory
    INSTALL_DIR="/opt/red-giant"
    sudo mkdir -p $INSTALL_DIR
    
    # Download source code
    if [ -d "$INSTALL_DIR/.git" ]; then
        print_info "Updating existing installation..."
        cd $INSTALL_DIR
        sudo git pull
    else
        print_info "Downloading Red Giant Protocol..."
        sudo git clone https://github.com/your-username/red-giant-protocol.git $INSTALL_DIR
        cd $INSTALL_DIR
    fi
    
    # Build the application
    print_info "Building Red Giant Protocol..."
    sudo CGO_ENABLED=1 go build -o red-giant-server red_giant_universal.go
    
    # Make executable
    sudo chmod +x red-giant-server
    
    print_success "Red Giant Protocol installed to $INSTALL_DIR"
}

# Create systemd service (Linux only)
create_service() {
    if [ "$OS" = "linux" ] && command -v systemctl &> /dev/null; then
        print_info "Creating systemd service..."
        
        sudo tee /etc/systemd/system/red-giant.service > /dev/null <<EOF
[Unit]
Description=Red Giant Protocol Server
After=network.target

[Service]
Type=simple
User=nobody
WorkingDirectory=/opt/red-giant
ExecStart=/opt/red-giant/red-giant-server
Restart=always
RestartSec=5
Environment=RED_GIANT_PORT=8080
Environment=RED_GIANT_HOST=0.0.0.0
Environment=RED_GIANT_WORKERS=8

[Install]
WantedBy=multi-user.target
EOF
        
        sudo systemctl daemon-reload
        sudo systemctl enable red-giant
        
        print_success "Systemd service created"
    fi
}

# Create startup script
create_startup_script() {
    print_info "Creating startup script..."
    
    sudo tee /usr/local/bin/red-giant > /dev/null <<'EOF'
#!/bin/bash

INSTALL_DIR="/opt/red-giant"
PID_FILE="/var/run/red-giant.pid"

case "$1" in
    start)
        echo "Starting Red Giant Protocol..."
        cd $INSTALL_DIR
        nohup ./red-giant-server > /var/log/red-giant.log 2>&1 &
        echo $! > $PID_FILE
        echo "Red Giant started (PID: $(cat $PID_FILE))"
        echo "Access at: http://localhost:8080"
        ;;
    stop)
        if [ -f $PID_FILE ]; then
            PID=$(cat $PID_FILE)
            echo "Stopping Red Giant Protocol (PID: $PID)..."
            kill $PID
            rm -f $PID_FILE
            echo "Red Giant stopped"
        else
            echo "Red Giant is not running"
        fi
        ;;
    restart)
        $0 stop
        sleep 2
        $0 start
        ;;
    status)
        if [ -f $PID_FILE ] && kill -0 $(cat $PID_FILE) 2>/dev/null; then
            echo "Red Giant is running (PID: $(cat $PID_FILE))"
        else
            echo "Red Giant is not running"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
EOF
    
    sudo chmod +x /usr/local/bin/red-giant
    print_success "Startup script created at /usr/local/bin/red-giant"
}

# Configure firewall
configure_firewall() {
    print_info "Configuring firewall..."
    
    if command -v ufw &> /dev/null; then
        # Ubuntu/Debian UFW
        sudo ufw allow 8080/tcp
        sudo ufw allow 9090/tcp
    elif command -v firewall-cmd &> /dev/null; then
        # RHEL/CentOS firewalld
        sudo firewall-cmd --permanent --add-port=8080/tcp
        sudo firewall-cmd --permanent --add-port=9090/tcp
        sudo firewall-cmd --reload
    elif command -v iptables &> /dev/null; then
        # Generic iptables
        sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
        sudo iptables -A INPUT -p tcp --dport 9090 -j ACCEPT
    fi
    
    print_success "Firewall configured"
}

# Main installation flow
main() {
    print_banner
    
    # Check if running as root
    if [ "$EUID" -eq 0 ]; then
        print_error "Please don't run this script as root. It will use sudo when needed."
        exit 1
    fi
    
    detect_system
    install_dependencies
    install_red_giant
    create_service
    create_startup_script
    configure_firewall
    
    print_success "Red Giant Protocol installation completed!"
    echo ""
    print_info "Quick start:"
    echo "  sudo red-giant start     # Start the server"
    echo "  sudo red-giant status    # Check status"
    echo "  sudo red-giant stop      # Stop the server"
    echo ""
    print_info "Or with systemd (Linux):"
    echo "  sudo systemctl start red-giant"
    echo "  sudo systemctl status red-giant"
    echo ""
    print_info "Access your Red Giant server at: http://localhost:8080"
    print_info "Metrics available at: http://localhost:9090/metrics"
    echo ""
    print_info "For cloud deployment, run: ./deploy.sh <platform>"
}

main "$@"