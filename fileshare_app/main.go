package main

import (
	"context"
	"database/sql"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

// FileShareApp represents the main application
type FileShareApp struct {
	db       *sql.DB
	redGiant *RedGiantClient
	server   *http.Server
}

// User represents a user in the system
type User struct {
	ID       int    `json:"id"`
	Username string `json:"username"`
	Email    string `json:"email"`
	Password string `json:"password"` // In a real app, this would be hashed
}

// SharedFile represents a file shared between users
type SharedFile struct {
	ID          int       `json:"id"`
	FileID      string    `json:"file_id"`
	FileName    string    `json:"file_name"`
	UploaderID  int       `json:"uploader_id"`
	RecipientID int       `json:"recipient_id"`
	SharedAt    time.Time `json:"shared_at"`
	Downloaded  bool      `json:"downloaded"`
}

// RedGiantClient represents the client for Red Giant Protocol
type RedGiantClient struct {
	BaseURL string
}

// NewFileShareApp creates a new file sharing application
func NewFileShareApp() *FileShareApp {
	// Initialize database
	db, err := sql.Open("sqlite3", "./fileshare.db")
	if err != nil {
		log.Fatal(err)
	}

	// Create tables if they don't exist
	createTables(db)

	// Initialize Red Giant client
	redGiantURL := os.Getenv("RED_GIANT_URL")
	if redGiantURL == "" {
		redGiantURL = "http://localhost:8080" // Default
	}

	app := &FileShareApp{
		db: db,
		redGiant: &RedGiantClient{
			BaseURL: redGiantURL,
		},
	}

	return app
}

// createTables creates the necessary database tables
func createTables(db *sql.DB) {
	queries := []string{
		`CREATE TABLE IF NOT EXISTS users (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			username TEXT UNIQUE NOT NULL,
			email TEXT UNIQUE NOT NULL,
			password TEXT NOT NULL,
			created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
		)`,
		`CREATE TABLE IF NOT EXISTS shared_files (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			file_id TEXT NOT NULL,
			file_name TEXT NOT NULL,
			uploader_id INTEGER NOT NULL,
			recipient_id INTEGER NOT NULL,
			shared_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
			downloaded BOOLEAN DEFAULT FALSE,
			FOREIGN KEY (uploader_id) REFERENCES users(id),
			FOREIGN KEY (recipient_id) REFERENCES users(id)
		)`,
	}

	for _, query := range queries {
		_, err := db.Exec(query)
		if err != nil {
			log.Fatal(err)
		}
	}
}

// Close closes the application resources
func (app *FileShareApp) Close() {
	app.db.Close()
}

// Start starts the HTTP server
func (app *FileShareApp) Start(port int) error {
	mux := http.NewServeMux()

	// API endpoints
	mux.HandleFunc("/api/register", app.handleRegister)
	mux.HandleFunc("/api/login", app.handleLogin)
	mux.HandleFunc("/api/upload", app.handleUpload)
	mux.HandleFunc("/api/share", app.handleShare)
	mux.HandleFunc("/api/files", app.handleListFiles)
	mux.HandleFunc("/api/download/", app.handleDownload)

	// Serve frontend files
	mux.Handle("/", http.FileServer(http.Dir("./web")))

	app.server = &http.Server{
		Addr:         fmt.Sprintf(":%d", port),
		Handler:      mux,
		ReadTimeout:  30 * time.Second,
		WriteTimeout: 30 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	log.Printf("File sharing app starting on port %d", port)
	log.Printf("Frontend available at http://localhost:%d", port)
	log.Printf("API endpoints at http://localhost:%d/api/", port)

	return app.server.ListenAndServe()
}

// Stop stops the HTTP server gracefully
func (app *FileShareApp) Stop() error {
	if app.server != nil {
		ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
		defer cancel()
		return app.server.Shutdown(ctx)
	}
	return nil
}

func main() {
	app := NewFileShareApp()
	defer app.Close()

	// Start server
	go func() {
		if err := app.Start(3000); err != nil && err != http.ErrServerClosed {
			log.Fatalf("Server failed to start: %v", err)
		}
	}()

	// Wait for interrupt signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan

	log.Println("Shutting down server...")
	if err := app.Stop(); err != nil {
		log.Printf("Server shutdown error: %v", err)
	}
	log.Println("Server stopped")
}
