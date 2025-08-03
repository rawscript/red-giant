// Red Giant Protocol - JavaScript/Node.js SDK
// High-performance distributed storage and communication client

class RedGiantClient {
    constructor(options = {}) {
        this.baseURL = options.baseURL || 'https://api.redgiant.network';
        this.peerID = options.peerID || this.generatePeerID();
        this.tokens = [];
        this.sessionID = options.sessionID || this.generateSessionID();
    }

    generatePeerID() {
        return 'peer_' + Math.random().toString(36).substr(2, 9);
    }

    generateSessionID() {
        return 'session_' + Math.random().toString(36).substr(2, 9);
    }

    async uploadJSON(data, fileName) {
        // Implementation for uploading JSON data
        const result = await this.upload(JSON.stringify(data), fileName);
        
        this.tokens.push(data);
        this.onToken(data, result);

        return result;
    }

    async upload(data, fileName) {
        // Basic upload implementation
        return { id: 'file_' + Date.now(), fileName, size: data.length };
    }

    onToken(token, result) {
        // Override this method to handle token events
    }

    async streamResponse(response, tokensPerSecond = 50) {
        const tokens = this.tokenize(response);
        const interval = 1000 / tokensPerSecond;

        for (let i = 0; i < tokens.length; i++) {
            await this.streamToken(tokens[i], i);

            if (i < tokens.length - 1) {
                await this.sleep(interval);
            }
        }

        this.onComplete({
            sessionID: this.sessionID,
            totalTokens: tokens.length,
            tokens: this.tokens
        });
    }

    async streamToken(token, index) {
        // Override this method to handle individual token streaming
    }

    onComplete(data) {
        // Override this method to handle completion events
    }

    tokenize(text) {
        // Simple tokenization - replace with proper tokenizer in production
        return text.split(/\s+/).filter(token => token.length > 0);
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    async searchFiles(pattern) {
        // Implementation for searching files
        return [];
    }

    async downloadData(fileId) {
        // Implementation for downloading data
        return new ArrayBuffer(0);
    }
}

class StreamingUpload {
    constructor(client, options = {}) {
        this.client = client;
        this.chunkSize = options.chunkSize || 1024 * 1024; // 1MB chunks
    }
}

class TokenStream {
    constructor(client, options = {}) {
        this.client = client;
        this.sessionID = options.sessionID || client.sessionID;
        this.tokens = [];
        this.onToken = options.onToken || (() => {});
        this.onComplete = options.onComplete || (() => {});
    }
}

// Chat Room Helper
class ChatRoom {
    constructor(client, roomID, username, options = {}) {
        this.client = client;
        this.roomID = roomID;
        this.username = username;
        this.onMessage = options.onMessage || (() => { });
        this.onUserJoin = options.onUserJoin || (() => { });
        this.onUserLeave = options.onUserLeave || (() => { });
        this.pollInterval = options.pollInterval || 1000;
        this.messages = [];
        this.isPolling = false;
    }

    async join() {
        await this.sendSystemMessage(`${this.username} joined the chat`);
        this.startPolling();
    }

    async leave() {
        await this.sendSystemMessage(`${this.username} left the chat`);
        this.stopPolling();
    }

    async sendMessage(content, recipient = null) {
        const message = {
            id: `msg_${Date.now()}_${Math.random()}`,
            from: this.username,
            to: recipient || this.roomID,
            content: content,
            timestamp: new Date().toISOString(),
            type: recipient ? 'private' : 'public'
        };

        const fileName = `chat_${this.roomID}_${message.type}_${Date.now()}.json`;
        await this.client.uploadJSON(message, fileName);

        this.messages.push(message);
        return message;
    }

    async sendSystemMessage(content) {
        const message = {
            id: `sys_${Date.now()}`,
            from: 'system',
            to: this.roomID,
            content: content,
            timestamp: new Date().toISOString(),
            type: 'system'
        };

        const fileName = `chat_${this.roomID}_system_${Date.now()}.json`;
        await this.client.uploadJSON(message, fileName);

        return message;
    }

    startPolling() {
        if (this.isPolling) return;

        this.isPolling = true;
        this.lastCheck = new Date();

        this.pollTimer = setInterval(async () => {
            try {
                await this.pollMessages();
            } catch (error) {
                console.error('Polling error:', error);
            }
        }, this.pollInterval);
    }

    stopPolling() {
        this.isPolling = false;
        if (this.pollTimer) {
            clearInterval(this.pollTimer);
            this.pollTimer = null;
        }
    }

    async pollMessages() {
        const pattern = `chat_${this.roomID}_`;
        const files = await this.client.searchFiles(pattern);

        for (const file of files) {
            const fileDate = new Date(file.uploaded_at);
            if (fileDate > this.lastCheck && file.peer_id !== this.client.peerID) {
                try {
                    const data = await this.client.downloadData(file.id);
                    const message = JSON.parse(new TextDecoder().decode(data));

                    if (message.from !== this.username) {
                        this.messages.push(message);
                        this.onMessage(message);
                    }
                } catch (error) {
                    console.error('Error processing message:', error);
                }
            }
        }

        this.lastCheck = new Date();
    }
}

// IoT Device Helper
class IoTDevice {
    constructor(client, deviceID, options = {}) {
        this.client = client;
        this.deviceID = deviceID;
        this.location = options.location || { latitude: 0, longitude: 0, altitude: 0 };
        this.sensors = options.sensors || ['temperature', 'humidity', 'pressure'];
        this.batchSize = options.batchSize || 10;
        this.interval = options.interval || 5000;
        this.onReading = options.onReading || (() => { });
        this.onBatch = options.onBatch || (() => { });
        this.isStreaming = false;
    }

    startStreaming() {
        if (this.isStreaming) return;

        this.isStreaming = true;
        this.batchCount = 0;

        this.streamTimer = setInterval(async () => {
            try {
                const batch = this.generateBatch();
                await this.uploadBatch(batch);
                this.batchCount++;
                this.onBatch(batch, this.batchCount);
            } catch (error) {
                console.error('Streaming error:', error);
            }
        }, this.interval);
    }

    stopStreaming() {
        this.isStreaming = false;
        if (this.streamTimer) {
            clearInterval(this.streamTimer);
            this.streamTimer = null;
        }
    }

    generateBatch() {
        const readings = [];

        for (let i = 0; i < this.batchSize; i++) {
            const sensorType = this.sensors[Math.floor(Math.random() * this.sensors.length)];

            const reading = {
                device_id: this.deviceID,
                sensor_type: sensorType,
                value: this.generateSensorValue(sensorType),
                unit: this.getSensorUnit(sensorType),
                timestamp: new Date().toISOString(),
                location: this.location,
                metadata: {
                    battery_level: 80 + Math.random() * 20,
                    signal_strength: -60 + Math.random() * 40,
                    firmware: 'v1.2.3'
                }
            };

            readings.push(reading);
            this.onReading(reading);
        }

        return {
            batch_id: `batch_${this.deviceID}_${Date.now()}`,
            device_id: this.deviceID,
            readings: readings,
            timestamp: new Date().toISOString(),
            count: readings.length
        };
    }

    async uploadBatch(batch) {
        const fileName = `iot_batch_${this.deviceID}_${Date.now()}.json`;
        return this.client.uploadJSON(batch, fileName);
    }

    generateSensorValue(sensorType) {
        switch (sensorType) {
            case 'temperature':
                return 20 + Math.random() * 15; // 20-35°C
            case 'humidity':
                return 30 + Math.random() * 40; // 30-70%
            case 'pressure':
                return 1000 + Math.random() * 50; // 1000-1050 hPa
            case 'light':
                return Math.random() * 1000; // 0-1000 lux
            case 'motion':
                return Math.round(Math.random()); // 0 or 1
            default:
                return Math.random() * 100;
        }
    }

    getSensorUnit(sensorType) {
        const units = {
            temperature: '°C',
            humidity: '%',
            pressure: 'hPa',
            light: 'lux',
            motion: 'bool'
        };
        return units[sensorType] || 'unit';
    }
}

// Export for different environments
if (typeof module !== 'undefined' && module.exports) {
    // Node.js
    module.exports = {
        RedGiantClient,
        StreamingUpload,
        TokenStream,
        ChatRoom,
        IoTDevice
    };
} else if (typeof window !== 'undefined') {
    // Browser
    window.RedGiant = {
        Client: RedGiantClient,
        StreamingUpload,
        TokenStream,
        ChatRoom,
        IoTDevice
    };
}