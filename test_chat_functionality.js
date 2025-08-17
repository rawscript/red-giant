// Red Giant Protocol - JavaScript Chat Functionality Test
const { RedGiantClient, ChatRoom } = require('./sdk/javascript/redgiant.js');

async function testChatFunctionality() {
    console.log('🧪 Testing Red Giant JavaScript Chat Functionality...');
    
    // Create two clients to simulate chat
    const client1 = new RedGiantClient({
        baseURL: 'http://localhost:5000',
        peerID: 'alice_js'
    });
    
    const client2 = new RedGiantClient({
        baseURL: 'http://localhost:5000',
        peerID: 'bob_js'
    });
    
    console.log(`✅ Created clients: ${client1.peerID} and ${client2.peerID}`);
    
    // Test health check
    try {
        await client1.healthCheck();
        await client2.healthCheck();
        console.log('✅ Both clients connected to server');
    } catch (e) {
        console.log(`❌ Server connection failed: ${e.message}`);
        return;
    }
    
    // Create chat rooms
    const messagesReceived = [];
    
    const room1 = new ChatRoom(client1, 'test_room_js', 'alice_js', {
        onMessage: (msg) => {
            console.log(`📩 Alice received: ${msg.from}: ${msg.content}`);
            messagesReceived.push(['alice', msg]);
        },
        pollInterval: 500
    });
    
    const room2 = new ChatRoom(client2, 'test_room_js', 'bob_js', {
        onMessage: (msg) => {
            console.log(`📩 Bob received: ${msg.from}: ${msg.content}`);
            messagesReceived.push(['bob', msg]);
        },
        pollInterval: 500
    });
    
    console.log('✅ Created chat rooms');
    
    // Test joining
    try {
        await room1.join();
        await room2.join();
        console.log('✅ Both users joined the chat room');
        await sleep(1000); // Wait for join messages to propagate
    } catch (e) {
        console.log(`❌ Failed to join rooms: ${e.message}`);
        return;
    }
    
    // Test public messages
    try {
        await room1.sendMessage('Hello from Alice JS!');
        await sleep(500);
        await room2.sendMessage('Hi Alice, this is Bob JS!');
        await sleep(500);
        await room1.sendMessage('How are you doing, Bob?');
        console.log('✅ Sent public messages');
        await sleep(2000); // Wait for messages to be received
    } catch (e) {
        console.log(`❌ Failed to send public messages: ${e.message}`);
    }
    
    // Test private messages
    try {
        await room1.sendMessage('This is a private message for you!', 'bob_js');
        await sleep(500);
        await room2.sendMessage('Thanks for the private message!', 'alice_js');
        console.log('✅ Sent private messages');
        await sleep(2000); // Wait for messages to be received
    } catch (e) {
        console.log(`❌ Failed to send private messages: ${e.message}`);
    }
    
    // Test message history
    try {
        console.log(`✅ Alice has ${room1.messages.length} messages in history`);
        console.log(`✅ Bob has ${room2.messages.length} messages in history`);
        
        // Show last 3 messages from Alice's history
        const recentMessages = room1.messages.slice(-3);
        for (const msg of recentMessages) {
            console.log(`   Alice history: ${msg.from} -> ${msg.to}: ${msg.content}`);
        }
    } catch (e) {
        console.log(`❌ Failed to get message history: ${e.message}`);
    }
    
    // Clean up
    try {
        await room1.leave();
        await room2.leave();
        console.log('✅ Both users left the chat room');
    } catch (e) {
        console.log(`⚠️  Cleanup warning: ${e.message}`);
    }
    
    console.log(`\n🎉 JavaScript Chat test completed!`);
    console.log(`📊 Total messages received: ${messagesReceived.length}`);
    
    if (messagesReceived.length > 0) {
        console.log('✅ JavaScript Chat functionality is working!');
    } else {
        console.log('⚠️  No messages were received - check server and polling');
    }
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

// Run test if this file is executed directly
if (require.main === module) {
    testChatFunctionality().catch(console.error);
}

module.exports = { testChatFunctionality };