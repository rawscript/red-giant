#!/usr/bin/env python3
"""Red Giant Protocol - Chat Functionality Test"""

import sys
import os
import time
import threading
sys.path.append(os.path.join(os.path.dirname(__file__), 'sdk', 'python'))

from redgiant import RedGiantClient, ChatRoom

def test_chat_functionality():
    print('🧪 Testing Red Giant Chat Functionality...')
    
    # Create two clients to simulate chat
    client1 = RedGiantClient('http://localhost:5000', peer_id='alice')
    client2 = RedGiantClient('http://localhost:5000', peer_id='bob')
    
    print(f'✅ Created clients: {client1.peer_id} and {client2.peer_id}')
    
    # Test health check
    try:
        health1 = client1.health_check()
        health2 = client2.health_check()
        print('✅ Both clients connected to server')
    except Exception as e:
        print(f'❌ Server connection failed: {e}')
        return
    
    # Create chat rooms
    messages_received = []
    
    def on_message_alice(msg):
        print(f'📩 Alice received: {msg["from"]}: {msg["content"]}')
        messages_received.append(('alice', msg))
    
    def on_message_bob(msg):
        print(f'📩 Bob received: {msg["from"]}: {msg["content"]}')
        messages_received.append(('bob', msg))
    
    room1 = ChatRoom(client1, 'test_room', 'alice', on_message=on_message_alice, poll_interval=0.5)
    room2 = ChatRoom(client2, 'test_room', 'bob', on_message=on_message_bob, poll_interval=0.5)
    
    print('✅ Created chat rooms')
    
    # Test joining
    try:
        room1.join()
        room2.join()
        print('✅ Both users joined the chat room')
        time.sleep(1)  # Wait for join messages to propagate
    except Exception as e:
        print(f'❌ Failed to join rooms: {e}')
        return
    
    # Test public messages
    try:
        room1.send_message('Hello from Alice!')
        time.sleep(0.5)
        room2.send_message('Hi Alice, this is Bob!')
        time.sleep(0.5)
        room1.send_message('How are you doing, Bob?')
        print('✅ Sent public messages')
        time.sleep(2)  # Wait for messages to be received
    except Exception as e:
        print(f'❌ Failed to send public messages: {e}')
    
    # Test private messages
    try:
        room1.send_message('This is a private message for you!', recipient='bob')
        time.sleep(0.5)
        room2.send_message('Thanks for the private message!', recipient='alice')
        print('✅ Sent private messages')
        time.sleep(2)  # Wait for messages to be received
    except Exception as e:
        print(f'❌ Failed to send private messages: {e}')
    
    # Test message history
    try:
        messages1 = room1.messages
        messages2 = room2.messages
        print(f'✅ Alice has {len(messages1)} messages in history')
        print(f'✅ Bob has {len(messages2)} messages in history')
        
        for msg in messages1[-3:]:  # Show last 3 messages
            print(f'   Alice history: {msg["from"]} -> {msg["to"]}: {msg["content"]}')
            
    except Exception as e:
        print(f'❌ Failed to get message history: {e}')
    
    # Clean up
    try:
        room1.leave()
        room2.leave()
        print('✅ Both users left the chat room')
    except Exception as e:
        print(f'⚠️  Cleanup warning: {e}')
    
    print(f'\n🎉 Chat test completed!')
    print(f'📊 Total messages received: {len(messages_received)}')
    
    if len(messages_received) > 0:
        print('✅ Chat functionality is working!')
    else:
        print('⚠️  No messages were received - check server and polling')

if __name__ == '__main__':
    test_chat_functionality()