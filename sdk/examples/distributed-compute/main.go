// Red Giant Protocol - Distributed Computing Example
// Demonstrates running simulations across multiple devices using Red Giant's high-performance core
package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math"
	"math/rand"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"redgiant-sdk" // Import Red Giant SDK
)

type ComputeTask struct {
	ID          string                 `json:"id"`
	Type        string                 `json:"type"`
	Parameters  map[string]interface{} `json:"parameters"`
	Priority    int                    `json:"priority"`
	CreatedAt   time.Time              `json:"created_at"`
	AssignedTo  string                 `json:"assigned_to,omitempty"`
	Status      string                 `json:"status"` // "pending", "running", "completed", "failed"
	Progress    float64                `json:"progress"`
	Result      interface{}            `json:"result,omitempty"`
	Error       string                 `json:"error,omitempty"`
	CompletedAt time.Time              `json:"completed_at,omitempty"`
}

type ComputeResult struct {
	TaskID      string      `json:"task_id"`
	WorkerID    string      `json:"worker_id"`
	Result      interface{} `json:"result"`
	Duration    string      `json:"duration"`
	Timestamp   time.Time   `json:"timestamp"`
	Performance Performance `json:"performance"`
}

type Performance struct {
	CPUUsage    float64 `json:"cpu_usage"`
	MemoryUsage float64 `json:"memory_usage"`
	Throughput  float64 `json:"throughput_mbps"`
}

type ComputeWorker struct {
	client   *redgiant.Client
	workerID string
	nodeType string
	capacity int
	running  bool
	tasks    map[string]*ComputeTask
	mu       sync.RWMutex
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

	fmt.Printf("🖥️  Red Giant Distributed Computing\n")
	fmt.Printf("🔗 Server: %s\n", serverURL)

	switch command {
	case "coordinator":
		runCoordinator(client)

	case "worker":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go worker <worker-id> [node-type] [capacity]")
			return
		}
		runWorker(client, os.Args[2:])

	case "submit":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run main.go submit <task-type> <parameters...>")
			return
		}
		submitTask(client, os.Args[2:])

	case "monitor":
		monitorTasks(client)

	case "simulate":
		if len(os.Args) < 4 {
			fmt.Println("Usage: go run main.go simulate <num-workers> <num-tasks>")
			return
		}
		numWorkers, _ := strconv.Atoi(os.Args[2])
		numTasks, _ := strconv.Atoi(os.Args[3])
		simulateCluster(client, numWorkers, numTasks)

	case "benchmark":
		if len(os.Args) < 3 {
			fmt.Println("Usage: go run main.go benchmark <computation-type>")
			return
		}
		benchmarkComputation(client, os.Args[2])

	default:
		fmt.Printf("Unknown command: %s\n", command)
		showUsage()
	}
}

func showUsage() {
	fmt.Println("🖥️  Red Giant Distributed Computing")
	fmt.Println("Usage: go run main.go <command> [args...]")
	fmt.Println("")
	fmt.Println("Commands:")
	fmt.Println("  coordinator                     Run task coordinator")
	fmt.Println("  worker <id> [type] [capacity]   Run compute worker")
	fmt.Println("  submit <type> <params...>       Submit computation task")
	fmt.Println("  monitor                         Monitor all tasks")
	fmt.Println("  simulate <workers> <tasks>      Simulate compute cluster")
	fmt.Println("  benchmark <type>                Benchmark computation")
	fmt.Println("")
	fmt.Println("Task Types:")
	fmt.Println("  pi-monte-carlo <iterations>     Calculate π using Monte Carlo")
	fmt.Println("  matrix-multiply <size>          Matrix multiplication")
	fmt.Println("  prime-search <start> <end>      Find prime numbers")
	fmt.Println("  neural-training <epochs>        Neural network training")
	fmt.Println("  crypto-mining <difficulty>      Cryptocurrency mining simulation")
	fmt.Println("")
	fmt.Println("Examples:")
	fmt.Println("  go run main.go coordinator")
	fmt.Println("  go run main.go worker gpu-node-01 gpu 8")
	fmt.Println("  go run main.go submit pi-monte-carlo 1000000")
	fmt.Println("  go run main.go simulate 5 20")
	fmt.Println("")
	fmt.Println("Environment:")
	fmt.Println("  RED_GIANT_SERVER  Server URL (default: http://localhost:8080)")
}

func runCoordinator(client *redgiant.Client) {
	client.SetPeerID(fmt.Sprintf("compute_coordinator_%d", time.Now().Unix()))

	fmt.Printf("🎯 Starting Compute Coordinator\n")
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(3 * time.Second)
	defer ticker.Stop()

	taskQueue := make(map[string]*ComputeTask)
	completedTasks := 0
	lastCheck := time.Now().Add(-1 * time.Minute)

	for range ticker.C {
		// Check for new tasks
		files, err := client.SearchFiles("compute_task_")
		if err != nil {
			continue
		}

		// Process new tasks
		for _, file := range files {
			if file.UploadedAt.After(lastCheck) {
				task, err := downloadAndParseTask(client, file.ID)
				if err != nil {
					continue
				}

				if task.Status == "pending" {
					taskQueue[task.ID] = task
					fmt.Printf("📥 New task queued: %s (%s)\n", task.ID, task.Type)
				}
			}
		}

		// Check for available workers and assign tasks
		workers := findAvailableWorkers(client)

		for taskID, task := range taskQueue {
			if task.Status == "pending" && len(workers) > 0 {
				// Find best worker for this task
				worker := selectBestWorker(workers, task)
				if worker != "" {
					task.AssignedTo = worker
					task.Status = "assigned"

					// Update task
					if err := uploadTask(client, task); err == nil {
						fmt.Printf("📤 Task assigned: %s -> %s\n", taskID, worker)
						delete(taskQueue, taskID)

						// Remove worker from available list
						for i, w := range workers {
							if w == worker {
								workers = append(workers[:i], workers[i+1:]...)
								break
							}
						}
					}
				}
			}
		}

		// Check for completed tasks
		results, err := client.SearchFiles("compute_result_")
		if err == nil {
			for _, file := range results {
				if file.UploadedAt.After(lastCheck) {
					result, err := downloadAndParseResult(client, file.ID)
					if err == nil {
						completedTasks++
						fmt.Printf("✅ Task completed: %s by %s (%.2f MB/s)\n",
							result.TaskID, result.WorkerID, result.Performance.Throughput)
					}
				}
			}
		}

		// Show status
		if len(taskQueue) > 0 || completedTasks > 0 {
			fmt.Printf("📊 Status: %d queued, %d completed, %d workers available\n\n",
				len(taskQueue), completedTasks, len(workers))
		}

		lastCheck = time.Now()
	}
}

func runWorker(client *redgiant.Client, args []string) {
	workerID := args[0]
	nodeType := "cpu"
	capacity := 4

	if len(args) > 1 {
		nodeType = args[1]
	}
	if len(args) > 2 {
		capacity, _ = strconv.Atoi(args[2])
	}

	client.SetPeerID(fmt.Sprintf("compute_worker_%s", workerID))

	worker := &ComputeWorker{
		client:   client,
		workerID: workerID,
		nodeType: nodeType,
		capacity: capacity,
		running:  true,
		tasks:    make(map[string]*ComputeTask),
	}

	fmt.Printf("⚡ Starting Compute Worker: %s\n", workerID)
	fmt.Printf("🔧 Node type: %s\n", nodeType)
	fmt.Printf("💪 Capacity: %d concurrent tasks\n", capacity)
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	// Register worker
	worker.registerWorker()

	// Start task processing
	worker.startProcessing()
}

func (w *ComputeWorker) registerWorker() {
	registration := map[string]interface{}{
		"worker_id": w.workerID,
		"node_type": w.nodeType,
		"capacity":  w.capacity,
		"status":    "available",
		"timestamp": time.Now(),
		"peer_id":   w.client.PeerID,
	}

	fileName := fmt.Sprintf("worker_registration_%s.json", w.workerID)
	data, err := json.Marshal(registration)
	if err != nil {
		log.Printf("❌ Failed to marshal registration: %v", err)
		return
	}
	_, err = w.client.UploadData(data, fileName)
	if err != nil {
		log.Printf("❌ Failed to upload registration: %v", err)
		return
	}

	fmt.Printf("📝 Worker registered\n")
}

func (w *ComputeWorker) startProcessing() {
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	lastCheck := time.Now()

	for w.running {
		select {
		case <-ticker.C:
			// Check for assigned tasks
			files, err := w.client.SearchFiles("compute_task_")
			if err != nil {
				continue
			}

			for _, file := range files {
				if file.UploadedAt.After(lastCheck) {
					task, err := downloadAndParseTask(w.client, file.ID)
					if err != nil {
						continue
					}

					// Check if task is assigned to this worker
					if task.AssignedTo == w.workerID && task.Status == "assigned" {
						w.mu.Lock()
						if len(w.tasks) < w.capacity {
							w.tasks[task.ID] = task
							go w.processTask(task)
							fmt.Printf("🔄 Processing task: %s (%s)\n", task.ID, task.Type)
						}
						w.mu.Unlock()
					}
				}
			}

			lastCheck = time.Now()
		}
	}
}

func (w *ComputeWorker) processTask(task *ComputeTask) {
	defer func() {
		w.mu.Lock()
		delete(w.tasks, task.ID)
		w.mu.Unlock()
	}()

	start := time.Now()

	// Update task status
	task.Status = "running"
	task.Progress = 0.0
	uploadTask(w.client, task)

	// Simulate computation based on task type
	var result interface{}
	var err error

	switch task.Type {
	case "pi-monte-carlo":
		result, err = w.calculatePiMonteCarlo(task)
	case "matrix-multiply":
		result, err = w.matrixMultiply(task)
	case "prime-search":
		result, err = w.findPrimes(task)
	case "neural-training":
		result, err = w.neuralTraining(task)
	case "crypto-mining":
		result, err = w.cryptoMining(task)
	default:
		err = fmt.Errorf("unknown task type: %s", task.Type)
	}

	duration := time.Since(start)

	// Create result
	computeResult := ComputeResult{
		TaskID:    task.ID,
		WorkerID:  w.workerID,
		Result:    result,
		Duration:  duration.String(),
		Timestamp: time.Now(),
		Performance: Performance{
			CPUUsage:    70 + rand.Float64()*30,
			MemoryUsage: 40 + rand.Float64()*40,
			Throughput:  100 + rand.Float64()*400, // Simulate Red Giant throughput
		},
	}

	if err != nil {
		task.Status = "failed"
		task.Error = err.Error()
		fmt.Printf("❌ Task failed: %s - %v\n", task.ID, err)
	} else {
		task.Status = "completed"
		task.Progress = 100.0
		task.Result = result
		task.CompletedAt = time.Now()
		fmt.Printf("✅ Task completed: %s in %v\n", task.ID, duration)
	}

	// Upload result and updated task
	uploadTask(w.client, task)

	resultFileName := fmt.Sprintf("compute_result_%s_%s.json", task.ID, w.workerID)
	resultData, err := json.Marshal(computeResult)
	if err != nil {
		log.Printf("❌ Failed to marshal result: %v", err)
		return
	}
	_, err = w.client.UploadData(resultData, resultFileName)
	if err != nil {
		log.Printf("❌ Failed to upload result: %v", err)
	}
}

func (w *ComputeWorker) calculatePiMonteCarlo(task *ComputeTask) (interface{}, error) {
	iterations, ok := task.Parameters["iterations"].(float64)
	if !ok {
		return nil, fmt.Errorf("invalid iterations parameter")
	}

	n := int(iterations)
	inside := 0

	for i := 0; i < n; i++ {
		x := rand.Float64()
		y := rand.Float64()

		if x*x+y*y <= 1.0 {
			inside++
		}

		// Update progress periodically
		if i%10000 == 0 {
			task.Progress = float64(i) / float64(n) * 100
		}
	}

	pi := 4.0 * float64(inside) / float64(n)

	return map[string]interface{}{
		"pi_estimate": pi,
		"iterations":  n,
		"accuracy":    math.Abs(pi-math.Pi) / math.Pi * 100,
	}, nil
}

func (w *ComputeWorker) matrixMultiply(task *ComputeTask) (interface{}, error) {
	size, ok := task.Parameters["size"].(float64)
	if !ok {
		return nil, fmt.Errorf("invalid size parameter")
	}

	n := int(size)

	// Create random matrices
	a := make([][]float64, n)
	b := make([][]float64, n)
	c := make([][]float64, n)

	for i := 0; i < n; i++ {
		a[i] = make([]float64, n)
		b[i] = make([]float64, n)
		c[i] = make([]float64, n)

		for j := 0; j < n; j++ {
			a[i][j] = rand.Float64()
			b[i][j] = rand.Float64()
		}
	}

	// Matrix multiplication
	for i := 0; i < n; i++ {
		for j := 0; j < n; j++ {
			for k := 0; k < n; k++ {
				c[i][j] += a[i][k] * b[k][j]
			}
		}

		// Update progress
		task.Progress = float64(i) / float64(n) * 100
	}

	return map[string]interface{}{
		"matrix_size": n,
		"operations":  n * n * n,
		"checksum":    c[0][0] + c[n-1][n-1], // Simple checksum
	}, nil
}

func (w *ComputeWorker) findPrimes(task *ComputeTask) (interface{}, error) {
	start, ok1 := task.Parameters["start"].(float64)
	end, ok2 := task.Parameters["end"].(float64)

	if !ok1 || !ok2 {
		return nil, fmt.Errorf("invalid start/end parameters")
	}

	startNum := int(start)
	endNum := int(end)
	primes := make([]int, 0)

	for num := startNum; num <= endNum; num++ {
		if isPrime(num) {
			primes = append(primes, num)
		}

		// Update progress
		if (num-startNum)%1000 == 0 {
			task.Progress = float64(num-startNum) / float64(endNum-startNum) * 100
		}
	}

	return map[string]interface{}{
		"range":       fmt.Sprintf("%d-%d", startNum, endNum),
		"prime_count": len(primes),
		"largest":     primes[len(primes)-1],
	}, nil
}

func (w *ComputeWorker) neuralTraining(task *ComputeTask) (interface{}, error) {
	epochs, ok := task.Parameters["epochs"].(float64)
	if !ok {
		return nil, fmt.Errorf("invalid epochs parameter")
	}

	numEpochs := int(epochs)
	loss := 1.0

	for epoch := 0; epoch < numEpochs; epoch++ {
		// Simulate training step
		loss *= 0.99 // Gradual loss reduction

		// Simulate computation time
		time.Sleep(10 * time.Millisecond)

		// Update progress
		task.Progress = float64(epoch) / float64(numEpochs) * 100
	}

	return map[string]interface{}{
		"epochs":     numEpochs,
		"final_loss": loss,
		"accuracy":   (1.0 - loss) * 100,
	}, nil
}

func (w *ComputeWorker) cryptoMining(task *ComputeTask) (interface{}, error) {
	difficulty, ok := task.Parameters["difficulty"].(float64)
	if !ok {
		return nil, fmt.Errorf("invalid difficulty parameter")
	}

	target := int(difficulty)
	nonce := 0

	for {
		// Simulate hash computation
		hash := fmt.Sprintf("%x", rand.Uint64())

		// Check if hash meets difficulty
		if countLeadingZeros(hash) >= target {
			break
		}

		nonce++

		// Update progress (arbitrary)
		if nonce%1000 == 0 {
			task.Progress = math.Min(float64(nonce)/10000*100, 99)
		}

		// Prevent infinite loop
		if nonce > 100000 {
			break
		}
	}

	return map[string]interface{}{
		"difficulty": target,
		"nonce":      nonce,
		"hash_rate":  float64(nonce) / 1000, // Simulated hash rate
	}, nil
}

func submitTask(client *redgiant.Client, args []string) {
	taskType := args[0]

	task := &ComputeTask{
		ID:         fmt.Sprintf("task_%d", time.Now().UnixNano()),
		Type:       taskType,
		Parameters: make(map[string]interface{}),
		Priority:   1,
		CreatedAt:  time.Now(),
		Status:     "pending",
		Progress:   0.0,
	}

	// Parse parameters based on task type
	switch taskType {
	case "pi-monte-carlo":
		if len(args) > 1 {
			if iterations, err := strconv.Atoi(args[1]); err == nil {
				task.Parameters["iterations"] = float64(iterations)
			}
		}
	case "matrix-multiply":
		if len(args) > 1 {
			if size, err := strconv.Atoi(args[1]); err == nil {
				task.Parameters["size"] = float64(size)
			}
		}
	case "prime-search":
		if len(args) > 2 {
			if start, err := strconv.Atoi(args[1]); err == nil {
				if end, err := strconv.Atoi(args[2]); err == nil {
					task.Parameters["start"] = float64(start)
					task.Parameters["end"] = float64(end)
				}
			}
		}
	case "neural-training":
		if len(args) > 1 {
			if epochs, err := strconv.Atoi(args[1]); err == nil {
				task.Parameters["epochs"] = float64(epochs)
			}
		}
	case "crypto-mining":
		if len(args) > 1 {
			if difficulty, err := strconv.Atoi(args[1]); err == nil {
				task.Parameters["difficulty"] = float64(difficulty)
			}
		}
	}

	// Upload task
	fileName := fmt.Sprintf("compute_task_%s.json", task.ID)
	result, err := client.UploadJSON(task, fileName)
	if err != nil {
		log.Fatalf("❌ Failed to submit task: %v", err)
	}

	fmt.Printf("📤 Task submitted successfully!\n")
	fmt.Printf("   • Task ID: %s\n", task.ID)
	fmt.Printf("   • Type: %s\n", task.Type)
	fmt.Printf("   • Parameters: %v\n", task.Parameters)
	fmt.Printf("   • Upload throughput: %.2f MB/s\n", result.ThroughputMbps)
	fmt.Printf("\n💡 Monitor progress with: go run main.go monitor\n")
}

func monitorTasks(client *redgiant.Client) {
	client.SetPeerID(fmt.Sprintf("compute_monitor_%d", time.Now().Unix()))

	fmt.Printf("📊 Monitoring Compute Tasks\n")
	fmt.Printf("🔗 Peer ID: %s\n\n", client.PeerID)

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		// Get all tasks
		files, err := client.SearchFiles("compute_task_")
		if err != nil {
			continue
		}

		taskStats := make(map[string]int)
		var tasks []*ComputeTask

		for _, file := range files {
			task, err := downloadAndParseTask(client, file.ID)
			if err != nil {
				continue
			}

			tasks = append(tasks, task)
			taskStats[task.Status]++
		}

		if len(tasks) > 0 {
			fmt.Printf("📊 Compute Cluster Status\n")
			fmt.Printf("========================\n")
			fmt.Printf("Total tasks: %d\n", len(tasks))
			fmt.Printf("  • Pending: %d\n", taskStats["pending"])
			fmt.Printf("  • Assigned: %d\n", taskStats["assigned"])
			fmt.Printf("  • Running: %d\n", taskStats["running"])
			fmt.Printf("  • Completed: %d\n", taskStats["completed"])
			fmt.Printf("  • Failed: %d\n", taskStats["failed"])

			// Show recent tasks
			fmt.Printf("\nRecent Tasks:\n")
			for i, task := range tasks {
				if i >= 5 {
					break
				}

				status := task.Status
				if task.Status == "running" {
					status = fmt.Sprintf("running (%.1f%%)", task.Progress)
				}

				fmt.Printf("  • %s: %s [%s]\n", task.ID[:12], task.Type, status)
			}

			// Network performance
			stats, err := client.GetNetworkStats()
			if err == nil {
				fmt.Printf("\nNetwork Performance:\n")
				fmt.Printf("  • Throughput: %.2f MB/s\n", stats.ThroughputMbps)
				fmt.Printf("  • Latency: %d ms\n", stats.AverageLatency)
			}

			fmt.Printf("========================\n\n")
		}
	}
}

func simulateCluster(client *redgiant.Client, numWorkers, numTasks int) {
	fmt.Printf("🌐 Simulating Compute Cluster\n")
	fmt.Printf("⚡ Workers: %d\n", numWorkers)
	fmt.Printf("📋 Tasks: %d\n\n", numTasks)

	// Start coordinator
	go runCoordinator(client)
	time.Sleep(2 * time.Second)

	// Start workers
	for i := 0; i < numWorkers; i++ {
		workerID := fmt.Sprintf("sim_worker_%03d", i+1)
		nodeType := "cpu"
		if i%3 == 0 {
			nodeType = "gpu"
		}

		go func(id, nType string) {
			workerClient := redgiant.NewClient(client.BaseURL)
			args := []string{id, nType, "2"}
			runWorker(workerClient, args)
		}(workerID, nodeType)

		time.Sleep(100 * time.Millisecond)
	}

	// Submit tasks
	time.Sleep(5 * time.Second)

	taskTypes := []string{"pi-monte-carlo", "matrix-multiply", "prime-search", "neural-training"}

	for i := 0; i < numTasks; i++ {
		taskType := taskTypes[rand.Intn(len(taskTypes))]

		go func(tType string, index int) {
			taskClient := redgiant.NewClient(client.BaseURL)

			var args []string
			switch tType {
			case "pi-monte-carlo":
				args = []string{tType, strconv.Itoa(100000 + rand.Intn(900000))}
			case "matrix-multiply":
				args = []string{tType, strconv.Itoa(50 + rand.Intn(100))}
			case "prime-search":
				start := rand.Intn(10000)
				args = []string{tType, strconv.Itoa(start), strconv.Itoa(start + 1000)}
			case "neural-training":
				args = []string{tType, strconv.Itoa(10 + rand.Intn(90))}
			}

			submitTask(taskClient, args)
		}(taskType, i)

		time.Sleep(time.Duration(500+rand.Intn(1500)) * time.Millisecond)
	}

	// Keep simulation running
	select {}
}

func benchmarkComputation(client *redgiant.Client, computationType string) {
	fmt.Printf("🏁 Benchmarking Computation: %s\n\n", computationType)

	// Submit multiple tasks of the same type
	numTasks := 10
	tasks := make([]string, numTasks)

	start := time.Now()

	for i := 0; i < numTasks; i++ {
		var args []string
		switch computationType {
		case "pi-monte-carlo":
			args = []string{computationType, "1000000"}
		case "matrix-multiply":
			args = []string{computationType, "100"}
		case "prime-search":
			args = []string{computationType, "1000", "10000"}
		default:
			log.Fatalf("Unknown computation type: %s", computationType)
		}

		taskClient := redgiant.NewClient(client.BaseURL)
		submitTask(taskClient, args)

		// Extract task ID (simplified)
		tasks[i] = fmt.Sprintf("task_%d", time.Now().UnixNano())
	}

	fmt.Printf("📊 Benchmark submitted %d %s tasks\n", numTasks, computationType)
	fmt.Printf("⏱️  Submission time: %v\n", time.Since(start))
	fmt.Printf("🔍 Monitor with: go run main.go monitor\n")
}

// Helper functions will be defined at the end of the file

func findAvailableWorkers(client *redgiant.Client) []string {
	files, err := client.SearchFiles("worker_registration_")
	if err != nil {
		return nil
	}

	var workers []string
	cutoff := time.Now().Add(-30 * time.Second) // Consider workers active in last 30 seconds

	for _, file := range files {
		if file.UploadedAt.After(cutoff) {
			// Extract worker ID from filename
			parts := strings.Split(file.Name, "_")
			if len(parts) >= 3 {
				workerID := strings.Join(parts[2:], "_")
				workerID = strings.TrimSuffix(workerID, ".json")
				workers = append(workers, workerID)
			}
		}
	}

	return workers
}

func selectBestWorker(workers []string, task *ComputeTask) string {
	if len(workers) == 0 {
		return ""
	}

	// Simple selection - return first available worker
	// In production, implement more sophisticated selection based on:
	// - Worker capacity and current load
	// - Node type (CPU vs GPU) matching task requirements
	// - Historical performance
	// - Network proximity

	return workers[0]
}

func isPrime(n int) bool {
	if n < 2 {
		return false
	}
	if n == 2 {
		return true
	}
	if n%2 == 0 {
		return false
	}

	for i := 3; i*i <= n; i += 2 {
		if n%i == 0 {
			return false
		}
	}

	return true
}

func countLeadingZeros(hash string) int {
	count := 0
	for _, char := range hash {
		if char == '0' {
			count++
		} else {
			break
		}
	}
	return count
}

func downloadAndParseTask(client *redgiant.Client, fileID string) (*ComputeTask, error) {
	data, err := client.DownloadData(fileID)
	if err != nil {
		return nil, fmt.Errorf("failed to download task: %v", err)
	}

	var task ComputeTask
	if err := json.Unmarshal(data, &task); err != nil {
		return nil, fmt.Errorf("failed to parse task: %v", err)
	}

	return &task, nil
}

func downloadAndParseResult(client *redgiant.Client, fileID string) (*ComputeResult, error) {
	data, err := client.DownloadData(fileID)
	if err != nil {
		return nil, fmt.Errorf("failed to download result: %v", err)
	}

	var result ComputeResult
	if err := json.Unmarshal(data, &result); err != nil {
		return nil, fmt.Errorf("failed to parse result: %v", err)
	}

	return &result, nil
}

func uploadTask(client *redgiant.Client, task *ComputeTask) error {
	data, err := json.Marshal(task)
	if err != nil {
		return fmt.Errorf("failed to marshal task: %v", err)
	}

	filename := fmt.Sprintf("compute_task_%s.json", task.ID)
	_, err = client.UploadData(data, filename)
	if err != nil {
		return fmt.Errorf("failed to upload task: %v", err)
	}

	return nil
}
