/**
 * TypeScript definitions for Red Giant Transport Protocol (RGTP) Node.js bindings
 */

import { EventEmitter } from 'events';

export interface RGTPConfig {
  /** Chunk size in bytes (0 = auto) */
  chunkSize?: number;
  /** Initial exposure rate (chunks/sec) */
  exposureRate?: number;
  /** Enable adaptive rate control */
  adaptiveMode?: boolean;
  /** Port number (0 = auto) */
  port?: number;
  /** Operation timeout in milliseconds */
  timeout?: number;
  /** Progress callback */
  onProgress?: (bytesTransferred: number, totalBytes: number) => void;
  /** Error callback */
  onError?: (errorCode: number, errorMessage: string) => void;
}

export interface RGTPStats {
  /** Bytes successfully transferred */
  bytesTransferred: number;
  /** Total bytes in transfer */
  totalBytes: number;
  /** Current throughput in MB/s */
  throughputMbps: number;
  /** Average throughput in MB/s */
  avgThroughputMbps: number;
  /** Number of chunks transferred */
  chunksTransferred: number;
  /** Total number of chunks */
  totalChunks: number;
  /** Number of retransmissions */
  retransmissions: number;
  /** Completion percentage (0-100) */
  completionPercent: number;
  /** Elapsed time in milliseconds */
  elapsedMs: number;
  /** Estimated remaining time in milliseconds */
  estimatedRemainingMs: number;
  /** Transfer efficiency percentage */
  efficiencyPercent: number;
}

/**
 * RGTP Session for exposing data
 * 
 * A session represents a server-side entity that exposes data for clients
 * to pull. Multiple clients can pull from the same session simultaneously.
 */
export declare class Session extends EventEmitter {
  /**
   * Create a new RGTP session
   */
  constructor(config?: RGTPConfig);

  /**
   * Expose a file for pulling
   */
  exposeFile(filename: string): Promise<void>;

  /**
   * Wait for exposure to complete
   */
  waitComplete(): Promise<void>;

  /**
   * Get current transfer statistics
   */
  getStats(): Promise<RGTPStats>;

  /**
   * Cancel ongoing exposure
   */
  cancel(): Promise<void>;

  /**
   * Close the session and free resources
   */
  close(): void;

  // Events
  on(event: 'progress', listener: (bytesTransferred: number, totalBytes: number) => void): this;
  on(event: 'error', listener: (error: Error) => void): this;
  on(event: 'close', listener: () => void): this;
  
  emit(event: 'progress', bytesTransferred: number, totalBytes: number): boolean;
  emit(event: 'error', error: Error): boolean;
  emit(event: 'close'): boolean;
}

/**
 * RGTP Client for pulling data
 * 
 * A client connects to sessions and pulls exposed data.
 */
export declare class Client extends EventEmitter {
  /**
   * Create a new RGTP client
   */
  constructor(config?: RGTPConfig);

  /**
   * Pull data from remote host and save to file
   */
  pullToFile(host: string, port: number, filename: string): Promise<void>;

  /**
   * Get current transfer statistics
   */
  getStats(): Promise<RGTPStats>;

  /**
   * Cancel ongoing pull operation
   */
  cancel(): Promise<void>;

  /**
   * Close the client and free resources
   */
  close(): void;

  // Events
  on(event: 'progress', listener: (bytesTransferred: number, totalBytes: number) => void): this;
  on(event: 'error', listener: (error: Error) => void): this;
  on(event: 'close', listener: () => void): this;
  
  emit(event: 'progress', bytesTransferred: number, totalBytes: number): boolean;
  emit(event: 'error', error: Error): boolean;
  emit(event: 'close'): boolean;
}

/**
 * Get RGTP library version
 */
export declare function getVersion(): string;

/**
 * Format bytes for display
 */
export declare function formatBytes(bytes: number): string;

/**
 * Format duration for display
 */
export declare function formatDuration(milliseconds: number): string;

/**
 * Send a file using RGTP (convenience function)
 */
export declare function sendFile(filename: string, config?: RGTPConfig): Promise<RGTPStats>;

/**
 * Receive a file using RGTP (convenience function)
 */
export declare function receiveFile(host: string, port: number, filename: string, config?: RGTPConfig): Promise<RGTPStats>;

/**
 * Create configuration optimized for LAN networks
 */
export declare function createLANConfig(): RGTPConfig;

/**
 * Create configuration optimized for WAN networks
 */
export declare function createWANConfig(): RGTPConfig;

/**
 * Create configuration optimized for mobile networks
 */
export declare function createMobileConfig(): RGTPConfig;

/**
 * RGTP library version
 */
export declare const VERSION: string;