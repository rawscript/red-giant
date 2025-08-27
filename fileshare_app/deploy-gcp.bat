@echo off
REM Red Giant File Share App - Google Cloud Deployment Script (Windows)

echo 🚀 Red Giant File Share App - Google Cloud Deployer
echo ==================================================

REM Check if gcloud is available
gcloud --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Error: gcloud not found. Please install Google Cloud SDK.
    echo Download from: https://cloud.google.com/sdk/
    pause
    exit /b 1
)

REM Check if docker is available
docker --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Error: Docker not found. Please install Docker Desktop.
    echo Download from: https://www.docker.com/products/docker-desktop
    pause
    exit /b 1
)

REM Check Google Cloud authentication
gcloud auth list --filter=status:ACTIVE --format="value(account)" >nul 2>&1
if errorlevel 1 (
    echo ❌ Error: GCP not authenticated. Run 'gcloud auth login' first.
    pause
    exit /b 1
)

echo ℹ️  Checking environment variables...

REM Check if RED_GIANT_URL is set
if "%RED_GIANT_URL%"=="" (
    echo ⚠️  RED_GIANT_URL environment variable not set
    echo ℹ️  Using default: http://redgiant-server:8080
    set RED_GIANT_URL=http://redgiant-server:8080
)

echo ℹ️  Getting GCP project ID...
for /f "tokens=*" %%i in ('gcloud config get-value project 2^>nul') do set PROJECT_ID=%%i

if "%PROJECT_ID%"=="" (
    echo ❌ Error: Unable to determine GCP project ID
    pause
    exit /b 1
)

echo ℹ️  Building Docker image...
set IMAGE_NAME=gcr.io/%PROJECT_ID%/fileshare-app
docker build -t %IMAGE_NAME% .

if errorlevel 1 (
    echo ❌ Error: Failed to build Docker image
    pause
    exit /b 1
)

echo ℹ️  Pushing Docker image to Google Container Registry...
docker push %IMAGE_NAME%

if errorlevel 1 (
    echo ❌ Error: Failed to push Docker image
    pause
    exit /b 1
)

echo ℹ️  Deploying to Cloud Run...
gcloud run deploy fileshare-app ^
    --image %IMAGE_NAME% ^
    --platform managed ^
    --allow-unauthenticated ^
    --port 3000 ^
    --set-env-vars RED_GIANT_URL=%RED_GIANT_URL%

if errorlevel 1 (
    echo ❌ Error: Failed to deploy to Cloud Run
    pause
    exit /b 1
)

echo ✅ Deployment completed!
echo ℹ️  Your application will be available at:
for /f "tokens=*" %%i in ('gcloud run services describe fileshare-app --platform managed --format "value^(status.url^)"') do set SERVICE_URL=%%i
echo %SERVICE_URL%

pause