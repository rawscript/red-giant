package main

import (
  "context"
  "crypto/rand"
  "encoding/binary"
  "fmt"
  "log"
  "net/http"

  "golang.org/x/exp/webtransport"
)

func main() {
  mux := http.NewServeMux()
  mux.HandleFunc("/rgtp", handleWebTransport)
  mux.HandleFunc("/manifest", func(w http.ResponseWriter, r *http.Request) {
    id := make([]byte, 16)
    rand.Read(id)
    w.Header().Set("Content-Type", "application/json")
    fmt.Fprintf(w, `{"exposureId":[%d,%d]}`,
      binary.BigEndian.Uint64(id[:8]), binary.BigEndian.Uint64(id[8:]))
  })

  log.Fatal(http.ListenAndServeTLS(":4433", "server.crt", "server.key", mux))
}

func handleWebTransport(w http.ResponseWriter, r *http.Request) {
  sess, err := webtransport.Upgrade(w, r)
  if err != nil { log.Println(err); return }

  go func() {
    for {
      stream, err := sess.AcceptStream()
      if err != nil { return }
      go handleStream(stream)
    }
  }()
}

func handleStream(stream *webtransport.Stream) {
  defer stream.Close()
  buf := make([]byte, 65536)
  n, _ := stream.Read(buf)
  fmt.Printf("New exposure received (%d bytes header)\n", n)
  // In real version: feed into your rgtp_core.c via CGO or spawn udp_expose_file
  // For demo: just echo success
}