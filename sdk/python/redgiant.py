"""
Red Giant Protocol - Python SDK
High-performance client library for Python applications
"""

import json
import time
import threading
import requests
import random
from typing import Dict, List, Optional, Callable, Any
from dataclasses import dataclass
from pathlib import Path


@dataclass
class FileInfo:
    id: str
    name: str
    size: int
    hash: str
    uploaded_at: str
    peer_id: Optional[str] = None
    md5_hash: Optional[str] = None
    content_type: Optional[str] = None


@dataclass
class UploadResult:
    status: str
    file_id: str
    bytes_processed: int
    chunks_processed: int
    throughput_mbps: float
    processing_time_ms: int
    message: str = "Upload completed"
    content_type: Optional[str] = None
    original_size: Optional[int] = None
    is_compressed: Optional[bool] = None
    optimal_chunk: Optional[int] = None


@dataclass
class NetworkStats:
    total_requests: int
    total_bytes: int
    total_chunks: int
    average_latency_ms: int
    error_count: int
    uptime_seconds: int
    throughput_mbps: float
    timestamp: int


class RedGiantClient:
    """High-performance Red Giant Protocol client for Python"""
    
    def __init__(self, base_url: str, peer_id: Optional[str] = None, timeout: int = 60):
        self.base_url = base_url.rstrip('/')
        self.peer_id = peer_id or f"python_client_{int(time.time())}"
        self.timeout = timeout
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'RedGiant-Python-SDK/1.0'
        })
    
    def set_peer_id(self, peer_id: str):
        """Set a custom peer ID"""
        self.peer_id = peer_id
    
    def upload_file(self, file_path: str) -> UploadResult:
        """Upload a file to the Red Giant network"""
        file_path = Path(file_path)
        
        if not file_path.exists():
            raise FileNotFoundError(f"File not found: {file_path}")
        
        with open(file_path, 'rb') as f:
            data = f.read()
        
        return self.upload_data(data, file_path.name)
    
    def upload_data(self, data: bytes, file_name: str) -> UploadResult:
        """Upload raw data to the Red Giant network"""
        headers = {
            'Content-Type': 'application/octet-stream',
            'X-Peer-ID': self.peer_id,
            'X-File-Name': file_name
        }
        
        response = self.session.post(
            f"{self.base_url}/upload",
            data=data,
            headers=headers,
            timeout=self.timeout
        )
        
        if response.status_code != 200:
            raise Exception(f"Upload failed: {response.status_code} {response.text}")
        
        result_data = response.json()
        # Filter out any unexpected fields to avoid dataclass errors
        expected_fields = {
            'status', 'file_id', 'bytes_processed', 'chunks_processed', 
            'throughput_mbps', 'processing_time_ms', 'message',
            'content_type', 'original_size', 'is_compressed', 'optimal_chunk'
        }
        filtered_data = {k: v for k, v in result_data.items() if k in expected_fields}
        return UploadResult(**filtered_data)
    
    def upload_json(self, data: Dict[str, Any], file_name: str) -> UploadResult:
        """Upload JSON data to the Red Giant network"""
        json_data = json.dumps(data, indent=2).encode('utf-8')
        return self.upload_data(json_data, file_name)
    
    def download_file(self, file_id: str, output_path: str) -> None:
        """Download a file from the Red Giant network"""
        headers = {
            'X-Peer-ID': self.peer_id
        }
        
        response = self.session.get(
            f"{self.base_url}/download/{file_id}",
            headers=headers,
            timeout=self.timeout
        )
        
        if response.status_code != 200:
            raise Exception(f"Download failed: {response.status_code}")
        
        with open(output_path, 'wb') as f:
            f.write(response.content)
    
    def download_data(self, file_id: str) -> bytes:
        """Download file data directly into memory"""
        headers = {
            'X-Peer-ID': self.peer_id
        }
        
        response = self.session.get(
            f"{self.base_url}/download/{file_id}",
            headers=headers,
            timeout=self.timeout
        )
        
        if response.status_code != 200:
            raise Exception(f"Download failed: {response.status_code}")
        
        return response.content
    
    def list_files(self) -> List[FileInfo]:
        """List all files in the Red Giant network"""
        response = self.session.get(f"{self.base_url}/files", timeout=self.timeout)
        
        if response.status_code != 200:
            raise Exception(f"List files failed: {response.status_code}")
        
        result = response.json()
        return [FileInfo(**file_data) for file_data in result['files']]
    
    def search_files(self, pattern: str) -> List[FileInfo]:
        """Search for files by name pattern"""
        # Use /files endpoint and filter client-side for now
        response = self.session.get(f"{self.base_url}/files", timeout=self.timeout)
        
        if response.status_code != 200:
            raise Exception(f"Search failed: {response.status_code}")
        
        result = response.json()
        all_files = []
        
        for file_data in result.get('files', []):
            # Filter out unexpected fields to avoid dataclass errors
            expected_fields = {
                'id', 'name', 'size', 'hash', 'peer_id', 'uploaded_at', 
                'md5_hash', 'content_type'
            }
            filtered_data = {k: v for k, v in file_data.items() if k in expected_fields}
            all_files.append(FileInfo(**filtered_data))
        
        # Filter files by pattern
        matching_files = [f for f in all_files if pattern in f.name]
        return matching_files
    
    def process_data(self, data: bytes) -> Dict[str, Any]:
        """Process raw data using Red Giant's high-performance C core"""
        response = self.session.post(
            f"{self.base_url}/process",
            data=data,
            headers={'Content-Type': 'application/octet-stream'},
            timeout=self.timeout
        )
        
        if response.status_code != 200:
            raise Exception(f"Processing failed: {response.status_code} {response.text}")
        
        return response.json()
    
    def get_network_stats(self) -> NetworkStats:
        """Retrieve network performance statistics"""
        response = self.session.get(f"{self.base_url}/metrics", timeout=self.timeout)
        
        if response.status_code != 200:
            raise Exception(f"Stats request failed: {response.status_code}")
        
        stats_data = response.json()
        return NetworkStats(**stats_data)
    
    def health_check(self) -> Dict[str, Any]:
        """Check if the Red Giant server is healthy"""
        response = self.session.get(f"{self.base_url}/health", timeout=self.timeout)
        
        if response.status_code != 200:
            raise Exception(f"Health check failed: {response.status_code}")
        
        return response.json()


class ChatRoom:
    """Real-time P2P chat using Red Giant Protocol"""
    
    def __init__(self, client: RedGiantClient, room_id: str, username: str,
                 on_message: Optional[Callable] = None, poll_interval: float = 1.0):
        self.client = client
        self.room_id = room_id
        self.username = username
        self.on_message = on_message or (lambda msg: None)
        self.poll_interval = poll_interval
        self.messages = []
        self.is_polling = False
        self.poll_thread = None
        self.last_check = time.time() - 60  # Check last minute initially
    
    def join(self):
        """Join the chat room"""
        self.send_system_message(f"{self.username} joined the chat")
        self.start_polling()
    
    def leave(self):
        """Leave the chat room"""
        self.send_system_message(f"{self.username} left the chat")
        self.stop_polling()
    
    def send_message(self, content: str, recipient: Optional[str] = None):
        """Send a message to the chat room"""
        message = {
            'id': f"msg_{int(time.time() * 1000)}_{hash(content) % 10000}",
            'from': self.username,
            'to': recipient or self.room_id,
            'content': content,
            'timestamp': time.time(),
            'type': 'private' if recipient else 'public'
        }
        
        file_name = f"chat_{self.room_id}_{message['type']}_{int(time.time() * 1000)}.json"
        self.client.upload_json(message, file_name)
        
        self.messages.append(message)
        return message
    
    def send_system_message(self, content: str):
        """Send a system message"""
        message = {
            'id': f"sys_{int(time.time() * 1000)}",
            'from': 'system',
            'to': self.room_id,
            'content': content,
            'timestamp': time.time(),
            'type': 'system'
        }
        
        file_name = f"chat_{self.room_id}_system_{int(time.time() * 1000)}.json"
        self.client.upload_json(message, file_name)
        
        return message
    
    def start_polling(self):
        """Start polling for new messages"""
        if self.is_polling:
            return
        
        self.is_polling = True
        self.poll_thread = threading.Thread(target=self._poll_messages, daemon=True)
        self.poll_thread.start()
    
    def stop_polling(self):
        """Stop polling for messages"""
        self.is_polling = False
        if self.poll_thread:
            self.poll_thread.join(timeout=2)
    
    def _poll_messages(self):
        """Internal method to poll for new messages"""
        processed_messages = set()
        
        while self.is_polling:
            try:
                pattern = f"chat_{self.room_id}_"
                files = self.client.search_files(pattern)
                
                for file_info in files:
                    # Skip if we've already processed this message
                    if file_info.id in processed_messages:
                        continue
                    
                    try:
                        # Handle different date formats
                        if hasattr(file_info, 'uploaded_at') and file_info.uploaded_at:
                            if '.' in file_info.uploaded_at:
                                file_time = time.mktime(time.strptime(file_info.uploaded_at, "%Y-%m-%dT%H:%M:%S.%fZ"))
                            else:
                                file_time = time.mktime(time.strptime(file_info.uploaded_at, "%Y-%m-%dT%H:%M:%SZ"))
                        else:
                            file_time = time.time()
                    except (ValueError, AttributeError):
                        # Fallback to current time if parsing fails
                        file_time = time.time()
                    
                    if file_time > self.last_check and (not file_info.peer_id or file_info.peer_id != self.client.peer_id):
                        try:
                            data = self.client.download_data(file_info.id)
                            message = json.loads(data.decode('utf-8'))
                            
                            # Validate message structure and check if it's for this room/user
                            if (message.get('from') and message.get('content') and 
                                message['from'] != self.username and
                                (message.get('to') == self.room_id or 
                                 message.get('to') == self.username or 
                                 message.get('to') == '*')):
                                
                                self.messages.append(message)
                                self.on_message(message)
                                processed_messages.add(file_info.id)
                                
                        except Exception as e:
                            print(f"Error processing message {file_info.id}: {e}")
                
                self.last_check = time.time()
                
            except Exception as e:
                print(f"Polling error: {e}")
            
            time.sleep(self.poll_interval)


class IoTDevice:
    """IoT device simulator using Red Giant Protocol"""
    
    def __init__(self, client: RedGiantClient, device_id: str,
                 sensors: Optional[List[str]] = None, batch_size: int = 10,
                 interval: float = 5.0, on_reading: Optional[Callable] = None,
                 on_batch: Optional[Callable] = None):
        self.client = client
        self.device_id = device_id
        self.sensors = sensors or ['temperature', 'humidity', 'pressure']
        self.batch_size = batch_size
        self.interval = interval
        self.on_reading = on_reading or (lambda reading: None)
        self.on_batch = on_batch or (lambda batch, count: None)
        self.is_streaming = False
        self.stream_thread = None
        self.batch_count = 0
        self.location = {
            'latitude': 40.7128,
            'longitude': -74.0060,
            'altitude': 0.0
        }
    
    def start_streaming(self):
        """Start streaming sensor data"""
        if self.is_streaming:
            return
        
        self.is_streaming = True
        self.batch_count = 0
        self.stream_thread = threading.Thread(target=self._stream_data, daemon=True)
        self.stream_thread.start()
    
    def stop_streaming(self):
        """Stop streaming sensor data"""
        self.is_streaming = False
        if self.stream_thread:
            self.stream_thread.join(timeout=2)
    
    def _stream_data(self):
        """Internal method to stream sensor data"""
        while self.is_streaming:
            try:
                batch = self._generate_batch()
                self._upload_batch(batch)
                self.batch_count += 1
                self.on_batch(batch, self.batch_count)
                
            except Exception as e:
                print(f"Streaming error: {e}")
            
            time.sleep(self.interval)
    
    def _generate_batch(self):
        """Generate a batch of sensor readings"""
        readings = []
        
        for i in range(self.batch_size):
            sensor_type = random.choice(self.sensors)
            
            reading = {
                'device_id': self.device_id,
                'sensor_type': sensor_type,
                'value': self._generate_sensor_value(sensor_type),
                'unit': self._get_sensor_unit(sensor_type),
                'timestamp': time.time(),
                'location': self.location,
                'metadata': {
                    'battery_level': 80 + random.random() * 20,
                    'signal_strength': -60 + random.random() * 40,
                    'firmware': 'v1.2.3'
                }
            }
            
            readings.append(reading)
            self.on_reading(reading)
        
        return {
            'batch_id': f"batch_{self.device_id}_{int(time.time() * 1000)}",
            'device_id': self.device_id,
            'readings': readings,
            'timestamp': time.time(),
            'count': len(readings)
        }
    
    def _upload_batch(self, batch):
        """Upload sensor batch to Red Giant network"""
        file_name = f"iot_batch_{self.device_id}_{int(time.time() * 1000)}.json"
        return self.client.upload_json(batch, file_name)
    
    def _generate_sensor_value(self, sensor_type: str) -> float:
        """Generate realistic sensor values"""
        import random
        
        if sensor_type == 'temperature':
            return 20 + random.random() * 15  # 20-35°C
        elif sensor_type == 'humidity':
            return 30 + random.random() * 40  # 30-70%
        elif sensor_type == 'pressure':
            return 1000 + random.random() * 50  # 1000-1050 hPa
        elif sensor_type == 'light':
            return random.random() * 1000  # 0-1000 lux
        elif sensor_type == 'motion':
            return round(random.random())  # 0 or 1
        else:
            return random.random() * 100
    
    def _get_sensor_unit(self, sensor_type: str) -> str:
        """Get unit for sensor type"""
        units = {
            'temperature': '°C',
            'humidity': '%',
            'pressure': 'hPa',
            'light': 'lux',
            'motion': 'bool'
        }
        return units.get(sensor_type, 'unit')


class TokenStream:
    """AI token streaming using Red Giant Protocol"""
    
    def __init__(self, client: RedGiantClient, session_id: str,
                 model_name: str = 'default', on_token: Optional[Callable] = None,
                 on_complete: Optional[Callable] = None):
        self.client = client
        self.session_id = session_id
        self.model_name = model_name
        self.on_token = on_token or (lambda token, result: None)
        self.on_complete = on_complete or (lambda session: None)
        self.tokens = []
    
    def stream_token(self, token: str, position: int) -> UploadResult:
        """Stream a single token"""
        ai_token = {
            'id': f"token_{self.session_id}_{position}",
            'session_id': self.session_id,
            'token': token,
            'position': position,
            'timestamp': time.time(),
            'metadata': {
                'confidence': 0.9,
                'token_type': 'word',
                'model_name': self.model_name,
                'temperature': 0.7
            }
        }
        
        file_name = f"ai_token_{self.session_id}_{position}.json"
        result = self.client.upload_json(ai_token, file_name)
        
        self.tokens.append(ai_token)
        self.on_token(ai_token, result)
        
        return result
    
    def stream_response(self, response: str, tokens_per_second: int = 50):
        """Stream an entire response token by token"""
        tokens = self._tokenize(response)
        interval = 1.0 / tokens_per_second
        
        for i, token in enumerate(tokens):
            self.stream_token(token, i)
            
            if i < len(tokens) - 1:
                time.sleep(interval)
        
        self.on_complete({
            'session_id': self.session_id,
            'total_tokens': len(tokens),
            'tokens': self.tokens
        })
    
    def _tokenize(self, text: str) -> List[str]:
        """Simple tokenization - replace with proper tokenizer in production"""
        return [token for token in text.split() if token.strip()]


# Example usage and testing
if __name__ == "__main__":
    # Example usage
    client = RedGiantClient("http://localhost:8080")
    
    # Test connection
    try:
        health = client.health_check()
        print(f"✅ Connected to Red Giant server: {health}")
    except Exception as e:
        print(f"❌ Connection failed: {e}")
        exit(1)
    
    # Example file upload
    try:
        result = client.upload_data(b"Hello, Red Giant!", "test.txt")
        print(f"📤 Upload successful: {result.file_id} ({result.throughput_mbps:.2f} MB/s)")
    except Exception as e:
        print(f"❌ Upload failed: {e}")
    
    # Example file listing
    try:
        files = client.list_files()
        print(f"📁 Found {len(files)} files in network")
        for file_info in files[:5]:  # Show first 5
            print(f"   • {file_info.name} ({file_info.size} bytes)")
    except Exception as e:
        print(f"❌ List files failed: {e}")
    
    # Example network stats
    try:
        stats = client.get_network_stats()
        print(f"📊 Network stats: {stats.throughput_mbps:.2f} MB/s, {stats.total_requests} requests")
    except Exception as e:
        print(f"❌ Stats failed: {e}")