#!/bin/bash

# Red Giant Protocol - Heroku Deployment Script

set -e

source ../scripts/common.sh

APP_NAME=${APP_NAME:-red-giant-$(date +%s)}

print_info "Deploying to Heroku..."
print_info "App name: $APP_NAME"

# Create Heroku app
create_app() {
    print_info "Creating Heroku app..."
    
    if heroku apps:info $APP_NAME &> /dev/null; then
        print_info "App $APP_NAME already exists"
    else
        heroku create $APP_NAME --stack container
        print_success "Heroku app created: $APP_NAME"
    fi
}

# Set environment variables
set_config() {
    print_info "Setting configuration..."
    
    heroku config:set \
        RED_GIANT_PORT=\$PORT \
        RED_GIANT_HOST=0.0.0.0 \
        RED_GIANT_WORKERS=4 \
        CLOUD_PROVIDER=heroku \
        --app $APP_NAME
    
    print_success "Configuration set"
}

# Deploy application
deploy_app() {
    print_info "Deploying application..."
    
    # Add Heroku remote if not exists
    if ! git remote | grep -q heroku; then
        heroku git:remote -a $APP_NAME
    fi
    
    # Push to Heroku
    git push heroku main
    
    # Scale dynos
    heroku ps:scale web=2 --app $APP_NAME
    
    print_success "Application deployed!"
}

# Main deployment flow
main() {
    # Check if we're in a git repository
    if [ ! -d ".git" ]; then
        print_error "This must be run from the root of the Red Giant git repository"
        exit 1
    fi
    
    create_app
    set_config
    deploy_app
    
    APP_URL=$(heroku apps:info $APP_NAME --json | jq -r '.app.web_url')
    
    print_success "Heroku deployment completed!"
    print_info "App URL: $APP_URL"
    print_info "Logs: heroku logs --tail --app $APP_NAME"
}

main "$@"