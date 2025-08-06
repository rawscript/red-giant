
#!/bin/bash
# Red Giant Protocol - Replit Deployment Script

echo "🚀 Deploying Red Giant Protocol on Replit..."
echo "============================================="

# Set optimal Replit configuration
export RED_GIANT_PORT=8080
export RED_GIANT_HOST=0.0.0.0
export RED_GIANT_WORKERS=$(nproc)
export RED_GIANT_BUFFER_SIZE=2097152  # 2MB for Replit optimization

# Create storage directory
mkdir -p ./storage/{text,image,video,audio,application}

# Compile C core (if needed)
echo "📦 Compiling C core..."
go build -o red_giant_server red_giant_test_server.go

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "✅ C core compiled successfully"
else
    echo "❌ C core compilation failed"
    exit 1
fi

# Start Red Giant server
echo "🚀 Starting Red Giant Protocol Server..."
echo "   Host: $RED_GIANT_HOST"
echo "   Port: $RED_GIANT_PORT"
echo "   Workers: $RED_GIANT_WORKERS"
echo "   Buffer Size: $RED_GIANT_BUFFER_SIZE bytes"
echo ""
echo "🌐 Your server will be available at:"
echo "   Internal: http://localhost:$RED_GIANT_PORT"
echo "   External: https://$(echo $REPL_SLUG).$(echo $REPL_OWNER).repl.co"
echo ""
echo "📊 Monitor performance: /metrics"
echo "🔍 Web interface: /"
echo "🆘 Health check: /health"
echo ""

# Run the server
./red_giant_server
