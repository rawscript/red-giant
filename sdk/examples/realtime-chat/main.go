// Red Giant Protocol - Real-time P2P Chat Example
// Demonstrates high-speed messaging using Red Giant's C core performance
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"redgiant-sdk" // Import Red Giant SDK
)

type ChatMessage struct {
	ID        string    `json:"id"`
	From      string    `json:"from"`
	To        string    `json:"to"`
	Content   string    `json:"content"`
	Timestamp time.Time `json:"timestamp"`
	Type      string    `json:"type"` // "public", "private", "system"
}

type ChatRoom struct {
	client   *redgiant.Client
	username string
	roomID   string
	messages []ChatMessage
}

func main() {
	if len(os.Args) < 2 {
		showUsage()
		return
	}

	username := os.Args[1]
	roomID := "general"
	if len(os.Args) > 2 {
		roomID = os.Args[2]
	}

	// Connect to Red Giant server
	serverURL := os.Getenv("RED_GIANT_SERVER")
	if serverURL == "" {
		serverURL = "http://localhost:8080"
	}

	client := redgiant.NewClient(serverURL)
	client.SetPeerID(fmt.Sprintf("chat_%s_%d", username, time.Now().Unix()))

	// Create chat room
	room := &ChatRoom{
		client:   client,
		username: username,
		roomID:   roomID,
		messages: make([]ChatMessage, 0),
	}

	fmt.Printf("🚀 Red Giant P2P Chat\n")
	fmt.Printf("🌐 Server: %s\n", serverURL)
	fmt.Printf("👤 Username: %s\n", username)
	fmt.Printf("🏠 Room: %s\n", roomID)
	fmt.Printf("🆔 Peer ID: %s\n\n", client.PeerID)

	// Check server health
	if err := client.HealthCheck(); err != nil {
		log.Fatalf("❌ Server is not healthy: %v", err)
	}

	// Send join message
	room.sendSystemMessage(fmt.Sprintf("%s joined the chat", username))

	// Start message polling in background
	go room.pollMessages()

	// Show commands
	room.showCommands()

	// Start interactive chat
	room.startChat()
}

func showUsage() {
	fmt.Println("🚀 Red Giant P2P Chat")
	fmt.Println("Usage: go run main.go <username> [room-id]")
	fmt.Println("")
	fmt.Println("Examples:")
	fmt.Println("  go run main.go alice")
	fmt.Println("  go run main.go bob gaming")
	fmt.Println("")
	fmt.Println("Environment:")
	fmt.Println("  RED_GIANT_SERVER  Server URL (default: http://localhost:8080)")
}

func (room *ChatRoom) showCommands() {
	fmt.Println("💬 Chat Commands:")
	fmt.Println("  /help                 Show this help")
	fmt.Println("  /msg <user> <text>    Send private message")
	fmt.Println("  /history              Show message history")
	fmt.Println("  /users                List online users")
	fmt.Println("  /stats                Show network stats")
	fmt.Println("  /quit                 Leave chat")
	fmt.Println("  <message>             Send public message")
	fmt.Println("")
}

func (room *ChatRoom) startChat() {
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
			room.handleCommand(input)
		} else {
			room.sendPublicMessage(input)
		}
	}

	// Send leave message
	room.sendSystemMessage(fmt.Sprintf("%s left the chat", room.username))
}

func (room *ChatRoom) handleCommand(command string) {
	parts := strings.Fields(command)
	if len(parts) == 0 {
		return
	}

	switch parts[0] {
	case "/help":
		room.showCommands()

	case "/msg":
		if len(parts) < 3 {
			fmt.Println("Usage: /msg <username> <message>")
			return
		}
		recipient := parts[1]
		message := strings.Join(parts[2:], " ")
		room.sendPrivateMessage(recipient, message)

	case "/history":
		room.showHistory()

	case "/users":
		room.listUsers()

	case "/stats":
		room.showStats()

	case "/quit":
		fmt.Println("👋 Goodbye!")
		os.Exit(0)

	default:
		fmt.Printf("Unknown command: %s (type /help for commands)\n", parts[0])
	}
}

func (room *ChatRoom) sendPublicMessage(content string) {
	message := ChatMessage{
		ID:        fmt.Sprintf("msg_%d", time.Now().UnixNano()),
		From:      room.username,
		To:        room.roomID,
		Content:   content,
		Timestamp: time.Now(),
		Type:      "public",
	}

	room.sendMessage(message)
}

func (room *ChatRoom) sendPrivateMessage(recipient, content string) {
	message := ChatMessage{
		ID:        fmt.Sprintf("msg_%d", time.Now().UnixNano()),
		From:      room.username,
		To:        recipient,
		Content:   content,
		Timestamp: time.Now(),
		Type:      "private",
	}

	room.sendMessage(message)
	fmt.Printf("📨 Private message sent to %s\n", recipient)
}

func (room *ChatRoom) sendSystemMessage(content string) {
	message := ChatMessage{
		ID:        fmt.Sprintf("sys_%d", time.Now().UnixNano()),
		From:      "system",
		To:        room.roomID,
		Content:   content,
		Timestamp: time.Now(),
		Type:      "system",
	}

	room.sendMessage(message)
}

func (room *ChatRoom) sendMessage(message ChatMessage) {
	// Serialize message to JSON
	data, err := json.Marshal(message)
	if err != nil {
		fmt.Printf("❌ Failed to serialize message: %v\n", err)
		return
	}

	// Create filename with room and message type
	filename := fmt.Sprintf("chat_%s_%s_%d.json", room.roomID, message.Type, time.Now().UnixNano())

	// Upload message using Red Giant's high-performance C core
	start := time.Now()
	result, err := room.client.UploadData(data, filename)
	if err != nil {
		fmt.Printf("❌ Failed to send message: %v\n", err)
		return
	}

	sendTime := time.Since(start)

	// Add to local history
	room.messages = append(room.messages, message)

	// Show performance info for large messages
	if len(data) > 1024 {
		fmt.Printf("📊 Message sent: %d bytes in %v (%.2f MB/s)\n",
			len(data), sendTime, result.ThroughputMbps)
	}
}

func (room *ChatRoom) pollMessages() {
	ticker := time.NewTicker(1 * time.Second) // Poll every second
	defer ticker.Stop()

	lastCheck := time.Now()

	for range ticker.C {
		// Search for new messages in our room
		pattern := fmt.Sprintf("chat_%s_", room.roomID)
		files, err := room.client.SearchFiles(pattern)
		if err != nil {
			continue // Silently continue on error
		}

		// Process new messages
		for _, file := range files {
			if file.UploadedAt.After(lastCheck) && file.PeerID != room.client.PeerID {
				room.processIncomingMessage(file.ID)
			}
		}

		lastCheck = time.Now()
	}
}

func (room *ChatRoom) processIncomingMessage(fileID string) {
	// Download message data
	data, err := room.client.DownloadData(fileID)
	if err != nil {
		return // Silently ignore errors
	}

	// Parse message
	var message ChatMessage
	if err := json.Unmarshal(data, &message); err != nil {
		return // Silently ignore invalid messages
	}

	// Skip our own messages
	if message.From == room.username {
		return
	}

	// Add to history
	room.messages = append(room.messages, message)

	// Display message
	room.displayMessage(message)
}

func (room *ChatRoom) displayMessage(message ChatMessage) {
	timestamp := message.Timestamp.Format("15:04:05")

	switch message.Type {
	case "public":
		fmt.Printf("\r[%s] %s: %s\n> ", timestamp, message.From, message.Content)

	case "private":
		if message.To == room.username {
			fmt.Printf("\r[%s] 📨 %s (private): %s\n> ", timestamp, message.From, message.Content)
		}

	case "system":
		fmt.Printf("\r[%s] 🔔 %s\n> ", timestamp, message.Content)
	}
}

func (room *ChatRoom) showHistory() {
	fmt.Println("\n📜 Message History:")
	fmt.Println("==================")

	if len(room.messages) == 0 {
		fmt.Println("No messages in history.")
		return
	}

	// Show last 20 messages
	start := 0
	if len(room.messages) > 20 {
		start = len(room.messages) - 20
	}

	for i := start; i < len(room.messages); i++ {
		msg := room.messages[i]
		timestamp := msg.Timestamp.Format("15:04:05")

		switch msg.Type {
		case "public":
			fmt.Printf("[%s] %s: %s\n", timestamp, msg.From, msg.Content)
		case "private":
			if msg.To == room.username || msg.From == room.username {
				fmt.Printf("[%s] 📨 %s -> %s: %s\n", timestamp, msg.From, msg.To, msg.Content)
			}
		case "system":
			fmt.Printf("[%s] 🔔 %s\n", timestamp, msg.Content)
		}
	}

	fmt.Println("==================")
}

func (room *ChatRoom) listUsers() {
	fmt.Println("\n👥 Finding active users...")

	// Search for recent system messages to find active users
	files, err := room.client.SearchFiles(fmt.Sprintf("chat_%s_system", room.roomID))
	if err != nil {
		fmt.Printf("❌ Failed to search for users: %v\n", err)
		return
	}

	users := make(map[string]time.Time)
	cutoff := time.Now().Add(-5 * time.Minute) // Consider users active in last 5 minutes

	for _, file := range files {
		if file.UploadedAt.After(cutoff) {
			// Download and parse system message
			data, err := room.client.DownloadData(file.ID)
			if err != nil {
				continue
			}

			var message ChatMessage
			if err := json.Unmarshal(data, &message); err != nil {
				continue
			}

			// Extract username from join messages
			if strings.Contains(message.Content, "joined the chat") {
				username := strings.Replace(message.Content, " joined the chat", "", 1)
				users[username] = message.Timestamp
			}
		}
	}

	fmt.Printf("Active users in room '%s':\n", room.roomID)
	for username, lastSeen := range users {
		fmt.Printf("  • %s (last seen: %s)\n", username, lastSeen.Format("15:04:05"))
	}
	fmt.Println()
}

func (room *ChatRoom) showStats() {
	fmt.Println("\n📊 Network Statistics:")

	stats, err := room.client.GetNetworkStats()
	if err != nil {
		fmt.Printf("❌ Failed to get stats: %v\n", err)
		return
	}

	fmt.Printf("  • Messages processed: %d\n", stats.TotalRequests)
	fmt.Printf("  • Data transferred: %.2f MB\n", float64(stats.TotalBytes)/(1024*1024))
	fmt.Printf("  • Average latency: %d ms\n", stats.AverageLatency)
	fmt.Printf("  • Network throughput: %.2f MB/s\n", stats.ThroughputMbps)
	fmt.Printf("  • Server uptime: %d seconds\n", stats.UptimeSeconds)

	// Chat-specific stats
	fmt.Printf("  • Local message history: %d messages\n", len(room.messages))

	if len(room.messages) > 0 {
		oldest := room.messages[0].Timestamp
		newest := room.messages[len(room.messages)-1].Timestamp
		duration := newest.Sub(oldest)
		fmt.Printf("  • Chat session duration: %v\n", duration.Round(time.Second))
	}

	fmt.Println()
}
