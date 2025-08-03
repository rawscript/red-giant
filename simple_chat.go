// Red Giant Protocol - Simple Chat Demo
package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"
)

type SimpleChatMessage struct {
	From      string    `json:"from"`
	Message   string    `json:"message"`
	Timestamp time.Time `json:"timestamp"`
}

func sendMessage(username, message string) error {
	chatMsg := SimpleChatMessage{
		From:      username,
		Message:   message,
		Timestamp: time.Now(),
	}

	msgData, err := json.Marshal(chatMsg)
	if err != nil {
		return err
	}

	req, err := http.NewRequest("POST", "http://localhost:8080/upload", bytes.NewReader(msgData))
	if err != nil {
		return err
	}

	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("X-Peer-ID", username)
	req.Header.Set("X-File-Name", fmt.Sprintf("msg_%s_%d.json", username, time.Now().Unix()))

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	return nil
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: go run simple_chat.go <username>")
		return
	}

	username := os.Args[1]
	fmt.Printf("💬 Simple Red Giant Chat - User: %s\n", username)
	fmt.Println("Type messages and press Enter (type 'quit' to exit)")
	fmt.Println()

	scanner := bufio.NewScanner(os.Stdin)
	for {
		fmt.Printf("%s> ", username)
		if !scanner.Scan() {
			break
		}

		message := strings.TrimSpace(scanner.Text())
		if message == "" {
			continue
		}

		if message == "quit" {
			fmt.Println("👋 Goodbye!")
			break
		}

		if err := sendMessage(username, message); err != nil {
			fmt.Printf("❌ Failed to send message: %v\n", err)
		} else {
			fmt.Printf("✅ Message sent!\n")
		}
	}
}