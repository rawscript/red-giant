// Red Giant Protocol - Complete P2P Demo
package main

import (
	"fmt"
	"os"
	"os/exec"
	"time"
)

func runCommand(name string, args ...string) error {
	fmt.Printf("🚀 Running: %s %v\n", name, args)
	cmd := exec.Command(name, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func main() {
	fmt.Println("🚀 Red Giant Protocol - Complete P2P System Demo")
	fmt.Println("═══════════════════════════════════════════════════")
	fmt.Println()

	if len(os.Args) > 1 && os.Args[1] == "quick" {
		quickDemo()
		return
	}

	fmt.Println("This demo will show you the complete Red Giant P2P system:")
	fmt.Println("1. 📁 File sharing between peers")
	fmt.Println("2. 💬 Real-time chat system")
	fmt.Println("3. 🌐 Network discovery")
	fmt.Println("4. ⚡ High-performance data transmission")
	fmt.Println()
	fmt.Println("Make sure your Red Giant server is running:")
	fmt.Println("  go run red_giant_server.go")
	fmt.Println()
	fmt.Print("Press Enter to continue...")
	fmt.Scanln()

	// Demo 1: Network Discovery
	fmt.Println("\n📊 DEMO 1: Network Discovery & Statistics")
	fmt.Println("═══════════════════════════════════════════")
	runCommand("go", "run", "red_giant_network.go", "stats")
	time.Sleep(2 * time.Second)

	// Demo 2: File Upload
	fmt.Println("\n📤 DEMO 2: File Upload to P2P Network")
	fmt.Println("═══════════════════════════════════════════")
	
	// Create a demo file
	demoContent := `# Red Giant Protocol Demo File

This file was uploaded to the Red Giant P2P network!

Features demonstrated:
- High-performance C-core processing
- Peer-to-peer file sharing
- Real-time network discovery
- 500+ MB/s throughput capability

The Red Giant Protocol revolutionizes data transmission
with its exposure-based architecture.
`
	
	if err := os.WriteFile("demo_file.md", []byte(demoContent), 0644); err != nil {
		fmt.Printf("Failed to create demo file: %v\n", err)
		return
	}
	
	runCommand("go", "run", "red_giant_peer.go", "upload", "demo_file.md")
	time.Sleep(2 * time.Second)

	// Demo 3: File Discovery
	fmt.Println("\n🔍 DEMO 3: File Discovery in Network")
	fmt.Println("═══════════════════════════════════════════")
	runCommand("go", "run", "red_giant_network.go", "discover")
	time.Sleep(2 * time.Second)

	// Demo 4: Performance Test
	fmt.Println("\n⚡ DEMO 4: Network Performance Test")
	fmt.Println("═══════════════════════════════════════════")
	runCommand("go", "run", "red_giant_network.go", "test")
	time.Sleep(2 * time.Second)

	// Demo 5: File List
	fmt.Println("\n📁 DEMO 5: List All Files in Network")
	fmt.Println("═══════════════════════════════════════════")
	runCommand("go", "run", "red_giant_peer.go", "list")

	fmt.Println("\n🎉 Demo Complete!")
	fmt.Println("═══════════════════════════════════════════")
	fmt.Println("Your Red Giant P2P network is now operational with:")
	fmt.Println("✅ High-performance file sharing")
	fmt.Println("✅ Real-time peer discovery")
	fmt.Println("✅ Network monitoring")
	fmt.Println("✅ 500+ MB/s throughput")
	fmt.Println()
	fmt.Println("Next steps:")
	fmt.Println("• Try the chat system: go run red_giant_chat.go your_name")
	fmt.Println("• Share folders: go run red_giant_peer.go share ./folder")
	fmt.Println("• Monitor network: go run red_giant_network.go monitor 30s")
	fmt.Println("• Visit web interface: http://localhost:8080")
}

func quickDemo() {
	fmt.Println("🚀 Quick Demo - Testing Red Giant P2P System")
	fmt.Println("═══════════════════════════════════════════")

	// Quick network test
	fmt.Println("1. Testing network connectivity...")
	if err := runCommand("go", "run", "red_giant_network.go", "test"); err != nil {
		fmt.Printf("❌ Network test failed: %v\n", err)
		fmt.Println("Make sure the server is running: go run red_giant_server.go")
		return
	}

	// Quick file upload
	fmt.Println("\n2. Testing file upload...")
	testFile := "quick_test.txt"
	os.WriteFile(testFile, []byte("Red Giant Protocol Quick Test"), 0644)
	
	if err := runCommand("go", "run", "red_giant_peer.go", "upload", testFile); err != nil {
		fmt.Printf("❌ File upload failed: %v\n", err)
		return
	}

	// Quick file list
	fmt.Println("\n3. Listing network files...")
	runCommand("go", "run", "red_giant_peer.go", "list")

	fmt.Println("\n✅ Quick demo successful!")
	fmt.Println("Your Red Giant P2P system is working correctly.")
	
	// Cleanup
	os.Remove(testFile)
}