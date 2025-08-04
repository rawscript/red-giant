# 🚀 Red Giant Protocol - Linode Deployment Guide

Deploy Red Giant on Linode with high performance and cost efficiency!

## 🎯 Deployment Options

### Option 1: Linode Kubernetes Engine (LKE) - Recommended
- **Managed Kubernetes** - Linode handles the control plane
- **Auto-scaling** - Scales from 2-10 instances based on load
- **Load balancing** - Built-in load balancer
- **High availability** - Multi-node deployment
- **Easy updates** - Rolling deployments

### Option 2: Linode Instance (VPS)
- **Traditional server** - Full control over the instance
- **Cost-effective** - Single instance deployment
- **Simple setup** - Direct installation on Ubuntu
- **SSH access** - Full root access to the server

## 🚀 Quick Start

### Prerequisites

1. **Linode Account**: Sign up at [linode.com](https://linode.com)
2. **API Token**: Create one in Linode Cloud Manager → API Tokens
3. **Linode CLI**: Install and configure

```bash
# Install Linode CLI
pip3 install linode-cli

# Configure with your API token
linode-cli configure
```

### Deploy with One Command

```bash
# Make script executable
chmod +x deploy/linode/deploy-linode.sh

# Run deployment
./deploy/linode/deploy-linode.sh
```

The script will ask you to choose:
1. **LKE (Kubernetes)** - For production, high availability
2. **Instance (VPS)** - For development, cost-effective

## 📋 Detailed Deployment Steps

### LKE (Kubernetes) Deployment

```bash
# 1. Set your preferences (optional)
export CLUSTER_NAME="red-giant-cluster"
export REGION="us-east"           # or us-west, eu-west, ap-south, etc.
export NODE_TYPE="g6-standard-2"  # 2 vCPUs, 4GB RAM
export NODE_COUNT="3"

# 2. Deploy
./deploy/linode/deploy-linode.sh

# 3. Choose option 1 (LKE)
```

**What happens:**
- Creates LKE cluster with 3 nodes
- Builds and pushes Red Giant Docker image
- Deploys with auto-scaling (2-10 instances)
- Sets up load balancer
- Configures health checks and monitoring
- Returns public IP address

**Result:**
```
✅ LKE deployment completed!
Red Giant URL: http://45.79.123.456:8080
Kubeconfig saved to: kubeconfig-red-giant-cluster.yaml
```

### Instance (VPS) Deployment

```bash
# 1. Set your preferences (optional)
export REGION="us-east"
export NODE_TYPE="g6-standard-2"  # 2 vCPUs, 4GB RAM

# 2. Deploy
./deploy/linode/deploy-linode.sh

# 3. Choose option 2 (Instance)
```

**What happens:**
- Creates Ubuntu 22.04 Linode instance
- Installs Go, Docker, and build tools
- Compiles Red Giant with CGO enabled
- Creates systemd service
- Configures firewall
- Starts Red Giant server

**Result:**
```
✅ Linode instance deployment completed!
Red Giant URL: http://45.79.123.456:8080
SSH access: ssh root@45.79.123.456
```

## 💰 Cost Estimates

### LKE Deployment (3 nodes)
- **g6-standard-2** (2 vCPU, 4GB): $36/month × 3 = **$108/month**
- **g6-standard-4** (4 vCPU, 8GB): $72/month × 3 = **$216/month**
- Plus load balancer: **$10/month**

### Instance Deployment (1 server)
- **g6-standard-2** (2 vCPU, 4GB): **$36/month**
- **g6-standard-4** (4 vCPU, 8GB): **$72/month**
- **g6-dedicated-8** (8 vCPU, 32GB): **$288/month**

## 🔧 Configuration Options

### Environment Variables

```bash
# Core settings
RED_GIANT_PORT=8080
RED_GIANT_HOST=0.0.0.0
RED_GIANT_WORKERS=8

# Performance (adjust based on Linode instance size)
RED_GIANT_MAX_CHUNK_SIZE=262144
RED_GIANT_BUFFER_SIZE=1048576
RED_GIANT_MAX_CONNECTIONS=1000

# Monitoring
RED_GIANT_METRICS_ENABLED=true
RED_GIANT_METRICS_PORT=9090
```

### Linode Instance Types

| Type | vCPUs | RAM | Storage | Network | Price/month |
|------|-------|-----|---------|---------|-------------|
| g6-nanode-1 | 1 | 1GB | 25GB | 1Gbps | $6 |
| g6-standard-1 | 1 | 2GB | 50GB | 1Gbps | $12 |
| g6-standard-2 | 2 | 4GB | 80GB | 2Gbps | $24 |
| g6-standard-4 | 4 | 8GB | 160GB | 4Gbps | $48 |
| g6-standard-6 | 6 | 16GB | 320GB | 6Gbps | $96 |
| g6-dedicated-8 | 8 | 32GB | 640GB | 8Gbps | $288 |

**Recommended for Red Giant:**
- **Development**: g6-standard-2 (2 vCPU, 4GB)
- **Production**: g6-standard-4+ (4+ vCPU, 8+ GB)
- **High Performance**: g6-dedicated-8 (8 vCPU, 32GB)

## 🌍 Linode Regions

Choose the region closest to your users:

| Region | Location | Code |
|--------|----------|------|
| Newark | New Jersey, USA | us-east |
| Dallas | Texas, USA | us-central |
| Fremont | California, USA | us-west |
| Atlanta | Georgia, USA | us-southeast |
| Toronto | Canada | ca-central |
| London | United Kingdom | eu-west |
| Frankfurt | Germany | eu-central |
| Singapore | Singapore | ap-south |
| Tokyo | Japan | ap-northeast |
| Sydney | Australia | ap-southeast |

## 🔒 Security Configuration

### Firewall Rules (Automatic)

The deployment automatically configures:
- **Port 8080**: Red Giant HTTP API
- **Port 9090**: Metrics endpoint
- **Port 22**: SSH access (Instance deployment only)

### SSL/TLS Setup (Optional)

For production, add SSL certificate:

```bash
# Install Certbot on your instance
apt-get install certbot

# Get SSL certificate (replace with your domain)
certbot certonly --standalone -d your-domain.com

# Update Red Giant configuration
export RED_GIANT_TLS_ENABLED=true
export RED_GIANT_TLS_CERT_PATH=/etc/letsencrypt/live/your-domain.com/fullchain.pem
export RED_GIANT_TLS_KEY_PATH=/etc/letsencrypt/live/your-domain.com/privkey.pem

# Restart Red Giant
systemctl restart red-giant
```

## 📊 Monitoring & Maintenance

### Check Status

```bash
# LKE deployment
kubectl get pods
kubectl get services
kubectl logs -l app=red-giant

# Instance deployment
ssh root@your-ip
systemctl status red-giant
journalctl -u red-giant -f
```

### Performance Monitoring

```bash
# Check Red Giant metrics
curl http://your-ip:9090/metrics

# Check system resources
htop
df -h
free -h
```

### Updates

```bash
# LKE deployment
kubectl set image deployment/red-giant-deployment red-giant-server=your-dockerhub-username/red-giant:new-version

# Instance deployment
ssh root@your-ip
cd /opt/red-giant
git pull
go build -o red-giant-server red_giant_server.go
systemctl restart red-giant
```

## 🆘 Troubleshooting

### Common Issues

**1. Linode CLI not configured**
```bash
linode-cli configure
# Enter your API token when prompted
```

**2. SSH key not found**
```bash
# Generate SSH key if you don't have one
ssh-keygen -t rsa -b 4096 -C "your-email@example.com"

# Add to ssh-agent
ssh-add ~/.ssh/id_rsa
```

**3. Docker Hub authentication**
```bash
# Login to Docker Hub
docker login

# Update image name in deployment files
sed -i 's/your-dockerhub-username/YOUR_ACTUAL_USERNAME/g' deploy/linode/lke-deployment.yaml
```

**4. Port 8080 blocked**
```bash
# Check firewall status
ufw status

# Allow port if needed
ufw allow 8080/tcp
```

**5. Red Giant not starting**
```bash
# Check logs
journalctl -u red-giant -f

# Check if Go is installed
/usr/local/go/bin/go version

# Rebuild if needed
cd /opt/red-giant
CGO_ENABLED=1 /usr/local/go/bin/go build -o red-giant-server red_giant_server.go
```

## 🎯 Performance Optimization

### For High-Performance Workloads

```bash
# Use dedicated CPU instances
export NODE_TYPE="g6-dedicated-8"

# Increase worker count
export RED_GIANT_WORKERS=16

# Optimize buffer sizes
export RED_GIANT_BUFFER_SIZE=2097152  # 2MB
export RED_GIANT_MAX_CHUNK_SIZE=524288  # 512KB
```

### Network Optimization

```bash
# Enable TCP BBR congestion control
echo 'net.core.default_qdisc=fq' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_congestion_control=bbr' >> /etc/sysctl.conf
sysctl -p
```

## 🎉 Success!

After deployment, you can:

1. **Test the API**: `curl http://your-ip:8080/health`
2. **Use the web interface**: Visit `http://your-ip:8080`
3. **Connect with SDKs**: Use your server URL in Go/JavaScript/Python clients
4. **Monitor performance**: Check `/metrics` endpoint

Your Red Giant Protocol is now running on Linode with the full power of the optimized C core and 500+ MB/s throughput! 🚀

## 📞 Support

- **Linode Support**: [Linode Support](https://www.linode.com/support/)
- **Red Giant Issues**: Create GitHub issues for Red Giant-specific problems
- **Community**: Join our Discord/Slack for deployment help