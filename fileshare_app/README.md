# Red Giant File Share App

A high-performance file sharing application built on top of the Red Giant Protocol, delivering 500+ MB/s transfer speeds.

## Features

- 🚀 **Lightning-fast transfers** - Powered by Red Giant Protocol's 500+ MB/s throughput
- 👤 **User authentication** - Secure registration and login system
- 📁 **File sharing** - Share files with other users by ID
- 🌐 **Web interface** - Beautiful, responsive web UI
- 🛡️ **Secure** - Password hashing with bcrypt
- 📱 **Mobile-friendly** - Works on all devices

## Prerequisites

- Go 1.21+
- Red Giant Protocol server deployed (locally or in cloud)
- SQLite3

## Installation

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd fileshare_app
   ```

2. Install dependencies:
   ```bash
   go mod tidy
   ```

3. Set environment variables:
   ```bash
   export RED_GIANT_URL=http://your-red-giant-server:8080
   ```

## Running the Application

1. Start the server:
   ```bash
   go run .
   ```

2. Open your browser and navigate to:
   ```
   http://localhost:3000
   ```

## Deployment

### Deploying to Google Cloud Platform

1. Build the application:
   ```bash
   go build -o fileshare-app .
   ```

2. Deploy using Google Cloud Run:
   ```bash
   gcloud run deploy fileshare-app \
     --source . \
     --platform managed \
     --allow-unauthenticated \
     --set-env-vars RED_GIANT_URL=https://your-red-giant-server.com
   ```

### Deploying with Docker

1. Build the Docker image:
   ```bash
   docker build -t fileshare-app .
   ```

2. Run the container:
   ```bash
   docker run -p 3000:3000 \
     -e RED_GIANT_URL=http://your-red-giant-server:8080 \
     fileshare-app
   ```

## API Endpoints

- `POST /api/register` - Register a new user
- `POST /api/login` - Login user
- `POST /api/upload` - Upload file through Red Giant
- `POST /api/share` - Share file with another user
- `GET /api/files` - List shared files
- `GET /api/download/{file_id}` - Download file through Red Giant

## How It Works

1. **User Registration/Login** - Users create accounts and log in securely
2. **File Upload** - Files are uploaded through the web interface to the Red Giant server
3. **File Sharing** - Users can share file IDs with other users
4. **File Download** - Recipients can download shared files at 500+ MB/s speeds

## Performance

The application leverages the Red Giant Protocol's C-core optimizations to deliver:
- 500+ MB/s transfer speeds
- Zero-copy operations
- Multi-core processing
- Minimal memory overhead

## Security

- Passwords are hashed using bcrypt
- Secure session management
- Input validation and sanitization
- Protection against common web vulnerabilities

## License

This project is licensed under the MIT License.