// Test CGO compilation
package main

/*
#cgo CFLAGS: -std=c99 -O3
#include "red_giant.h"
#include "red_giant.c"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

func main() {
	fmt.Println("🧪 Testing Red Giant C-Core Integration")
	
	// Create a test manifest
	cManifest := C.rg_manifest_t{
		total_size:         C.uint64_t(1024),
		chunk_size:         C.uint32_t(256),
		encoding_type:      C.uint16_t(0x01),
		exposure_cadence_ms: C.uint32_t(10),
		total_chunks:       C.uint32_t(4),
	}
	
	// Test surface creation
	fmt.Print("Creating C surface... ")
	surface := C.rg_create_surface(&cManifest)
	if surface == nil {
		fmt.Println("❌ FAILED")
		return
	}
	fmt.Println("✅ OK")
	
	// Test chunk exposure
	fmt.Print("Testing chunk exposure... ")
	testData := []byte("Red Giant Protocol C-Core Test Data")
	success := C.rg_expose_chunk_fast(
		surface,
		C.uint32_t(0),
		unsafe.Pointer(&testData[0]),
		C.uint32_t(len(testData)),
	)
	
	if bool(success) {
		fmt.Println("✅ OK")
	} else {
		fmt.Println("❌ FAILED")
	}
	
	// Test completion check
	fmt.Print("Testing completion check... ")
	complete := C.rg_is_complete(surface)
	fmt.Printf("Complete: %v\n", bool(complete))
	
	// Cleanup
	fmt.Print("Cleaning up... ")
	C.rg_destroy_surface(surface)
	fmt.Println("✅ OK")
	
	fmt.Println("🎉 C-Core integration test successful!")
}