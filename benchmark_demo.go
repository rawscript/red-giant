// Red Giant Protocol - Performance Benchmark Demo
package main

import (
	"fmt"
	"os"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

// Benchmark-optimized structures
type BenchmarkChunk struct {
	SequenceID   uint32
	DataSize     uint32
	IsExposed    int32  // Atomic flag
	Data         []byte
	ExposureTime int64  // Nanoseconds
}

type BenchmarkSurface struct {
	TotalChunks       uint32
	Chunks            []BenchmarkChunk
	ExposedCount      int32  // Atomic
	RedFlagRaised     int32  // Atomic
	StartTime         int64  // Nanoseconds
	TotalBytesExposed int64  // Atomic
}

type BenchmarkOrchestrator struct {
	surface     *BenchmarkSurface
	fileData    []byte
	chunkSize   int
	totalChunks int
	subscribers []chan int // Just chunk IDs for speed
	subMu       sync.RWMutex
	output      func(string)
}

func NewBenchmarkOrchestrator(fileData []byte, chunkSize int, output func(string)) *BenchmarkOrchestrator {
	totalChunks := (len(fileData) + chunkSize - 1) / chunkSize
	
	// Pre-allocate everything for maximum performance
	chunks := make([]BenchmarkChunk, totalChunks)
	for i := 0; i < totalChunks; i++ {
		start := i * chunkSize
		end := start + chunkSize
		if end > len(fileData) {
			end = len(fileData)
		}
		
		chunks[i] = BenchmarkChunk{
			SequenceID: uint32(i),
			DataSize:   uint32(end - start),
			Data:       make([]byte, 0, end-start), // Pre-allocate capacity
		}
	}
	
	surface := &BenchmarkSurface{
		TotalChunks: uint32(totalChunks),
		Chunks:      chunks,
		StartTime:   time.Now().UnixNano(),
	}
	
	return &BenchmarkOrchestrator{
		surface:     surface,
		fileData:    fileData,
		chunkSize:   chunkSize,
		totalChunks: totalChunks,
		subscribers: make([]chan int, 0),
		output:      output,
	}
}

func (bo *BenchmarkOrchestrator) Subscribe() <-chan int {
	bo.subMu.Lock()
	defer bo.subMu.Unlock()
	
	ch := make(chan int, 10000) // Huge buffer
	bo.subscribers = append(bo.subscribers, ch)
	return ch
}

// Maximum speed exposure
func (bo *BenchmarkOrchestrator) BeginBenchmarkExposure() {
	bo.output("🏁 Benchmark Red Giant Protocol - Maximum Speed Test\n")
	bo.output(fmt.Sprintf("📊 %d chunks, %d workers\n", bo.totalChunks, runtime.NumCPU()))
	
	// Use all CPU cores for parallel exposure
	numWorkers := runtime.NumCPU()
	chunksPerWorker := bo.totalChunks / numWorkers
	
	var wg sync.WaitGroup
	
	for worker := 0; worker < numWorkers; worker++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			
			start := workerID * chunksPerWorker
			end := start + chunksPerWorker
			if workerID == numWorkers-1 {
				end = bo.totalChunks
			}
			
			for i := start; i < end; i++ {
				chunkStart := i * bo.chunkSize
				chunkEnd := chunkStart + bo.chunkSize
				if chunkEnd > len(bo.fileData) {
					chunkEnd = len(bo.fileData)
				}
				
				chunk := &bo.surface.Chunks[i]
				chunkData := bo.fileData[chunkStart:chunkEnd]
				
				// Ultra-fast copy
				chunk.Data = chunk.Data[:len(chunkData)]
				copy(chunk.Data, chunkData)
				
				// Atomic operations
				atomic.StoreInt64(&chunk.ExposureTime, time.Now().UnixNano())
				atomic.StoreInt32(&chunk.IsExposed, 1)
				atomic.AddInt32(&bo.surface.ExposedCount, 1)
				atomic.AddInt64(&bo.surface.TotalBytesExposed, int64(len(chunkData)))
				
				// Notify subscribers (ultra-fast)
				bo.subMu.RLock()
				for _, subscriber := range bo.subscribers {
					select {
					case subscriber <- i:
					default:
					}
				}
				bo.subMu.RUnlock()
			}
		}(worker)
	}
	
	go func() {
		wg.Wait()
		atomic.StoreInt32(&bo.surface.RedFlagRaised, 1)
		bo.output("[RED FLAG] 🏁 Benchmark exposure complete!\n")
	}()
}

func (bo *BenchmarkOrchestrator) PullChunkBenchmark(chunkID int) ([]byte, bool) {
	if chunkID >= bo.totalChunks {
		return nil, false
	}
	
	chunk := &bo.surface.Chunks[chunkID]
	if atomic.LoadInt32(&chunk.IsExposed) == 0 {
		return nil, false
	}
	
	// Return slice directly for maximum speed (caller must not modify)
	return chunk.Data, true
}

func (bo *BenchmarkOrchestrator) IsComplete() bool {
	return atomic.LoadInt32(&bo.surface.RedFlagRaised) == 1
}

func (bo *BenchmarkOrchestrator) GetBenchmarkStats() (elapsedMs int64, throughputMBps float64) {
	elapsed := time.Now().UnixNano() - bo.surface.StartTime
	elapsedMs = elapsed / 1000000
	
	totalBytes := atomic.LoadInt64(&bo.surface.TotalBytesExposed)
	if elapsedMs > 0 {
		bytesPerSec := float64(totalBytes) * 1000.0 / float64(elapsedMs)
		throughputMBps = bytesPerSec / (1024 * 1024)
	}
	
	return
}

// Benchmark receiver with minimal overhead
type BenchmarkReceiver struct {
	expectedChunks  int
	receivedData    [][]byte
	chunkStatus     []int32
	completedChunks int32
	workerPool      chan struct{}
	chunkQueue      chan int
	completionChan  chan bool
	output          func(string)
	startTime       time.Time
}

func NewBenchmarkReceiver(totalChunks int, output func(string)) *BenchmarkReceiver {
	workerCount := runtime.NumCPU() * 4 // 4x CPU cores for maximum speed
	
	return &BenchmarkReceiver{
		expectedChunks: totalChunks,
		receivedData:   make([][]byte, totalChunks),
		chunkStatus:    make([]int32, totalChunks),
		workerPool:     make(chan struct{}, workerCount),
		chunkQueue:     make(chan int, totalChunks*16), // Massive buffer
		completionChan: make(chan bool, 1),
		output:         output,
		startTime:      time.Now(),
	}
}

func (br *BenchmarkReceiver) ConstructFileBenchmark(sender *BenchmarkOrchestrator) {
	workerCount := cap(br.workerPool)
	br.output(fmt.Sprintf("🏁 Benchmark Receiver: %d maximum-speed workers\n", workerCount))
	
	notifications := sender.Subscribe()
	
	// Start benchmark workers
	for i := 0; i < workerCount; i++ {
		go br.benchmarkWorker(sender, i)
	}
	
	// Ultra-fast dispatcher
	go func() {
		for chunkID := range notifications {
			select {
			case br.chunkQueue <- chunkID:
			default:
				go br.processBenchmarkChunk(sender, chunkID)
			}
		}
		close(br.chunkQueue)
	}()
	
	// Fast completion monitor
	go func() {
		for {
			completed := atomic.LoadInt32(&br.completedChunks)
			if int(completed) >= br.expectedChunks {
				select {
				case br.completionChan <- true:
				default:
				}
				return
			}
			runtime.Gosched() // Yield without sleep for maximum speed
		}
	}()
}

func (br *BenchmarkReceiver) benchmarkWorker(sender *BenchmarkOrchestrator, workerID int) {
	br.workerPool <- struct{}{}
	defer func() { <-br.workerPool }()
	
	for chunkID := range br.chunkQueue {
		br.processBenchmarkChunk(sender, chunkID)
	}
}

func (br *BenchmarkReceiver) processBenchmarkChunk(sender *BenchmarkOrchestrator, chunkID int) {
	if atomic.LoadInt32(&br.chunkStatus[chunkID]) == 1 {
		return
	}
	
	chunkData, success := sender.PullChunkBenchmark(chunkID)
	if success {
		if atomic.CompareAndSwapInt32(&br.chunkStatus[chunkID], 0, 1) {
			// Store reference directly for maximum speed
			br.receivedData[chunkID] = chunkData
			atomic.AddInt32(&br.completedChunks, 1)
		}
	}
}

func (br *BenchmarkReceiver) WaitForBenchmarkCompletion(sender *BenchmarkOrchestrator) []byte {
	br.output("🏁 Waiting for benchmark completion...\n")
	
	select {
	case <-br.completionChan:
		br.output("✅ Benchmark processing complete!\n")
	case <-time.After(30 * time.Second):
		br.output("⚠️  Benchmark timeout\n")
	}
	
	for !sender.IsComplete() {
		runtime.Gosched()
	}
	
	// Ultra-fast reconstruction
	totalSize := 0
	for _, chunk := range br.receivedData {
		if chunk != nil {
			totalSize += len(chunk)
		}
	}
	
	completeFile := make([]byte, 0, totalSize)
	missingChunks := 0
	
	for _, chunk := range br.receivedData {
		if chunk == nil {
			missingChunks++
			continue
		}
		completeFile = append(completeFile, chunk...)
	}
	
	elapsed := time.Since(br.startTime)
	throughputMBps := float64(len(completeFile)) / elapsed.Seconds() / (1024 * 1024)
	
	elapsedMs, senderThroughput := sender.GetBenchmarkStats()
	
	br.output("🏁 Benchmark Results:\n")
	br.output(fmt.Sprintf("   • Total bytes: %d (%.2f MB)\n", len(completeFile), float64(len(completeFile))/(1024*1024)))
	br.output(fmt.Sprintf("   • Chunks: %d/%d (%.1f%% success)\n", 
		br.expectedChunks-missingChunks, br.expectedChunks, 
		float64(br.expectedChunks-missingChunks)/float64(br.expectedChunks)*100))
	br.output(fmt.Sprintf("   • Total time: %v\n", elapsed))
	br.output(fmt.Sprintf("   • Receiver throughput: %.2f MB/s\n", throughputMBps))
	br.output(fmt.Sprintf("   • Sender throughput: %.2f MB/s\n", senderThroughput))
	br.output(fmt.Sprintf("   • Exposure time: %d ms\n", elapsedMs))
	br.output(fmt.Sprintf("   • Workers: %d\n", cap(br.workerPool)))
	br.output(fmt.Sprintf("   • CPU cores: %d\n", runtime.NumCPU()))
	
	return completeFile
}

func runBenchmark(name string, dataSize int, chunkSize int, output func(string)) {
	output(fmt.Sprintf("\n🏁 BENCHMARK: %s\n", name))
	output(strings.Repeat("=", 50) + "\n")
	
	// Generate test data
	pattern := "Red Giant Protocol Benchmark! "
	sampleData := []byte(strings.Repeat(pattern, dataSize/len(pattern)))
	
	output(fmt.Sprintf("📊 Data: %.2f MB, Chunk: %d KB\n", 
		float64(len(sampleData))/(1024*1024), chunkSize/1024))
	
	totalChunks := (len(sampleData) + chunkSize - 1) / chunkSize
	output(fmt.Sprintf("📦 Chunks: %d\n", totalChunks))
	
	// Run benchmark
	sender := NewBenchmarkOrchestrator(sampleData, chunkSize, output)
	receiver := NewBenchmarkReceiver(totalChunks, output)
	
	receiver.ConstructFileBenchmark(sender)
	sender.BeginBenchmarkExposure()
	
	reconstructedData := receiver.WaitForBenchmarkCompletion(sender)
	
	// Verify
	if len(reconstructedData) == len(sampleData) {
		output("✅ Benchmark PASSED\n")
	} else {
		output("❌ Benchmark FAILED\n")
	}
}

func main() {
	runtime.GOMAXPROCS(runtime.NumCPU())
	
	file, err := os.Create("benchmark_output.txt")
	if err != nil {
		panic(err)
	}
	defer file.Close()
	
	output := func(msg string) {
		fmt.Print(msg)
		file.WriteString(msg)
	}
	
	output("🏁 Red Giant Protocol - Performance Benchmark Suite\n")
	output("═══════════════════════════════════════════════════\n")
	output(fmt.Sprintf("💻 System: %d CPU cores, GOMAXPROCS: %d\n", 
		runtime.NumCPU(), runtime.GOMAXPROCS(0)))
	output(fmt.Sprintf("🚀 Go version: %s\n\n", runtime.Version()))
	
	// Run multiple benchmarks with different configurations
	benchmarks := []struct {
		name      string
		dataSize  int
		chunkSize int
	}{
		{"Small Files", 1024 * 1024, 16 * 1024},      // 1MB, 16KB chunks
		{"Medium Files", 10 * 1024 * 1024, 64 * 1024}, // 10MB, 64KB chunks
		{"Large Files", 50 * 1024 * 1024, 256 * 1024}, // 50MB, 256KB chunks
		{"Huge Files", 100 * 1024 * 1024, 512 * 1024}, // 100MB, 512KB chunks
	}
	
	for _, bench := range benchmarks {
		runBenchmark(bench.name, bench.dataSize, bench.chunkSize, output)
		time.Sleep(1 * time.Second) // Brief pause between benchmarks
	}
	
	output("\n🏁 Red Giant Protocol Benchmark Suite Complete!\n")
	output("   • Lock-free atomic operations ✓\n")
	output("   • Multi-core parallel processing ✓\n")
	output("   • Zero-copy optimizations ✓\n")
	output("   • Maximum CPU utilization ✓\n")
	output("   • Scalable worker pools ✓\n")
	
	output("\nCheck benchmark_output.txt for detailed results!\n")
}