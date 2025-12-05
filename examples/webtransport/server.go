// server.go
// Red Giant WebTransport Server — December 2025
// Serves file via WebTransport over HTTP/3 + QUIC on UDP 443

package main

import (
    "context"
    "flag"
    "fmt"
    "io"
    "log"
    "net/http"
    "os"
    "time"

    "crypto/tls"

    "github.com/quic-go/quic-go"
    "github.com/quic-go/quic-go/http3"
    "github.com/quic-go/webtransport-go"
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

    file, err := os.Open(filePath)
    if err != nil {
        log.Fatal(err)
    }
    defer file.Close()

    // Load TLS cert
    cert, err := tls.LoadX509KeyPair(*certFile, *keyFile)
    if err != nil {
        log.Fatal(err)
    }

    mux := http.NewServeMux()
    mux.HandleFunc("/webtransport", func(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Content-Type", "text/plain")
        fmt.Fprint(w, "WebTransport ready")
    })

    // HTTP/3 server
    server := &http3.Server{
        Addr: ":443",
        TLSConfig: &tls.Config{
            Certificates: []tls.Certificate{cert},
            NextProtos:   []string{"h3"},
        },
        Handler: mux,
    }

    // WebTransport handler
    wtServer := &webtransport.Server{
        Server: server,
    }

    wtServer.BatonHandler = func(ctx context.Context, session *webtransport.Session) *webtransport.Baton {
        // Accept unidirectional stream from client
        stream, err := session.AcceptUnidirectionalStream(ctx)
        if err != nil {
            log.Printf("Failed to accept stream: %v", err)
            return nil
        }

        // Stream the file to the stream
        _, err = io.Copy(stream, file)
        if err != nil {
            log.Printf("Failed to copy file to stream: %v", err)
        }

        return nil
    }

    log.Printf("WebTransport server running on :443 — exposing %s", filePath)
    log.Printf("Connect from browser: new WebTransport('https://localhost:443/webtransport')")

    if err := wtServer.Serve(); err != nil {
        log.Fatal(err)
    }
}