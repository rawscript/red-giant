class RGTPEndpoint {
  constructor(url) {
    this.url = url;
    this.transport = null;
    this.stream = null;
    this.currentExposureId = null;
  }

  async connect() {
    if (this.transport) return;
    this.transport = new WebTransport(this.url);
    await this.transport.ready;
    console.log("WebTransport connected over QUIC+HTTP/3");
  }

  async exposeFile(file, onProgress) {
    await this.connect();

    // Step 1: Get exposure ID from server (manifest response)
    const resp = await fetch(this.url + "/manifest", {method: "POST"});
    const {exposureId} = await resp.json();
    this.currentExposureId = exposureId;

    const chunkSize = 64 * 1024; // 64 KB — same as native
    const totalChunks = Math.ceil(file.size / chunkSize);
    let sent = 0;

    // Open bidirectional stream
    const stream = await this.transport.createBidirectionalStream();
    const writer = stream.writable.getWriter();
    const encoder = new TextEncoder();

    // Send header
    const header = new Uint8Array(48);
    const view = new DataView(header.buffer);
    view.setBigUint64(16, BigInt(exposureId[0]), false);
    view.setBigUint64(24, BigInt(exposureId[1]), false);
    view.setBigUint64(32, BigInt(file.size), false);
    view.setUint32(40, totalChunks, false);
    view.setUint32(44, chunkSize, false);
    await writer.write(header);

    // Stream the file in chunks
    for (let i = 0; i < totalChunks; i++) {
      const start = i * chunkSize;
      const blob = file.slice(start, start + chunkSize);
      const chunk = await blob.arrayBuffer();
      await writer.write(new Uint8Array(chunk));
      sent += chunk.byteLength;
      onProgress(sent / file.size);

      // Allow instant resume: store progress in localStorage
      localStorage.setItem(`rgtp_${exposureId}`, sent);
    }

    await writer.close();
    console.log("File fully exposed");
  }
}