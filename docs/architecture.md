# RGTP Architecture

**Version:** 1.0  
**Date:** May 26, 2026

This document describes the architecture and data flow of the Red Giant Transport Protocol (RGTP) using Mermaid diagrams.

---

## 1. Layered Architecture

The library is organized into seven horizontal layers. Each layer depends only on layers below it.

```mermaid
graph TB
    subgraph APP["Application Layer"]
        A1[C Application]
        A2[Go Application]
        A3[Node.js Application]
        A4[Python Application]
    end

    subgraph BIND["Language Bindings"]
        B1[Go CGo\nbindings/go/]
        B2[Node.js N-API\nbindings/node/]
        B3[Python C Extension\nbindings/python/]
    end

    subgraph API["Public C API — rgtp.h"]
        C1[rgtp_init / rgtp_cleanup]
        C2[rgtp_expose / rgtp_poll]
        C3[rgtp_pull_start / rgtp_pull_next]
        C4[rgtp_get_stats / rgtp_get_latency_stats]
    end

    subgraph TRANSPORT["Transport Layer"]
        T1[Exposer State Machine\nrgtp_exposer.c]
        T2[Puller State Machine\nrgtp_puller.c]
        T3[AIMD Flow Control\nrgtp_flow.c]
        T4[Anti-Replay Window\nrgtp_replay.c]
        T5[Rate Limiter\nrgtp_ratelimit.c]
        T6[Surface Lifecycle\nrgtp_surface.c]
    end

    subgraph WIRE["Wire Protocol Layer"]
        W1[Packet Parser\nrgtp_parser.c]
        W2[Packet Serializer\nrgtp_packet.c]
        W3[Packet Type Definitions\nrgtp_packet_types.h]
    end

    subgraph CRYPTO["Cryptographic Layer"]
        CR1[AEAD Encrypt/Decrypt\nrgtp_crypto.c]
        CR2[Merkle Tree\nrgtp_merkle.c]
        CR3[CSPRNG\nrgtp_csprng.c]
        CR4[Key Zeroization\nrgtp_zeroize.c]
    end

    subgraph FEC["FEC Layer"]
        F1[GF 2^8 Tables\nrgtp_gf256.c]
        F2[RS Encoder\nrgtp_rs_encode.c]
        F3[RS Decoder\nrgtp_rs_decode.c]
        F4[SIMD Acceleration\nrgtp_rs_simd.c]
    end

    subgraph IO["I/O Layer"]
        I1[Socket UDP/Raw Eth\nrgtp_socket.c]
        I2[sendmmsg/recvmmsg\nrgtp_sendmmsg.c]
        I3[io_uring Linux 5.1+\nrgtp_iouring.c]
        I4[IOCP Windows\nrgtp_iocp.c]
        I5[AF_PACKET Raw Ethernet\nrgtp_raweth.c]
    end

    subgraph OBS["Observability"]
        O1[Prometheus Metrics\nrgtp_metrics.c]
        O2[OpenTelemetry Spans\nrgtp_otel.c]
        O3[Structured Logging\nrgtp_log.c]
    end

    subgraph MW["Automotive Middleware"]
        M1[ROS2 rmw Plugin\nrgtp_ros2.c]
        M2[DDS/RTPS Adapter\nrgtp_dds.c]
        M3[SOME/IP Adapter\nrgtp_someip.c]
    end

    A1 --> API
    A2 --> B1 --> API
    A3 --> B2 --> API
    A4 --> B3 --> API

    API --> TRANSPORT
    TRANSPORT --> WIRE
    TRANSPORT --> CRYPTO
    TRANSPORT --> FEC
    TRANSPORT --> OBS
    WIRE --> IO
    MW --> API
```

---

## 2. Exposer Data Flow

Shows what happens from the moment `rgtp_expose()` is called until a chunk is served to a puller.

```mermaid
sequenceDiagram
    participant APP as Application
    participant EXP as Exposer
    participant CRYPTO as Crypto Layer
    participant MERKLE as Merkle Layer
    participant FEC as FEC Layer
    participant IO as I/O Layer
    participant PULLER as Puller (remote)

    APP->>EXP: rgtp_expose(data, size, cfg)
    EXP->>CRYPTO: CSPRNG → generate exposure_id[16]
    EXP->>CRYPTO: CSPRNG → generate key[32]
    loop For each chunk i
        EXP->>CRYPTO: rgtp_encrypt_chunk(key, i, plaintext)
        CRYPTO-->>EXP: ciphertext[i]
    end
    EXP->>MERKLE: rgtp_merkle_build(plaintexts, chunk_count)
    MERKLE-->>EXP: merkle_root[32], proofs[]
    opt FEC enabled
        EXP->>FEC: rgtp_rs_encode(k, n, data_symbols)
        FEC-->>EXP: parity_chunks[]
    end
    EXP-->>APP: surface* (ACTIVE)

    loop Poll loop
        APP->>EXP: rgtp_poll(surface, timeout_ms)
        IO-->>EXP: recvmmsg → Pull_Request packet
        EXP->>EXP: rate_limit_check(source_ip)
        EXP->>EXP: parse_packet → validate chunk_index
        EXP->>IO: sendmmsg → Chunk_Data[+Merkle_Proof]
        IO-->>PULLER: UDP / Raw Ethernet frame
    end
```

---

## 3. Puller Data Flow

Shows the full pull lifecycle from connection to chunk delivery.

```mermaid
sequenceDiagram
    participant APP as Application
    participant PULL as Puller
    participant IO as I/O Layer
    participant CRYPTO as Crypto Layer
    participant MERKLE as Merkle Layer
    participant FEC as FEC Layer
    participant EXP as Exposer (remote)

    APP->>PULL: rgtp_pull_start(sock, server, exposure_id, cfg)
    PULL->>IO: send Pull_Request(exposure_id, window_size, version_range)
    IO-->>EXP: UDP packet
    EXP-->>IO: Manifest(chunk_count, chunk_size, merkle_root, fec_params)
    IO-->>PULL: recv Manifest
    PULL->>PULL: store merkle_root, chunk_count, fec_params
    PULL-->>APP: surface* (ACTIVE)

    loop Until all chunks received
        APP->>PULL: rgtp_pull_next(surface, buf, buf_size)
        PULL->>IO: send Pull_Request(chunk_index, window)
        IO-->>EXP: UDP packet
        EXP-->>IO: Chunk_Data[_With_Proof](chunk_index, ciphertext)
        IO-->>PULL: recv chunk packet

        PULL->>PULL: anti_replay_check(seq)
        PULL->>CRYPTO: rgtp_decrypt_chunk(key, chunk_index, ciphertext)
        CRYPTO-->>PULL: plaintext (or RGTP_ERR_AUTH_FAIL)
        opt Merkle proof present
            PULL->>MERKLE: rgtp_merkle_verify(plaintext, proof, index, root)
            MERKLE-->>PULL: OK or RGTP_ERR_MERKLE_FAIL
        end
        PULL->>PULL: update recv_bitmap, RTT EWMA
        PULL->>IO: send Rate_Report(rtt_us, loss_rate)
        PULL-->>APP: plaintext chunk + chunk_index
    end
```

---

## 4. Packet Flow and Wire Protocol

Shows all 8 packet types and their direction between exposer and puller.

```mermaid
flowchart LR
    subgraph PULLER["Puller"]
        P_SEND["Sends"]
        P_RECV["Receives"]
    end

    subgraph WIRE["Wire — UDP or Raw Ethernet"]
        PKT1["0x01 Pull_Request\n36 bytes"]
        PKT2["0x02 Manifest\n72 bytes"]
        PKT3["0x03 Chunk_Data\nvariable"]
        PKT4["0x04 Chunk_Data_With_Proof\nvariable"]
        PKT5["0x05 FEC_Parity\nvariable"]
        PKT6["0x06 NAK\nvariable"]
        PKT7["0x07 Rate_Report\n44 bytes"]
        PKT8["0x08 Keepalive\n32 bytes"]
    end

    subgraph EXPOSER["Exposer"]
        E_RECV["Receives"]
        E_SEND["Sends"]
    end

    P_SEND -->|"Pull_Request"| PKT1 -->|parse| E_RECV
    P_SEND -->|"NAK"| PKT6 -->|parse| E_RECV
    P_SEND -->|"Rate_Report"| PKT7 -->|parse| E_RECV
    P_SEND -->|"Keepalive"| PKT8 -->|parse| E_RECV

    E_SEND -->|"Manifest"| PKT2 -->|parse| P_RECV
    E_SEND -->|"Chunk_Data"| PKT3 -->|parse| P_RECV
    E_SEND -->|"Chunk_Data_With_Proof"| PKT4 -->|parse| P_RECV
    E_SEND -->|"FEC_Parity"| PKT5 -->|parse| P_RECV
```

---

## 5. Cryptographic Pipeline

Shows how a single chunk moves through the crypto subsystem on both sides.

```mermaid
flowchart TD
    subgraph EXPOSE_TIME["At rgtp_expose time — Exposer"]
        PT[Plaintext Chunk i]
        NONCE["Nonce = chunk_index_LE ++ 0x00×8"]
        AEAD_ENC["AEAD Encrypt\nChaCha20-Poly1305-IETF\nor AES-256-GCM"]
        CT["Ciphertext + 16-byte Tag"]
        LEAF["Leaf Hash\nH 0x00 ∥ plaintext"]
        TREE["Merkle Tree\nH 0x01 ∥ left ∥ right"]
        ROOT["Merkle Root 32 bytes"]
        PROOF["Pre-computed Proof\nlog2 N sibling hashes"]

        PT --> NONCE --> AEAD_ENC --> CT
        PT --> LEAF --> TREE --> ROOT
        TREE --> PROOF
    end

    subgraph PULL_TIME["At rgtp_pull_next time — Puller"]
        CT2["Received Ciphertext + Tag"]
        AEAD_DEC["AEAD Decrypt\nverify tag FIRST"]
        PT2["Plaintext Chunk i"]
        VERIFY["Merkle Verify\nrecompute path to root"]
        ACCEPT["Deliver to Application"]
        REJECT["Discard + increment\nauth_failures counter"]

        CT2 --> AEAD_DEC
        AEAD_DEC -->|"tag OK"| PT2 --> VERIFY
        AEAD_DEC -->|"tag FAIL"| REJECT
        VERIFY -->|"proof OK"| ACCEPT
        VERIFY -->|"proof FAIL"| REJECT
    end

    CT -.->|"transmitted over wire"| CT2
```

---

## 6. Flow and Congestion Control

Shows the AIMD feedback loop between puller and exposer.

```mermaid
flowchart TD
    subgraph PULLER["Puller — AIMD Controller"]
        WINDOW["Pull Window\nwindow_size chunks"]
        RTT["RTT EWMA\nα = 0.125"]
        LOSS["Loss Rate EWMA"]
        AIMD_DEC["Congestion Detected\nRTT > 2× baseline\n→ window ×= 0.5"]
        AIMD_INC["Congestion Cleared\nRTT ≤ 1.1× baseline\nfor 5 RTT periods\n→ window += 1"]
        RATE_RPT["Rate_Report\nRTT_Us + Loss_Rate_Q16"]
    end

    subgraph EXPOSER["Exposer — Adaptive FEC"]
        PULL_PRESSURE["Pull Pressure\nrequests in last 100ms"]
        FEC_ADJ["Adaptive FEC\nloss > 5% → overhead +10%\nloss < 1% → overhead -5%\ncap 0%–50%"]
    end

    WINDOW -->|"issues pull requests"| EXPOSER
    EXPOSER -->|"serves chunks"| RTT
    RTT --> AIMD_DEC
    RTT --> AIMD_INC
    AIMD_DEC --> WINDOW
    AIMD_INC --> WINDOW
    LOSS --> RATE_RPT --> FEC_ADJ
    PULL_PRESSURE --> FEC_ADJ
```

---

## 7. I/O Backend Selection

Shows how the I/O backend is chosen at socket creation time.

```mermaid
flowchart TD
    CREATE["rgtp_socket_create cfg"]

    CREATE --> Q1{raw_ethernet?}
    Q1 -->|"Yes + Linux"| RAWETH["AF_PACKET SOCK_DGRAM\nEtherType 0x88B5\n802.1Q VLAN + PCP TSN"]
    Q1 -->|"Yes + Windows"| WINPCAP["WinPcap / Npcap\nor RGTP_ERR_NOT_SUPPORTED"]
    Q1 -->|No| Q2{Platform?}

    Q2 -->|"Linux kernel ≥ 5.1\n+ RGTP_ENABLE_IOURING"| IOURING["io_uring\nSQ depth ≥ 256\nFixed buffer registration"]
    Q2 -->|"Linux kernel ≥ 4.14"| SMMSG["sendmmsg / recvmmsg\nbatch size 64\nUDP_SEGMENT GSO\nMSG_ZEROCOPY"]
    Q2 -->|Windows| IOCP["IOCP\nWSARecvFrom / WSASendTo\nOverlapped I/O"]
    Q2 -->|"Other / Fallback"| BASIC["sendto / recvfrom\nBlocking fallback"]
```

---

## 8. FEC Encode / Decode Pipeline

Shows how Reed-Solomon FEC protects a block of chunks.

```mermaid
flowchart LR
    subgraph ENCODE["Exposer — Encode"]
        D["k Data Chunks\ne.g. k=223"]
        GEN["Generator Polynomial\ng x = ∏ x − αⁱ\nover GF 2^8 poly 0x11D"]
        ENC["RS Encoder\nPolynomial Division\nSIMD AVX2/SSE4.2/NEON"]
        PAR["n−k Parity Chunks\ne.g. 32 parity symbols"]
        TX["Transmit n=255 chunks\ndata + parity"]

        D --> GEN --> ENC --> PAR
        D --> TX
        PAR --> TX
    end

    subgraph NETWORK["Network — Packet Loss"]
        LOSS["Up to n−k chunks lost\ne.g. up to 32 lost"]
    end

    subgraph DECODE["Puller — Decode"]
        RX["Receive ≥ k chunks\nany k of n"]
        BM["Berlekamp-Massey\nError Locator Polynomial"]
        FORNEY["Forney Algorithm\nError Values"]
        GAUSS["Gaussian Elimination\nErasure-only path"]
        RECOVER["Recovered k Data Chunks"]

        RX --> BM --> FORNEY --> RECOVER
        RX --> GAUSS --> RECOVER
    end

    TX --> LOSS --> RX
```

---

## 9. Automotive Middleware Integration

Shows how RGTP integrates with AV middleware stacks.

```mermaid
flowchart TD
    subgraph AV["Autonomous Vehicle Software Stack"]
        ROS2_APP["ROS2 Node\npublish / subscribe"]
        DDS_APP["DDS Participant\nRTPS discovery"]
        SOMEIP_APP["SOME/IP Client\nservice discovery"]
    end

    subgraph MW["RGTP Middleware Adapters"]
        ROS2_PLUG["librgtp_ros2.so\nrmw interface\nCDR serialization"]
        DDS_PLUG["librgtp_dds.so\nRTPS over RGTP"]
        SOMEIP_PLUG["librgtp_someip.so\nservice → Exposure mapping"]
    end

    subgraph RGTP["RGTP Core — librgtp.so"]
        EXPOSE["rgtp_expose\nPre-encrypt + Merkle"]
        POLL["rgtp_poll\nServe pull requests"]
        PULL["rgtp_pull_start\nrgtp_pull_next"]
    end

    subgraph NET["In-Vehicle Network"]
        ETH["Raw Ethernet\nAF_PACKET EtherType 0x88B5"]
        TSN["TSN 802.1Q\nVLAN + PCP\nCBS / TAS shaping"]
    end

    ROS2_APP --> ROS2_PLUG --> EXPOSE
    DDS_APP --> DDS_PLUG --> EXPOSE
    SOMEIP_APP --> SOMEIP_PLUG --> EXPOSE
    EXPOSE --> POLL --> ETH --> TSN
    TSN --> PULL
```

---

## 10. Observability Pipeline

Shows how metrics, traces, and logs flow out of the library.

```mermaid
flowchart LR
    subgraph RGTP["RGTP Internal Events"]
        EV1["Chunk encrypted"]
        EV2["Pull request received"]
        EV3["Auth failure"]
        EV4["FEC decode"]
        EV5["Surface destroyed"]
    end

    subgraph OBS["Observability Layer"]
        LOG["rgtp_log.c\nStructured Log Events\ntimestamp_ns + KV pairs"]
        METRICS["rgtp_metrics.c\nPrometheus Registry\ncounters / gauges / histograms"]
        OTEL["rgtp_otel.c\nOpenTelemetry Spans\ntrace per operation"]
    end

    subgraph SINK["External Sinks"]
        STDERR["stderr callback\ndefault"]
        PROM["Prometheus\nPush Gateway"]
        JAEGER["Jaeger / Tempo\nOTLP exporter"]
        CUSTOM["Custom callback\nrgtp_set_log_callback"]
    end

    EV1 & EV2 & EV3 & EV4 & EV5 --> LOG
    EV1 & EV2 & EV3 & EV4 & EV5 --> METRICS
    EV1 & EV2 & EV4 & EV5 --> OTEL

    LOG --> STDERR
    LOG --> CUSTOM
    METRICS --> PROM
    OTEL --> JAEGER
```

---

## 11. State Machines

### Exposer State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> ACTIVE : rgtp_expose()\npre-encrypt all chunks\nbuild Merkle tree\ncompute FEC parity
    ACTIVE --> ACTIVE : rgtp_poll()\nreceive Pull_Request\nrate-limit check\nserve chunk from store
    ACTIVE --> DESTROYED : rgtp_destroy_surface()\nzeroize key[32]\nfree all heap
    DESTROYED --> [*]
```

### Puller State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> HANDSHAKE : rgtp_pull_start()\nsend Pull_Request\nwait for Manifest
    HANDSHAKE --> ACTIVE : receive Manifest\nstore merkle_root\nchunk_count, fec_params
    HANDSHAKE --> IDLE : timeout\nRGTP_ERR_TIMEOUT
    ACTIVE --> ACTIVE : rgtp_pull_next()\nrequest chunks via sliding window\ndecrypt + verify\nupdate RTT EWMA\nsend Rate_Report
    ACTIVE --> COMPLETE : all chunks received\nrgtp_progress == 1.0
    ACTIVE --> ACTIVE : NAK issued\nchunk not arrived within 2× interval
    COMPLETE --> DESTROYED : rgtp_destroy_surface()
    ACTIVE --> DESTROYED : rgtp_destroy_surface()
    DESTROYED --> [*]
```

---

## 12. Module Dependency Graph

```mermaid
graph LR
    CORE["core/\nrgtp_init\nrgtp_error\nrgtp_alloc"]

    CRYPTO["crypto/\nrgtp_crypto\nrgtp_csprng\nrgtp_zeroize"]

    MERKLE["merkle/\nrgtp_merkle\nrgtp_merkle_hash"]

    FEC["fec/\nrgtp_gf256\nrgtp_rs_encode\nrgtp_rs_decode\nrgtp_rs_simd"]

    WIRE["wire/\nrgtp_packet\nrgtp_parser"]

    TRANSPORT["transport/\nrgtp_surface\nrgtp_exposer\nrgtp_puller\nrgtp_flow\nrgtp_replay\nrgtp_ratelimit"]

    IO["io/\nrgtp_socket\nrgtp_sendmmsg\nrgtp_iouring\nrgtp_iocp\nrgtp_raweth"]

    OBS["observability/\nrgtp_metrics\nrgtp_otel\nrgtp_log"]

    MW["middleware/\nrgtp_ros2\nrgtp_dds\nrgtp_someip"]

    BIND["bindings/\nnode go python"]

    CRYPTO --> CORE
    MERKLE --> CRYPTO
    FEC --> CORE
    WIRE --> CORE
    IO --> CORE
    TRANSPORT --> CRYPTO
    TRANSPORT --> MERKLE
    TRANSPORT --> FEC
    TRANSPORT --> WIRE
    TRANSPORT --> IO
    TRANSPORT --> OBS
    OBS --> CORE
    MW --> TRANSPORT
    BIND --> TRANSPORT
```
