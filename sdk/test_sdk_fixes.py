#!/usr/bin/env python3
"""Red Giant SDK - Python Test"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), 'python'))

from redgiant import RedGiantClient, ChatRoom, IoTDevice

def test_python_sdk():
    print('🧪 Testing Python SDK fixes...')
    
    # Test 1: Client creation
    client = RedGiantClient('http://localhost:5000', peer_id='test_peer_python')
    print(f'✅ Client created with peer ID: {client.peer_id}')
    
    # Test 2: Health check (will fail if server not running)
    try:
        health = client.health_check()
        print('✅ Health check passed:', health)
    except Exception as error:
        print(f'⚠️  Health check failed (server may not be running): {error}')
    
    # Test 3: Upload test
    try:
        result = client.upload_data(b'Hello from Python SDK!', 'test.txt')
        print('✅ Upload successful:', result)
    except Exception as error:
        print(f'⚠️  Upload failed (server may not be running): {error}')
    
    # Test 4: IoT device simulation
    try:
        device = IoTDevice(client, 'test_device_001', sensors=['temperature', 'humidity'])
        batch = device._generate_batch()
        print('✅ IoT batch generated:', len(batch['readings']), 'readings')
    except Exception as error:
        print(f'❌ IoT test failed: {error}')
    
    print('🎉 Python SDK test completed!')

if __name__ == '__main__':
    test_python_sdk()