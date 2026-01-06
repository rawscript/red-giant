// server.go
// Red Giant HTTP Server - December 2025
// Serves file via standard HTTP on TCP (no QUIC)

package main

import (
	"flag"
	"log"
	"net/http"

	"crypto/tls"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
)

var (
	certFile = flag.String("tls-cert", "cert.pem", "TLS certificate file")
	keyFile  = flag.String("tls-key", "key.pem", "TLS key file")
)

func main() {
	flag.Parse()

	if len(flag.Args()) == 0 {
		log.Fatal("Usage: go run server.go -tls-cert cert.pem -tls-key key.pem <file-to-expose>")
	}
	filePath := flag.Args()[0]

	// Create router with chi
	r := chi.NewRouter()
	r.Use(middleware.Logger)

	// Serve the specific file
	r.Get("/", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, filePath)
	})

	// Load TLS cert for HTTPS
	cert, err := tls.LoadX509KeyPair(*certFile, *keyFile)
	if err != nil {
		log.Fatal(err)
	}

	// Create HTTPS server using standard HTTP/TCP (no QUIC)
	server := &http.Server{
		Addr: ":8443", // Changed from 443 to 8443 to avoid requiring admin privileges
		TLSConfig: &tls.Config{
			Certificates: []tls.Certificate{cert},
			NextProtos:   []string{"http/1.1"}, // Explicitly use HTTP/1.1, not HTTP/3
		},
		Handler: r,
	}

	log.Printf("HTTPS server running on :8443 - exposing %s", filePath)
	log.Printf("Access from browser: https://localhost:8443/")

	// Start HTTPS server (no QUIC/HTTP3/WebTransport)
	if err := server.ListenAndServeTLS(*certFile, *keyFile); err != nil {
		log.Fatal(err)
	}
}
