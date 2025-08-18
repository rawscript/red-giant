//go:build testsystem
// +build testsystem

// Red Giant Protocol - System Test
package main

import (
	"fmt"
	"os"
	"os/exec"
)

func main() {
	fmt.Println("🧪 Red Giant Protocol - System Test")
	fmt.Println("=====================================")

	// Test 1: Check if Go files compile
	fmt.Println("\n1. Testing Go file compilation...")

	testFiles := []string{
		"red_giant_universal.go",
		"red_giant_test_server.go",
		"red_giant_adaptive.go",
	}

	for _, file := range testFiles {
		fmt.Printf("   Testing %s... ", file)
		cmd := exec.Command("go", "build", "-o", "temp_"+file+".exe", file)
		if err := cmd.Run(); err != nil {
			fmt.Printf("❌ FAILED: %v\n", err)
		} else {
			fmt.Printf("✅ OK\n")
			// Clean up
			os.Remove("temp_" + file + ".exe")
		}
	}

	// Test 2: Check C compilation
	fmt.Println("\n2. Testing C core compilation...")
	fmt.Printf("   Testing red_giant.c... ")
	cmd := exec.Command("gcc", "-c", "red_giant.c", "-o", "red_giant.o")
	if err := cmd.Run(); err != nil {
		fmt.Printf("❌ FAILED: %v\n", err)
	} else {
		fmt.Printf("✅ OK\n")
		os.Remove("red_giant.o")
	}

	// Test 3: Quick functionality test
	fmt.Println("\n3. Testing basic functionality...")
	fmt.Printf("   Universal client help... ")
	cmd = exec.Command("go", "run", "red_giant_universal.go")
	if err := cmd.Run(); err == nil {
		fmt.Printf("✅ OK\n")
	} else {
		fmt.Printf("❌ FAILED: %v\n", err)
	}

	fmt.Println("\n🎉 System test completed!")
	fmt.Println("\nNext steps:")
	fmt.Println("1. Start the test server: go run red_giant_test_server.go")
	fmt.Println("2. Start the adaptive server: go run red_giant_adaptive.go")
	fmt.Println("3. Test with client: go run red_giant_universal.go smart <file>")
}
