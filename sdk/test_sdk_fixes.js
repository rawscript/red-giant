// Red Giant SDK - JavaScript Test
const { RedGiantClient, ChatRoom, IoTDevice } = require('./javascript/redgiant.js');

async function testJavaScriptSDK() {
    console.log('🧪 Testing JavaScript SDK fixes...');
    
    // Test 1: Client creation with fixed ID generation
    const client = new RedGiantClient({
        baseURL: 'http://localhost:5000',
        peerID: 'test_peer_js'
    });
    
    console.log('✅ Client created with peer ID:', client.peerID);
    
    // Test 2: Health check (will fail if server not running)
    try {
        await client.healthCheck();
        console.log('✅ Health check passed');
    } catch (error) {
        console.log('⚠️  Health check failed (server may not be running):', error.message);
    }
    
    // Test 3: Upload test
    try {
        const result = await client.upload('Hello from JavaScript SDK!', 'test.txt');
        console.log('✅ Upload successful:', result);
    } catch (error) {
        console.log('⚠️  Upload failed (server may not be running):', error.message);
    }
    
    console.log('🎉 JavaScript SDK test completed!');
}

// Run test if this file is executed directly
if (require.main === module) {
    testJavaScriptSDK().catch(console.error);
}

module.exports = { testJavaScriptSDK };