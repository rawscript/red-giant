// Red Giant Protocol - AI Token Streaming Example
// Demonstrates high-speed AI response streaming using Red Giant's C core
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"log"
	"math/rand"
	"os"
	"strconv"
	"strings"
	"time"

	"../go" // Import Red Giant SDK
)

type AIToken struct {
	ID        string    `json:"id"`
	SessionID string    `json:"session_id"`
	Token     string    `json:"token"`
	Position  int       `json:"position"`
	Timestamp time.Time `json:"timestamp"`
	Metadata  TokenMeta `json:"metadata"`
}

type TokenMeta struct {
	Confidence  float64 `json:"confidence"`
	TokenType   string  `json:"token_type"`
	ModelName   string  `json:"model_name"`
	Temperature float64 `json:"temperature"`
}

type AIResponse struct {
	SessionID    string    `json:"session_id"`
	Prompt       string    `json:"prompt"`
	Response     string    `json:"response"`
	Tokens       []AIToken `json:"tokens"`
	TotalTokens  int       `json:"total_tokens"`
	StartTime    time.Time `json:"start_time"`
	EndTime      time.Time `json:"end_time"`
	Duration     string    `json:"duration"`
	ModelName    string    `json:"model_name"`
}

type StreamingSession struct {
	client    *redgiant.Client
	sessionID string
	modelName string
	tokens    []AIToken
}

func main() {
	if len(os.Args) < 2 {
		showUsage()
		return
	}

	command := os.Args[1]

	// Connect to Red Giant server
	serverURL := os.Getenv("RED_GIANT_SERVER")
	if serverURL == "" {
		serverURL = "http://localhost:8080"
	}

	client := redgiant.NewClient(serverURL)

	fmt.Printf("🤖 Red Giant AI Token Streaming\n")
	fmt.Printf("🔗 Server: %s\n", serverURL)

	switch command {
	case "stream":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go stream <model-name> [session-id]")
			return
		}
		runStreaming(client, os.Args[2:])

	case "simulate":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run main.go simulate <model-name> <num-sessions> [tokens-per-sec]")
			return
		}
		modelName := os.Args[2]
		numSessions, _ := strconv.Atoi(os.Args[3])
		tokensPerSec := 50
		if len(os.Args) > 4 {
			tokensPerSec, _ = strconv.Atoi(os.Args[4])
		}
		simulateAI(client, modelName, numSessions, tokensPerSec)

	case "collect":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go collect <session-id>")
			return
		}
		collectTokens(client, os.Args[2])

	case "monitor":
		monitorStreams(client)

	case "benchmark":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go benchmark <tokens-per-second>")
			return
		}
		tokensPerSec, _ := strconv.Atoi(os.Args[2])
		benchmarkStreaming(client, tokensPerSec)

	default:
		fmt.Printf("Unknown command: %s\n", command)
		showUsage()
	}
}

func showUsage() {
	fmt.Println("🤖 Red Giant AI Token Streaming")
	fmt.Println("Usage: go run main.go <command> [args...]")
	fmt.Println("")
	fmt.Println("Commands:")
	fmt.Println("  stream <model> [session]        Interactive AI streaming")
	fmt.Println("  simulate <model> <sessions> [tps] Simulate AI responses")
	fmt.Println("  collect <session-id>            Collect streamed tokens")
	fmt.Println("  monitor                         Monitor all streams")
	fmt.Println("  benchmark <tokens-per-sec>      Benchmark streaming performance")
	fmt.Println("")
	fmt.Println("Examples:")
	fmt.Println("  go run main.go stream gpt-4")
	fmt.Println("  go run main.go simulate claude-3 5 100")
	fmt.Println("  go run main.go collect session_123")
	fmt.Println("  go run main.go benchmark 200")
	fmt.Println("")
	fmt.Println("Environment:")
	fmt.Println("  RED_GIANT_SERVER  Server URL (default: http://localhost:8080)")
}

func runStreaming(client *redgiant.Client, args []string) {
	modelName := args[0]
	sessionID := fmt.Sprintf("session_%d", time.Now().Unix())
	if len(args) > 1 {
		sessionID = args[1]
	}

	client.SetPeerID(fmt.Sprintf("ai_streamer_%s", sessionID))

	session := &StreamingSession{
		client:    client,
		sessionID: sessionID,
		modelName: modelName,
		tokens:    make([]AIToken, 0),
	}

	fmt.Printf("🤖 AI Token Streaming Session\n")
	fmt.Printf("🔗 Model: %s\n", modelName)
	fmt.Printf("🆔 Session: %s\n", sessionID)
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	scanner := bufio.NewScanner(os.Stdin)

	for {
		fmt.Print("💬 Enter prompt (or 'quit' to exit): ")
		if !scanner.Scan() {
			break
		}

		prompt := strings.TrimSpace(scanner.Text())
		if prompt == "quit" {
			break
		}

		if prompt == "" {
			continue
		}

		session.processPrompt(prompt)
	}

	// Save final session
	session.saveSession()
	fmt.Printf("👋 Session saved. Total tokens streamed: %d\n", len(session.tokens))
}

func (session *StreamingSession) processPrompt(prompt string) {
	fmt.Printf("\n🤖 %s is thinking...\n", session.modelName)
	fmt.Print("📝 Response: ")

	startTime := time.Now()
	response := session.simulateAIResponse(prompt)
	
	// Stream tokens with Red Giant's high performance
	tokens := session.tokenizeResponse(response)
	
	for i, token := range tokens {
		// Create AI token
		aiToken := AIToken{
			ID:        fmt.Sprintf("token_%s_%d", session.sessionID, len(session.tokens)+i),
			SessionID: session.sessionID,
			Token:     token,
			Position:  len(session.tokens) + i,
			Timestamp: time.Now(),
			Metadata: TokenMeta{
				Confidence:  0.8 + rand.Float64()*0.2,
				TokenType:   session.getTokenType(token),
				ModelName:   session.modelName,
				Temperature: 0.7,
			},
		}

		// Stream token using Red Giant
		session.streamToken(aiToken)
		
		// Display token
		fmt.Print(token)
		if !strings.HasSuffix(token, " ") && i < len(tokens)-1 {
			fmt.Print(" ")
		}

		// Add to session
		session.tokens = append(session.tokens, aiToken)

		// Simulate realistic streaming delay
		time.Sleep(time.Duration(20+rand.Intn(80)) * time.Millisecond)
	}

	endTime := time.Now()
	duration := endTime.Sub(startTime)

	fmt.Printf("\n\n📊 Streaming stats: %d tokens in %v (%.1f tokens/sec)\n\n",
		len(tokens), duration, float64(len(tokens))/duration.Seconds())
}

func (session *StreamingSession) simulateAIResponse(prompt string) string {
	// Simulate different types of AI responses
	responses := []string{
		"I understand your question about " + prompt + ". Let me provide a comprehensive answer that covers the key aspects you're interested in.",
		"That's an interesting topic! Here's what I can tell you: " + prompt + " involves several important considerations that we should explore together.",
		"Great question! To address " + prompt + ", I'll break this down into several key points that will help clarify the concept for you.",
		"I'd be happy to help with " + prompt + ". This is a complex topic that requires careful consideration of multiple factors and perspectives.",
	}

	baseResponse := responses[rand.Intn(len(responses))]
	
	// Add some additional content to make it more realistic
	additions := []string{
		" First, it's important to understand the fundamental principles involved.",
		" The key factors to consider include efficiency, scalability, and maintainability.",
		" From a technical perspective, there are several approaches we could take.",
		" Let me walk you through the most effective strategies for this situation.",
		" The best practices in this area have evolved significantly over recent years.",
	}

	for i := 0; i < 2+rand.Intn(3); i++ {
		baseResponse += additions[rand.Intn(len(additions))]
	}

	return baseResponse
}

func (session *StreamingSession) tokenizeResponse(response string) []string {
	// Simple tokenization - in real implementation, use proper tokenizer
	words := strings.Fields(response)
	tokens := make([]string, 0)

	for _, word := range words {
		// Sometimes split words into subword tokens
		if len(word) > 8 && rand.Float64() < 0.3 {
			mid := len(word) / 2
			tokens = append(tokens, word[:mid], word[mid:])
		} else {
			tokens = append(tokens, word)
		}
	}

	return tokens
}

func (session *StreamingSession) getTokenType(token string) string {
	if strings.HasSuffix(token, ".") || strings.HasSuffix(token, "!") || strings.HasSuffix(token, "?") {
		return "sentence_end"
	}
	if strings.HasSuffix(token, ",") {
		return "comma"
	}
	if len(token) > 6 {
		return "word"
	}
	return "subword"
}

func (session *StreamingSession) streamToken(token AIToken) {
	// Serialize token to JSON
	data, err := json.Marshal(token)
	if err != nil {
		log.Printf("❌ Failed to serialize token: %v", err)
		return
	}

	// Upload token using Red Giant's high-performance C core
	filename := fmt.Sprintf("ai_token_%s_%d.json", token.SessionID, token.Position)
	
	_, err = session.client.UploadData(data, filename)
	if err != nil {
		log.Printf("❌ Failed to stream token: %v", err)
	}
}

func (session *StreamingSession) saveSession() {
	// Create complete session record
	aiResponse := AIResponse{
		SessionID:   session.sessionID,
		Tokens:      session.tokens,
		TotalTokens: len(session.tokens),
		StartTime:   time.Now().Add(-time.Hour), // Approximate
		EndTime:     time.Now(),
		ModelName:   session.modelName,
	}
	aiResponse.Duration = aiResponse.EndTime.Sub(aiResponse.StartTime).String()

	// Serialize and upload session
	data, err := json.Marshal(aiResponse)
	if err != nil {
		log.Printf("❌ Failed to serialize session: %v", err)
		return
	}

	filename := fmt.Sprintf("ai_session_%s.json", session.sessionID)
	result, err := session.client.UploadData(data, filename)
	if err != nil {
		log.Printf("❌ Failed to save session: %v", err)
		return
	}

	fmt.Printf("💾 Session saved: %d bytes (%.2f MB/s)\n", len(data), result.ThroughputMbps)
}

func simulateAI(client *redgiant.Client, modelName string, numSessions, tokensPerSec int) {
	fmt.Printf("🤖 Simulating AI Model: %s\n", modelName)
	fmt.Printf("📊 Sessions: %d\n", numSessions)
	fmt.Printf("⚡ Target rate: %d tokens/second\n\n", tokensPerSec)

	// Start multiple streaming sessions
	for i := 0; i < numSessions; i++ {
		sessionID := fmt.Sprintf("sim_session_%s_%03d", modelName, i+1)
		
		go func(id string) {
			sessionClient := redgiant.NewClient(client.BaseURL)
			sessionClient.SetPeerID(fmt.Sprintf("ai_sim_%s", id))
			
			session := &StreamingSession{
				client:    sessionClient,
				sessionID: id,
				modelName: modelName,
				tokens:    make([]AIToken, 0),
			}
			
			fmt.Printf("🚀 Started session: %s\n", id)
			
			// Simulate continuous AI responses
			prompts := []string{
				"Explain quantum computing",
				"Write a Python function",
				"Describe machine learning",
				"Create a REST API",
				"Optimize database queries",
			}
			
			for {
				prompt := prompts[rand.Intn(len(prompts))]
				response := session.simulateAIResponse(prompt)
				tokens := session.tokenizeResponse(response)
				
				// Stream tokens at specified rate
				interval := time.Duration(1000/tokensPerSec) * time.Millisecond
				
				for i, token := range tokens {
					aiToken := AIToken{
						ID:        fmt.Sprintf("token_%s_%d", id, len(session.tokens)+i),
						SessionID: id,
						Token:     token,
						Position:  len(session.tokens) + i,
						Timestamp: time.Now(),
						Metadata: TokenMeta{
							Confidence:  0.8 + rand.Float64()*0.2,
							TokenType:   session.getTokenType(token),
							ModelName:   modelName,
							Temperature: 0.7,
						},
					}
					
					session.streamToken(aiToken)
					session.tokens = append(session.tokens, aiToken)
					
					time.Sleep(interval)
				}
				
				// Pause between responses
				time.Sleep(time.Duration(2+rand.Intn(5)) * time.Second)
			}
		}(sessionID)
		
		// Stagger session starts
		time.Sleep(200 * time.Millisecond)
	}

	// Keep main thread alive
	select {}
}

func collectTokens(client *redgiant.Client, sessionID string) {
	client.SetPeerID(fmt.Sprintf("ai_collector_%s", sessionID))

	fmt.Printf("📥 Collecting tokens for session: %s\n", sessionID)
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	collectedTokens := make(map[int]AIToken)
	lastCheck := time.Now().Add(-1 * time.Minute)

	for range ticker.C {
		// Search for tokens from this session
		pattern := fmt.Sprintf("ai_token_%s_", sessionID)
		files, err := client.SearchFiles(pattern)
		if err != nil {
			log.Printf("❌ Failed to search for tokens: %v", err)
			continue
		}

		newTokens := 0

		for _, file := range files {
			if file.UploadedAt.After(lastCheck) {
				// Download and parse token
				data, err := client.DownloadData(file.ID)
				if err != nil {
					continue
				}

				var token AIToken
				if err := json.Unmarshal(data, &token); err != nil {
					continue
				}

				collectedTokens[token.Position] = token
				newTokens++
			}
		}

		if newTokens > 0 {
			fmt.Printf("📥 Collected %d new tokens\n", newTokens)
			
			// Reconstruct response from collected tokens
			response := reconstructResponse(collectedTokens)
			if response != "" {
				fmt.Printf("📝 Current response: %s\n", response)
			}
			
			fmt.Printf("📊 Total tokens collected: %d\n\n", len(collectedTokens))
		}

		lastCheck = time.Now()
	}
}

func reconstructResponse(tokens map[int]AIToken) string {
	if len(tokens) == 0 {
		return ""
	}

	// Sort tokens by position
	maxPos := 0
	for pos := range tokens {
		if pos > maxPos {
			maxPos = pos
		}
	}

	var response strings.Builder
	for i := 0; i <= maxPos; i++ {
		if token, exists := tokens[i]; exists {
			response.WriteString(token.Token)
			if !strings.HasSuffix(token.Token, " ") && i < maxPos {
				response.WriteString(" ")
			}
		} else {
			response.WriteString("[missing] ")
		}
	}

	return response.String()
}

func monitorStreams(client *redgiant.Client) {
	client.SetPeerID(fmt.Sprintf("ai_monitor_%d", time.Now().Unix()))

	fmt.Printf("📊 Monitoring AI Token Streams\n")
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	sessionStats := make(map[string]int)
	lastCheck := time.Now().Add(-1 * time.Minute)

	for range ticker.C {
		// Search for all AI tokens
		files, err := client.SearchFiles("ai_token_")
		if err != nil {
			continue
		}

		newTokens := 0
		for _, file := range files {
			if file.UploadedAt.After(lastCheck) {
				// Extract session ID from filename
				parts := strings.Split(file.Name, "_")
				if len(parts) >= 4 {
					sessionID := strings.Join(parts[2:len(parts)-1], "_")
					sessionStats[sessionID]++
					newTokens++
				}
			}
		}

		if newTokens > 0 {
			fmt.Printf("📊 AI Streaming Monitor Report\n")
			fmt.Printf("==============================\n")
			fmt.Printf("Active sessions: %d\n", len(sessionStats))
			fmt.Printf("New tokens: %d\n\n", newTokens)

			totalTokens := 0
			for sessionID, count := range sessionStats {
				fmt.Printf("🤖 %s: %d tokens\n", sessionID, count)
				totalTokens += count
			}

			fmt.Printf("\n📈 Total tokens streamed: %d\n", totalTokens)
			
			// Network performance
			stats, err := client.GetNetworkStats()
			if err == nil {
				fmt.Printf("⚡ Network throughput: %.2f MB/s\n", stats.ThroughputMbps)
				fmt.Printf("⏱️  Average latency: %d ms\n", stats.AverageLatency)
			}
			
			fmt.Printf("==============================\n\n")
		}

		lastCheck = time.Now()
	}
}

func benchmarkStreaming(client *redgiant.Client, tokensPerSec int) {
	client.SetPeerID(fmt.Sprintf("ai_benchmark_%d", time.Now().Unix()))

	fmt.Printf("🏁 AI Token Streaming Benchmark\n")
	fmt.Printf("⚡ Target rate: %d tokens/second\n", tokensPerSec)
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	sessionID := fmt.Sprintf("benchmark_%d", time.Now().Unix())
	interval := time.Duration(1000/tokensPerSec) * time.Millisecond

	fmt.Printf("🚀 Starting benchmark...\n")
	start := time.Now()

	tokenCount := 0
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	// Run for 30 seconds
	timeout := time.After(30 * time.Second)

	for {
		select {
		case <-ticker.C:
			token := AIToken{
				ID:        fmt.Sprintf("bench_token_%d", tokenCount),
				SessionID: sessionID,
				Token:     fmt.Sprintf("token_%d", tokenCount),
				Position:  tokenCount,
				Timestamp: time.Now(),
				Metadata: TokenMeta{
					Confidence:  0.95,
					TokenType:   "benchmark",
					ModelName:   "benchmark-model",
					Temperature: 0.0,
				},
			}

			// Stream token
			data, _ := json.Marshal(token)
			filename := fmt.Sprintf("bench_token_%s_%d.json", sessionID, tokenCount)
			
			_, err := client.UploadData(data, filename)
			if err != nil {
				log.Printf("❌ Failed to stream token %d: %v", tokenCount, err)
			}

			tokenCount++

			// Show progress every 100 tokens
			if tokenCount%100 == 0 {
				elapsed := time.Since(start)
				actualRate := float64(tokenCount) / elapsed.Seconds()
				fmt.Printf("📊 Progress: %d tokens, %.1f tokens/sec (target: %d)\n", 
					tokenCount, actualRate, tokensPerSec)
			}

		case <-timeout:
			elapsed := time.Since(start)
			actualRate := float64(tokenCount) / elapsed.Seconds()

			fmt.Printf("\n🏁 Benchmark Complete!\n")
			fmt.Printf("==============================\n")
			fmt.Printf("Duration: %v\n", elapsed)
			fmt.Printf("Tokens streamed: %d\n", tokenCount)
			fmt.Printf("Target rate: %d tokens/sec\n", tokensPerSec)
			fmt.Printf("Actual rate: %.1f tokens/sec\n", actualRate)
			fmt.Printf("Efficiency: %.1f%%\n", (actualRate/float64(tokensPerSec))*100)

			// Network stats
			stats, err := client.GetNetworkStats()
			if err == nil {
				fmt.Printf("Network throughput: %.2f MB/s\n", stats.ThroughputMbps)
				fmt.Printf("Average latency: %d ms\n", stats.AverageLatency)
			}

			fmt.Printf("==============================\n")
			return
		}
	}
}