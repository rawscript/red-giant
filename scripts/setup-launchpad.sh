#!/bin/bash
# Setup Launchpad repository for RED_GIANT
set -euo pipefail

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║   RED GIANT - Launchpad Repository Setup                       ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}ℹ${NC} $1"; }
log_success() { echo -e "${GREEN}✓${NC} $1"; }
log_error() { echo -e "${RED}✗${NC} $1"; }
log_warning() { echo -e "${YELLOW}⚠${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

# 1. Check prerequisites
log_info "Checking prerequisites..."

if ! command -v git &> /dev/null; then
    log_error "git not found"
    exit 1
fi
log_success "git found"

if ! git config user.email &>/dev/null; then
    log_error "Git not configured. Run: git config --global user.email 'your@email.com'"
    exit 1
fi
log_success "git configured"

echo ""

# 2. Get Launchpad username
log_info "Launchpad Configuration"
echo ""

read -p "Enter your Launchpad username: " LP_USERNAME

if [[ -z "$LP_USERNAME" ]]; then
    log_error "Launchpad username required"
    exit 1
fi

log_success "Username: $LP_USERNAME"

# Validate SSH key setup
log_info "Checking SSH key for Launchpad..."
if [[ ! -f ~/.ssh/id_rsa ]] && [[ ! -f ~/.ssh/id_ed25519 ]]; then
    log_warning "No SSH key found"
    read -p "Generate new SSH key? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -N ""
        log_success "SSH key generated"
        echo ""
        echo "Add this key to Launchpad:"
        echo "1. Go to https://launchpad.net/~/+editsshkeys"
        echo "2. Copy and paste:"
        cat ~/.ssh/id_ed25519.pub
        echo ""
        read -p "Press Enter after adding key to Launchpad..."
    else
        log_error "SSH key required for Launchpad"
        exit 1
    fi
else
    log_success "SSH key found"
fi

echo ""

# 3. Setup Launchpad repository
log_info "Setting up Launchpad repository..."
echo ""

cd "$PROJECT_ROOT"

# Check if origin already exists
if git remote | grep -q "^origin$"; then
    CURRENT_ORIGIN=$(git remote get-url origin)
    log_warning "Remote 'origin' already exists: $CURRENT_ORIGIN"
    
    read -p "Replace with Launchpad remote? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git remote remove origin
        log_success "Removed old origin"
    else
        read -p "Enter alternate remote name (e.g., launchpad): " REMOTE_NAME
        if [[ -z "$REMOTE_NAME" ]]; then
            REMOTE_NAME="launchpad"
        fi
    fi
else
    REMOTE_NAME="origin"
fi

# Add Launchpad remote
LP_REPO_URL="git+ssh://${LP_USERNAME}@git.launchpad.net/~${LP_USERNAME}/+git/RED_GIANT"
git remote add "$REMOTE_NAME" "$LP_REPO_URL" 2>/dev/null || git remote set-url "$REMOTE_NAME" "$LP_REPO_URL"

log_success "Added Launchpad remote: $REMOTE_NAME"
log_info "URL: $LP_REPO_URL"

echo ""

# 4. Create Launchpad project (if needed)
log_info "Launchpad Project Information"
echo ""

cat <<EOF
To create the project on Launchpad:

1. Go to https://launchpad.net/projects/+new
2. Fill in project details:
   - Name: RED_GIANT (or preferred name)
   - Display name: Red Giant Transport Protocol
   - Title: Red Giant Exposure-based Transport Protocol
   - Summary: Layer 4 transport protocol with exposure-based congestion control
   - Homepage: https://github.com/rawscript/red-giant
   - Category: Development → Libraries
   
3. Click "Create Project"

4. Once project is created, add yourself as developer:
   - Go to https://launchpad.net/~RED_GIANT/+administer-members
   - Add your user

EOF

read -p "Have you created the project on Launchpad? (y/n) " -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_warning "Create project at https://launchpad.net/projects/+new first"
fi

echo ""

# 5. Push to Launchpad
log_info "Pushing repository to Launchpad..."
echo ""

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

log_info "Current branch: $CURRENT_BRANCH"
read -p "Branch to push (default: master): " PUSH_BRANCH
PUSH_BRANCH=${PUSH_BRANCH:-master}

log_info "Checking out $PUSH_BRANCH..."
git checkout -B "$PUSH_BRANCH" || git checkout "$PUSH_BRANCH"

log_info "Pushing to Launchpad (this may take a moment)..."
if git push --set-upstream "$REMOTE_NAME" "$PUSH_BRANCH"; then
    log_success "Repository pushed to Launchpad!"
else
    log_error "Push failed. Check SSH key configuration."
    log_info "Troubleshooting:"
    echo "1. Verify Launchpad SSH key: https://launchpad.net/~/+editsshkeys"
    echo "2. Test SSH: ssh -v git.launchpad.net"
    echo "3. Create project first: https://launchpad.net/projects/+new"
    exit 1
fi

echo ""

# 6. Setup tracking branches
log_info "Setting up tracking branches..."

git push "$REMOTE_NAME" main || log_warning "Could not push main branch"
git push "$REMOTE_NAME" develop || log_warning "Could not push develop branch"

log_success "All branches pushed"

echo ""

# 7. Verify setup
log_info "Verifying Launchpad setup..."
echo ""

echo "Repository: https://launchpad.net/~${LP_USERNAME}/+git/RED_GIANT"
echo "Git URL: $LP_REPO_URL"
echo ""

git remote -v | grep "$REMOTE_NAME"

echo ""
log_success "Setup complete!"

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║   Next Steps                                                   ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "1. Configure Launchpad PPA (for releases):"
echo "   https://launchpad.net/~${LP_USERNAME}/+new-ppa"
echo ""
echo "2. Link GitHub repository:"
echo "   Settings → Import Code → GitHub → red-giant"
echo ""
echo "3. Setup automatic syncing:"
echo "   Launchpad → Import Settings"
echo ""
echo "4. Create releases:"
echo "   git tag -s v1.0.0 -m 'Release v1.0.0'"
echo "   git push $REMOTE_NAME v1.0.0"
echo ""
echo "5. Build packages:"
echo "   dput ppa:${LP_USERNAME}/red-giant ../librgtp_*.changes"
echo ""
