# 🌍 Red Giant - Universal Cloud Deployment

Deploy Red Giant anywhere in minutes! This package supports all major cloud providers and deployment methods.

## 🚀 Quick Deploy Options

### Option 1: One-Click Cloud Deploy
```bash
# AWS
./deploy.sh aws

# Google Cloud
./deploy.sh gcp

# Azure
./deploy.sh azure

# DigitalOcean
./deploy.sh digitalocean

# Any Kubernetes cluster
./deploy.sh kubernetes
```

### Option 2: Docker Anywhere
```bash
# Build and run locally
docker build -t red-giant .
docker run -p 8080:8080 red-giant

# Or use our pre-built image
docker run -p 8080:8080 redgiant/red-giant:latest
```

### Option 3: Traditional Server
```bash
# Any Linux server
curl -sSL https://raw.githubusercontent.com/your-repo/red-giant/main/install.sh | bash
```

## 🌐 Supported Platforms

✅ **AWS** - ECS, EKS, EC2, Lambda  
✅ **Google Cloud** - GKE, Cloud Run, Compute Engine  
✅ **Azure** - AKS, Container Instances, VMs  
✅ **DigitalOcean** - Kubernetes, Droplets, App Platform  
✅ **Heroku** - Container deployment  
✅ **Railway** - One-click deploy  
✅ **Fly.io** - Global edge deployment  
✅ **Vercel** - Serverless functions  
✅ **Any Kubernetes** - Universal K8s manifests  
✅ **Docker** - Any Docker-compatible platform  
✅ **Traditional Servers** - Linux, Windows, macOS  

## 📁 Deployment Files

```
deploy/
├── aws/                    # AWS-specific configs
├── gcp/                    # Google Cloud configs  
├── azure/                  # Azure configs
├── digitalocean/           # DigitalOcean configs
├── kubernetes/             # Universal Kubernetes
├── docker/                 # Docker configurations
├── serverless/             # Serverless functions
├── traditional/            # Traditional server setup
├── monitoring/             # Observability configs
└── scripts/                # Deployment scripts
```

## 🔧 Environment Variables

All deployments support these environment variables:

```bash
# Core Settings
RED_GIANT_PORT=8080
RED_GIANT_HOST=0.0.0.0
RED_GIANT_WORKERS=8

# Performance
RED_GIANT_MAX_CHUNK_SIZE=262144
RED_GIANT_BUFFER_SIZE=1048576
RED_GIANT_MAX_CONNECTIONS=1000

# Security
RED_GIANT_TLS_ENABLED=false
RED_GIANT_TLS_CERT_PATH=/certs/tls.crt
RED_GIANT_TLS_KEY_PATH=/certs/tls.key

# Monitoring
RED_GIANT_METRICS_ENABLED=true
RED_GIANT_METRICS_PORT=9090
RED_GIANT_LOG_LEVEL=INFO

# Cloud-specific
CLOUD_PROVIDER=aws
REGION=us-east-1
```

## 🎯 Next Steps

1. Choose your deployment method above
2. Follow the specific guide in the corresponding folder
3. Your Red Giant server will be running in minutes!

Need help? Check the troubleshooting guides in each deployment folder.