
package main

type SecurityLevel int

const (
    SecurityNone SecurityLevel = iota    // 0% overhead - raw performance
    SecurityCRC                         // <0.1% overhead - integrity only  
    SecurityLite                        // ~1% overhead - auth + integrity
    SecurityFull                        // ~5% overhead - full encryption
    SecurityBatch                       // ~2% overhead - batch crypto
)

type SecurityConfig struct {
    Level           SecurityLevel
    BatchSize       int
    EncryptionKey   []byte
    AuthToken       []byte
    ChecksumOnly    bool
    ZeroCopy        bool
}

func OptimalSecurityConfig(throughputTarget float64) *SecurityConfig {
    if throughputTarget > 1000 { // 1+ GB/s targets
        return &SecurityConfig{
            Level:        SecurityCRC,
            BatchSize:    64,
            ChecksumOnly: true,
            ZeroCopy:     true,
        }
    } else if throughputTarget > 500 { // 500+ MB/s targets  
        return &SecurityConfig{
            Level:     SecurityLite,
            BatchSize: 32,
            ZeroCopy:  true,
        }
    } else { // <500 MB/s - can afford full security
        return &SecurityConfig{
            Level:     SecurityFull,
            BatchSize: 16,
        }
    }
}
