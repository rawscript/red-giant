module github.com/red-giant-protocol/rgtp-go

go 1.21

// This module provides Go bindings for the Red Giant Transport Protocol (RGTP)
// 
// RGTP is a Layer 4 transport protocol that implements exposure-based data
// transmission, offering significant advantages over TCP including:
// - Natural multicast support
// - Instant resume capability  
// - Adaptive flow control
// - No head-of-line blocking
//
// For more information, visit: https://github.com/red-giant-protocol/rgtp