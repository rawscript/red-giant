// Red Giant Protocol - P2P Chat System
package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

type ChatMessage struct {
	ID        string    `json:"id"`
	From      string    `json:"from"`
	To        string    `json:"to"`
	Message   string    `json:"message"`
	Timestamp time.Time `json:"timestamp"`
	Type      string    `json:"type"` // "message", "file", "broadcast"
}

type RedGiantChat struct {
	ServerURL string
	Username  string
	Client    *http.Client
}

func NewRedGiantChat(serverURL, username string) *RedGiantChat {
	return &RedGiantChat{
		ServerURL: serverURL,
		Username:  username,
		Client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// Send a message through Red Giant Protocol
func (c *RedGiantChat) SendMessage(to, message string) error {
	chatMsg := ChatMessage{
		ID:        fmt.Sprintf("msg_%d", time.Now().UnixNano()),
		From:      c.Username,
		To:        to,
		Message:   message,
		Timestamp: time.Now(),
		Type:      "message",
	}

	// Serialize message
	msgData, err := json.Marshal(chatMsg)
	if err != nil {
		return fmt.Errorf("failed to serialize message: %w", err)
	}

	// Send via Red Giant Protocol
	url := fmt.Sprintf("%s/upload", c.ServerURL)
	req, err := http.NewRequest("POST", url, bytes.NewReader(msgData))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("X-Peer-ID", c.Username)
	req.Header.Set("X-File-Name", fmt.Sprintf("chat_%s_%d.json", to, time.Now().Unix()))

	resp, err := c.Client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send message: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("server error: %d", resp.StatusCode)
	}

	return nil
}

// Broadcast message to all peers
func (c *RedGiantChat) BroadcastMessage(message string) error {
	return c.SendMessage("*", message)
}

// Get messages from the network
func (c *RedGiantChat) GetMessages() ([]ChatMessage, error) {
	url := fmt.Sprintf("%s/search?q=chat_", c.ServerURL)
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("X-Peer-ID", c.Username)

	resp, err := c.Client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("failed to get messages: %w", err)
	}
	defer resp.Body.Close()

	var result struct {
		Files []struct {
			ID   string `json:"id"`
			Name string `json:"name"`
		} `json:"files"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, fmt.Errorf("failed to parse response: %w", err)
	}

	var messages []ChatMessage
	for _, file := range result.Files {
		// Download and parse each chat message
		msgData, err := c.downloadFile(file.ID)
		if err != nil {
			continue
		}

		var chatMsg ChatMessage
		if err := json.Unmarshal(msgData, &chatMsg); err != nil {
			continue
		}

		// Filter messages for this user
		if chatMsg.To == c.Username || chatMsg.To == "*" || chatMsg.From == c.Username {
			messages = append(messages, chatMsg)
		}
	}

	return messages, nil
}

// Download file data
func (c *RedGiantChat) downloadFile(fileID string) ([]byte, error) {
	url := fmt.Sprintf("%s/download/%s", c.ServerURL, fileID)
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("X-Peer-ID", c.Username)

	resp, err := c.Client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("download failed with status %d", resp.StatusCode)
	}

	return io.ReadAll(resp.Body)
}

// Start interactive chat session
func (c *RedGiantChat) StartChat() {
	fmt.Printf("🚀 Red Giant Protocol Chat\n")
	fmt.Printf("👤 Username: %s\n", c.Username)
	fmt.Printf("🌐 Server: %s\n", c.ServerURL)
	fmt.Printf("💬 Type messages and press Enter. Type '/help' for commands.\n\n")

	// Start message polling in background
	go c.pollMessages()

	// Interactive input loop
	scanner := bufio.NewScanner(os.Stdin)
	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}

		input := strings.TrimSpace(scanner.Text())
		if input == "" {
			continue
		}

		if strings.HasPrefix(input, "/") {
			c.handleCommand(input)
		} else {
			// Send as broadcast message
			if err := c.BroadcastMessage(input); err != nil {
				fmt.Printf("❌ Failed to send message: %v\n", err)
			}
		}
	}
}

// Handle chat commands
func (c *RedGiantChat) handleCommand(command string) {
	parts := strings.Fields(command)
	if len(parts) == 0 {
		return
	}

	switch parts[0] {
	case "/help":
		fmt.Println("📖 Red Giant Chat Commands:")
		fmt.Println("  /help              - Show this help")
		fmt.Println("  /msg <user> <text> - Send private message")
		fmt.Println("  /list              - List online users")
		fmt.Println("  /history           - Show message history")
		fmt.Println("  /quit              - Exit chat")

	case "/msg":
		if len(parts) < 3 {
			fmt.Println("Usage: /msg <username> <message>")
			return
		}
		to := parts[1]
		message := strings.Join(parts[2:], " ")
		if err := c.SendMessage(to, message); err != nil {
			fmt.Printf("❌ Failed to send private message: %v\n", err)
		} else {
			fmt.Printf("📤 Private message sent to %s\n", to)
		}

	case "/history":
		messages, err := c.GetMessages()
		if err != nil {
			fmt.Printf("❌ Failed to get message history: %v\n", err)
			return
		}
		fmt.Printf("📜 Message History (%d messages):\n", len(messages))
		for _, msg := range messages {
			if msg.To == "*" {
				fmt.Printf("  [%s] %s: %s\n", msg.Timestamp.Format("15:04"), msg.From, msg.Message)
			} else {
				fmt.Printf("  [%s] %s -> %s: %s\n", msg.Timestamp.Format("15:04"), msg.From, msg.To, msg.Message)
			}
		}

	case "/quit":
		fmt.Println("👋 Goodbye!")
		os.Exit(0)

	default:
		fmt.Printf("❌ Unknown command: %s (type /help for commands)\n", parts[0])
	}
}

// Poll for new messages
func (c *RedGiantChat) pollMessages() {
	lastCheck := time.Now()
	
	for {
		time.Sleep(2 * time.Second) // Poll every 2 seconds
		
		messages, err := c.GetMessages()
		if err != nil {
			continue
		}

		// Show new messages since last check
		for _, msg := range messages {
			if msg.Timestamp.After(lastCheck) && msg.From != c.Username {
				if msg.To == "*" {
					fmt.Printf("\n💬 %s: %s\n> ", msg.From, msg.Message)
				} else if msg.To == c.Username {
					fmt.Printf("\n📩 %s (private): %s\n> ", msg.From, msg.Message)
				}
			}
		}
		
		lastCheck = time.Now()
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("🚀 Red Giant Protocol - P2P Chat System")
		fmt.Println("Usage:")
		fmt.Println("  go run red_giant_chat.go <username>")
		fmt.Println("  go run red_giant_chat.go <username> <server-url>")
		fmt.Println("")
		fmt.Println("Examples:")
		fmt.Println("  go run red_giant_chat.go alice")
		fmt.Println("  go run red_giant_chat.go bob http://localhost:8080")
		return
	}

	username := os.Args[1]
	serverURL := "http://localhost:8080"
	if len(os.Args) > 2 {
		serverURL = os.Args[2]
	}

	chat := NewRedGiantChat(serverURL, username)
	chat.StartChat()
}