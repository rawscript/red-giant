// Basic test to verify the package structure
const assert = require('assert');

try {
  // Test that the package can be loaded
  const rgtp = require('../index.js');
  console.log('✅ Package loads successfully');
  
  // Test that basic functions exist
  assert(typeof rgtp.Session === 'function', 'Session class should exist');
  assert(typeof rgtp.Client === 'function', 'Client class should exist');
  assert(typeof rgtp.sendFile === 'function', 'sendFile function should exist');
  assert(typeof rgtp.receiveFile === 'function', 'receiveFile function should exist');
  console.log('✅ All expected functions exist');
  
  // Test that utility functions exist
  assert(typeof rgtp.formatBytes === 'function', 'formatBytes function should exist');
  assert(typeof rgtp.createLANConfig === 'function', 'createLANConfig should exist');
  assert(typeof rgtp.createWANConfig === 'function', 'createWANConfig should exist');
  assert(typeof rgtp.createMobileConfig === 'function', 'createMobileConfig should exist');
  console.log('✅ All utility functions exist');
  
  // Test that adapter can be loaded
  const adapters = require('../http-adapter.js');
  assert(adapters.RGTPHTTPAdapter, 'HTTP Adapter should exist');
  assert(adapters.RGTPWeb3Interface, 'Web3 Interface should exist');
  console.log('✅ Adapters load successfully');
  
  console.log('\n🎉 All tests passed! Package is ready for publishing.');
  
} catch (error) {
  console.error('❌ Test failed:', error.message);
  process.exit(1);
}