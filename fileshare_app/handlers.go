package main

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"time"

	"golang.org/x/crypto/bcrypt"
)

// RegisterRequest represents the registration request
type RegisterRequest struct {
	Username string `json:"username"`
	Email    string `json:"email"`
	Password string `json:"password"`
}

// LoginRequest represents the login request
type LoginRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

// UploadResponse represents the upload response
type UploadResponse struct {
	FileID     string  `json:"file_id"`
	FileName   string  `json:"file_name"`
	Size       int64   `json:"size"`
	Throughput float64 `json:"throughput_mbps"`
}

// ShareRequest represents the file sharing request
type ShareRequest struct {
	FileID      string `json:"file_id"`
	RecipientID int    `json:"recipient_id"`
}

// APIResponse represents a standard API response
type APIResponse struct {
	Success bool        `json:"success"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

// handleRegister handles user registration
func (app *FileShareApp) handleRegister(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req RegisterRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		app.sendError(w, "Invalid request body", http.StatusBadRequest)
		return
	}

	// Hash password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		app.sendError(w, "Failed to hash password", http.StatusInternalServerError)
		return
	}

	// Insert user into database
	result, err := app.db.Exec("INSERT INTO users (username, email, password) VALUES (?, ?, ?)",
		req.Username, req.Email, string(hashedPassword))
	if err != nil {
		app.sendError(w, "Failed to create user", http.StatusInternalServerError)
		return
	}

	userID, _ := result.LastInsertId()
	log.Printf("User registered: %s (ID: %d)", req.Username, userID)

	app.sendSuccess(w, "User registered successfully", map[string]interface{}{
		"user_id":  userID,
		"username": req.Username,
	})
}

// handleLogin handles user login
func (app *FileShareApp) handleLogin(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req LoginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		app.sendError(w, "Invalid request body", http.StatusBadRequest)
		return
	}

	// Get user from database
	var user User
	var hashedPassword string
	err := app.db.QueryRow("SELECT id, username, email, password FROM users WHERE username = ?",
		req.Username).Scan(&user.ID, &user.Username, &user.Email, &hashedPassword)
	if err != nil {
		if err == sql.ErrNoRows {
			app.sendError(w, "Invalid credentials", http.StatusUnauthorized)
		} else {
			app.sendError(w, "Database error", http.StatusInternalServerError)
		}
		return
	}

	// Check password
	if err := bcrypt.CompareHashAndPassword([]byte(hashedPassword), []byte(req.Password)); err != nil {
		app.sendError(w, "Invalid credentials", http.StatusUnauthorized)
		return
	}

	log.Printf("User logged in: %s (ID: %d)", user.Username, user.ID)

	app.sendSuccess(w, "Login successful", map[string]interface{}{
		"user_id":  user.ID,
		"username": user.Username,
	})
}

// handleUpload handles file upload through Red Giant
func (app *FileShareApp) handleUpload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Parse multipart form
	err := r.ParseMultipartForm(32 << 20) // 32MB max memory
	if err != nil {
		app.sendError(w, "Failed to parse form", http.StatusBadRequest)
		return
	}

	// Get uploaded file
	file, handler, err := r.FormFile("file")
	if err != nil {
		app.sendError(w, "Failed to get uploaded file", http.StatusBadRequest)
		return
	}
	defer file.Close()

	// Create temporary file
	tempFile, err := os.CreateTemp("", "upload_*_"+handler.Filename)
	if err != nil {
		app.sendError(w, "Failed to create temporary file", http.StatusInternalServerError)
		return
	}
	defer os.Remove(tempFile.Name())
	defer tempFile.Close()

	// Copy uploaded file to temporary file
	_, err = io.Copy(tempFile, file)
	if err != nil {
		app.sendError(w, "Failed to save file", http.StatusInternalServerError)
		return
	}

	// Upload to Red Giant
	uploadResult, err := app.redGiant.UploadFile(tempFile.Name())
	if err != nil {
		app.sendError(w, "Failed to upload to Red Giant: "+err.Error(), http.StatusInternalServerError)
		return
	}

	app.sendSuccess(w, "File uploaded successfully", uploadResult)
}

// handleShare handles file sharing between users
func (app *FileShareApp) handleShare(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var req ShareRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		app.sendError(w, "Invalid request body", http.StatusBadRequest)
		return
	}

	// Get user ID from session/header (simplified for this example)
	// In a real app, you would validate the session
	uploaderID := 1 // Default for demo

	// Insert shared file record
	_, err := app.db.Exec("INSERT INTO shared_files (file_id, file_name, uploader_id, recipient_id) VALUES (?, ?, ?, ?)",
		req.FileID, "shared_file", uploaderID, req.RecipientID)
	if err != nil {
		app.sendError(w, "Failed to share file", http.StatusInternalServerError)
		return
	}

	app.sendSuccess(w, "File shared successfully", nil)
}

// handleListFiles handles listing shared files
func (app *FileShareApp) handleListFiles(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Get user ID from session/header (simplified for this example)
	userID := 1 // Default for demo

	// Query shared files
	rows, err := app.db.Query(`
		SELECT sf.id, sf.file_id, sf.file_name, sf.shared_at, sf.downloaded, u.username as uploader
		FROM shared_files sf
		JOIN users u ON sf.uploader_id = u.id
		WHERE sf.recipient_id = ?
		ORDER BY sf.shared_at DESC
	`, userID)
	if err != nil {
		app.sendError(w, "Failed to query files", http.StatusInternalServerError)
		return
	}
	defer rows.Close()

	var files []map[string]interface{}
	for rows.Next() {
		var id int
		var fileID, fileName, uploader string
		var sharedAt time.Time
		var downloaded bool

		err := rows.Scan(&id, &fileID, &fileName, &sharedAt, &downloaded, &uploader)
		if err != nil {
			app.sendError(w, "Failed to scan file", http.StatusInternalServerError)
			return
		}

		files = append(files, map[string]interface{}{
			"id":         id,
			"file_id":    fileID,
			"file_name":  fileName,
			"shared_at":  sharedAt,
			"downloaded": downloaded,
			"uploader":   uploader,
		})
	}

	app.sendSuccess(w, "Files retrieved successfully", files)
}

// handleDownload handles file download through Red Giant
func (app *FileShareApp) handleDownload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Extract file ID from URL
	fileID := r.URL.Path[len("/api/download/"):]

	// Create temporary file for download
	tempFile, err := os.CreateTemp("", "download_*")
	if err != nil {
		app.sendError(w, "Failed to create temporary file", http.StatusInternalServerError)
		return
	}
	defer os.Remove(tempFile.Name())
	defer tempFile.Close()

	// Download from Red Giant
	err = app.redGiant.DownloadFile(fileID, tempFile.Name())
	if err != nil {
		app.sendError(w, "Failed to download from Red Giant: "+err.Error(), http.StatusInternalServerError)
		return
	}

	// Get file info
	fileInfo, err := tempFile.Stat()
	if err != nil {
		app.sendError(w, "Failed to get file info", http.StatusInternalServerError)
		return
	}

	// Set headers for download
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=%s", filepath.Base(tempFile.Name())))
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Length", strconv.FormatInt(fileInfo.Size(), 10))

	// Copy file to response
	tempFile.Seek(0, 0)
	_, err = io.Copy(w, tempFile)
	if err != nil {
		log.Printf("Failed to send file: %v", err)
		return
	}

	log.Printf("File downloaded: %s", fileID)
}

// sendSuccess sends a successful JSON response
func (app *FileShareApp) sendSuccess(w http.ResponseWriter, message string, data interface{}) {
	response := APIResponse{
		Success: true,
		Message: message,
		Data:    data,
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(response)
}

// sendError sends an error JSON response
func (app *FileShareApp) sendError(w http.ResponseWriter, message string, statusCode int) {
	response := APIResponse{
		Success: false,
		Message: message,
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	json.NewEncoder(w).Encode(response)
}