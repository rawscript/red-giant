//go:build ignore

package main

import (
	"C"
	"fmt"

	"github.com/redgiant/rgtp-go"
)

//export RgtpInitialize
func RgtpInitialize() *C.char {
	err := rgtp.Initialize()
	if err != nil {
		return C.CString(fmt.Sprintf("Error: %v", err))
	}
	return C.CString("OK")
}

//export RgtpCleanup
func RgtpCleanup() {
	rgtp.Cleanup()
}

//export RgtpVersion
func RgtpVersion() *C.char {
	return C.CString(rgtp.Version())
}

//export RgtpSendFile
func RgtpSendFile(filename *C.char, chunkSize C.uint, exposureRate C.uint) *C.char {
	config := rgtp.CreateConfig()
	config.ChunkSize = uint32(chunkSize)
	config.ExposureRate = uint32(exposureRate)

	stats, err := rgtp.SendFile(C.GoString(filename), config)
	if err != nil {
		return C.CString(fmt.Sprintf("Error: %v", err))
	}

	return C.CString(fmt.Sprintf("Success: Sent %d bytes at %.2f Mbps",
		stats.BytesSent, stats.AvgThroughputMbps))
}

//export RgtpReceiveFile
func RgtpReceiveFile(host *C.char, port C.ushort, filename *C.char, chunkSize C.uint) *C.char {
	config := rgtp.CreateConfig()
	config.ChunkSize = uint32(chunkSize)

	stats, err := rgtp.ReceiveFile(C.GoString(host), uint16(port), C.GoString(filename), config)
	if err != nil {
		return C.CString(fmt.Sprintf("Error: %v", err))
	}

	return C.CString(fmt.Sprintf("Success: Received %d bytes at %.2f Mbps",
		stats.BytesReceived, stats.AvgThroughputMbps))
}

func main() {}
