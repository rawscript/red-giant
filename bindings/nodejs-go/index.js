const addon = require('./build/Release/rgtp_bridge.node');

class RGTP {
    constructor() {
        this.initialized = false;
    }

    async initialize() {
        if (this.initialized) {
            return 'Already initialized';
        }
        
        const result = addon.initialize();
        if (result === 'OK') {
            this.initialized = true;
        }
        return result;
    }

    cleanup() {
        if (this.initialized) {
            addon.cleanup();
            this.initialized = false;
        }
    }

    version() {
        return addon.version();
    }

    async sendFile(filename, chunkSize = 1024 * 1024, exposureRate = 1000) {
        if (!this.initialized) {
            throw new Error('RGTP not initialized');
        }
        
        return addon.sendFile(filename, chunkSize, exposureRate);
    }

    async receiveFile(host, port, filename, chunkSize = 1024 * 1024) {
        if (!this.initialized) {
            throw new Error('RGTP not initialized');
        }
        
        return addon.receiveFile(host, port, filename, chunkSize);
    }
}

// Convenience functions
const rgtp = new RGTP();

const sendFile = async (filename, options = {}) => {
    const { chunkSize = 1024 * 1024, exposureRate = 1000 } = options;
    await rgtp.initialize();
    return rgtp.sendFile(filename, chunkSize, exposureRate);
};

const receiveFile = async (host, port, filename, options = {}) => {
    const { chunkSize = 1024 * 1024 } = options;
    await rgtp.initialize();
    return rgtp.receiveFile(host, port, filename, chunkSize);
};

module.exports = {
    RGTP,
    rgtp,
    sendFile,
    receiveFile,
    initialize: () => rgtp.initialize(),
    cleanup: () => rgtp.cleanup(),
    version: () => rgtp.version()
};