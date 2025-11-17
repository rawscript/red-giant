# Test script for HTTPS over RGTP
Write-Host "Testing HTTPS over RGTP implementation" -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green

# Start the server in background
Write-Host "Starting HTTPS RGTP Server..." -ForegroundColor Yellow
Start-Process -FilePath ".\https_rgtp_server.exe" -ArgumentList "./www 8443" -NoNewWindow

# Wait a moment for server to start
Start-Sleep -Seconds 2

# Run the client
Write-Host "Running HTTPS RGTP Client..." -ForegroundColor Yellow
.\https_rgtp_client.exe https://localhost:8443/index.html downloaded_rgtp.html

# Check if download was successful
if (Test-Path "downloaded_rgtp.html") {
    Write-Host "SUCCESS: File downloaded successfully!" -ForegroundColor Green
    Write-Host "Downloaded file size: $((Get-Item downloaded_rgtp.html).Length) bytes" -ForegroundColor Cyan
    Write-Host "First 200 characters of downloaded file:" -ForegroundColor Cyan
    Get-Content downloaded_rgtp.html -First 1
} else {
    Write-Host "ERROR: File download failed" -ForegroundColor Red
}

Write-Host "Test completed." -ForegroundColor Green