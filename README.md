# AudioBus

**Multi-channel audio + IO protocol for ESP32-P4**

Two transport modes, one API:

| | Ethernet Mode | LVDS Bus Mode |
|---|---|---|
| **Topology** | Star (any switch) | Daisy chain (single pair) |
| **Setup** | Plug into switch, done | Wire SN65LVDT41 transceiver |
| **Discovery** | Automatic (multicast) | Master-initiated |
| **Master/Slave** | None (peer-to-peer) | Master + slaves |
| **Clock sync** | PTP (HW timestamped) | CDR / software PLL |
| **Channels** | 64/stream @ 48kHz | 25/dir (1-chip) or 64 (2-chip) |
| **Latency** | ~2ms (configurable) | ~21µs per hop |
| **Tunneling** | GPIO + MIDI + SPI + I2C | GPIO + MIDI + SPI + I2C |
| **Diagnostics** | Per-stream jitter, ping RTT | Per-node roundtrip |
| **IP coexistence** | Yes (same port) | N/A (dedicated wire) |

Built on commodity components — no proprietary silicon required.

```
┌──────────┐  1 twisted  ┌──────────┐  1 twisted  ┌──────────┐  1 twisted  ┌──────────┐
│  MASTER  │────pair────→│  SLAVE 1 │────pair────→│  SLAVE 2 │────pair────→│  SLAVE 3 │
│ ESP32-P4 │←───────────│ speaker  │←───────────│ spk+mic  │←───────────│ analog   │
│          │  98 Mbps    │  node    │  98 Mbps    │  node    │  98 Mbps    │  bridge  │
│ EMAC free│  half-dpx   │ EMAC free│             │ EMAC free│             │ EMAC free│
└──────────┘             └──────────┘             └──────────┘             └──────────┘
     ↕                                                                        (end)
  Ethernet              1 IC per port (SN65LVDT41, ~$2)
  or WiFi               Only 5 GPIO pins for the bus!
  (independent)         Single Cat5 pair per link
```

## Two PHY Options

| | SN65LVDT41 (Recommended) | DS92LV1021A + DS92LV1212A |
|---|---|---|
| **Chips per port** | **1** | 2 (serializer + deserializer) |
| **Cost per port** | **~$2** | ~$7 |
| **GPIO pins** | **5** | 24 |
| **Line rate** | 98.3 Mbps | 491.5 Mbps |
| **Channels/dir @ 48kHz** | **25** (32-bit) / **34** (24-bit) | **64** (32-bit) |
| **Channels/dir @ 96kHz** | **12** (32-bit) | **62** (32-bit) |
| **Clock recovery** | Software PLL (Si5351A) | Hardware CDR (in deserializer) |
| **Best for** | Most nodes (2-16 ch) | High-density (32-64 ch) |

The single-chip approach is recommended for most use cases. Speakers, microphones, small mixers, and analog bridges rarely need more than 8 channels per direction.

## Key Features

| Feature | Specification |
|---------|---------------|
| Audio channels | **25/dir** (1-chip) or **64/dir** (2-chip) at 48 kHz/32-bit |
| Sample rates | 44.1, 48, 88.2, 96 kHz |
| Bit depth | 16, 24, or 32 bit (configurable per bus) |
| Wire | **Single twisted pair** per link (Cat5 pair, STP, etc.) |
| Topology | **Daisy chain**, up to 16 nodes |
| Duplex | Half-duplex with direction switching (~650 ns guard) |
| Line rate | 98.3 Mbps (1-chip) or 491.5 Mbps (2-chip) |
| Encoding | 8b10b (DC-balanced, self-clocking, error detection) |
| Latency | ~21 µs per hop (1 sample @ 48 kHz) |
| Jitter | Deterministic — fixed slot map, no per-frame allocation |
| Hot-plug | Frame timeout / CDR lock detection |
| Tunneling | SPI, I2C, GPIO (16 pins/node), MIDI |
| Sideband | 1 byte/frame/direction (MIDI clock, sync triggers) |
| ESP32 EMAC | **Free** — use Ethernet/WiFi independently |
| Cost per node | **~$8** (master, 1-chip) or ~$14 (master, 2-chip) |

## How It Works

### The Big Picture

AudioBus multiplexes audio and control data into **TDM frames** transmitted over LVDS. One frame is sent per audio sample period (every 20.83 µs at 48 kHz). The frame is split between downstream (master→slaves) and upstream (slaves→master) data:

```
                    One frame period (20.83 µs at 48 kHz)
                    ◄────────────────────────────────────►

 ┌──────┬────────┬───────────────────┬──────────┬───────┬───────────────────┬──────────┬──────┐
 │ SYNC │ HEADER │  DOWNSTREAM AUDIO │  DN AUX  │ GUARD │   UPSTREAM AUDIO  │  UP AUX  │ CRC  │
 │  2B  │   8B   │   (up to 256B)    │ (tunnel) │  2B   │   (up to 256B)    │ (tunnel) │  4B  │
 └──────┴────────┴───────────────────┴──────────┴───────┴───────────────────┴──────────┘──────┘
 ◄─K28.5──K28.1──►                              ◄K28.5─►
   comma   SOF                                  K27.7
                                               turnaround
```

**Frame sizes:**
- **1024 bytes** at 48/44.1 kHz (1024 symbols at 49.152 MHz)
- **512 bytes** at 96/88.2 kHz

### Channel Capacity

| Sample Rate | Bit Depth | Max Channels/Direction | Aux Bandwidth |
|-------------|-----------|----------------------|---------------|
| 48 kHz | 32-bit | **64** | 246 bytes/frame |
| 48 kHz | 24-bit | **64** | 310 bytes/frame |
| 96 kHz | 32-bit | **62** | ~0 bytes |
| 96 kHz | 24-bit | **64** | 54 bytes/frame |

### Deterministic Slot Map

At configuration time, the master computes a **slot map** — a fixed assignment of byte offsets within the frame for every audio channel and tunnel stream. This slot map is distributed to all nodes and **never changes during operation**.

```
Slot Map (computed once at config time):
┌─────────────────────────────────────────────────────────────────┐
│ Channel 0 (Node 1, DN, 32-bit) → frame bytes [10..13]          │
│ Channel 1 (Node 1, DN, 32-bit) → frame bytes [14..17]          │
│ Channel 2 (Node 2, DN, 32-bit) → frame bytes [18..21]          │
│ ...                                                             │
│ GPIO tunnel (Node 1, DN)       → frame bytes [266..267]         │
│ MIDI tunnel (Node 2, DN)       → frame bytes [268..270]         │
│ ─── GUARD (direction turnaround) ───                            │
│ Channel 64 (Node 1, UP, 32-bit) → frame bytes [514..517]       │
│ ...                                                             │
└─────────────────────────────────────────────────────────────────┘
```

Every frame uses identical byte positions. Packing and unpacking is a simple `memcpy` per slot — no branching, no allocation, no jitter.

### Physical Layer — Single-Chip LVDS Transceiver (Recommended)

The default PHY uses the **TI SN65LVDT41** — a single LVDS transceiver with both driver and receiver on one differential pair. Half-duplex via the DE pin. **Only 1 chip and 5 GPIO pins per port.**

```
  ESP32-P4                    SN65LVDT41                Twisted Pair
 ┌─────────┐                ┌─────────────┐           ╔═══════════╗
 │         │                │             │           ║           ║
 │ PARLIO  │  1-bit @       │  D ────→ Y+ ┼──────────║  Single   ║
 │ TX out  │──98.304MHz───→│  (driver)    │           ║  Twisted  ║
 │         │                │         Y- ─┼──────────║  Pair     ║
 │ PARLIO  │  1-bit         │  R ←─── A ──┼──────────║           ║
 │ RX in   │←──────────────│  (receiver)  │           ║  Cat5     ║
 │         │                │         B ──┼──────────║  or STP   ║
 │ GPIO ───┼──→ DE          │ HIGH=TX     │   100Ω   ║  up to    ║
 │         │   LOW=RX       │ LOW=RX      │   term.  ║  ~20m     ║
 └─────────┘                └─────────────┘           ╚═══════════╝

  Total: 5 GPIO + 1 chip per port. That's it!
```

**Why this chip?**
- **Single IC**: Driver + receiver in one package, no serializer/deserializer pair needed.
- **Dirt cheap**: ~$2 per port (vs ~$7 for the 2-chip SerDes).
- **Minimal pins**: Data TX, data RX, DE, clock out, ext clock in = **5 GPIO total**.
- **Fast**: Rated 400 Mbps. We run 98.3 Mbps — well within spec.
- **Half-duplex**: DE pin controls direction. The PARLIO serializes 8b10b-encoded data 1 bit at a time.

### High-Performance Option — 10:1 LVDS SerDes (64 channels)

For applications needing 32-64 channels per direction, the 2-chip option uses **DS92LV1021A** (serializer) + **DS92LV1212A** (deserializer with hardware CDR):
- PARLIO 16-bit mode at 49.152 MHz → 491 Mbps line rate
- 64 channels/direction at 48 kHz/32-bit
- True hardware self-clocking via CDR PLL
- 24 GPIO pins per port, ~$7/port
- See [HARDWARE.md](docs/HARDWARE.md) for full wiring details.

### Self-Clocking via 8b10b + CDR

Every data byte is encoded into a 10-bit symbol using the **8b10b line code**:
- Guarantees a maximum run length of 5 identical bits → frequent transitions for CDR
- DC-balanced → no baseline wander on the twisted pair
- Special K-characters (comma, SOF, turnaround) for frame synchronization
- Built-in error detection (disparity violations)

The DS92LV1212A CDR PLL locks onto these transitions and outputs a recovered clock (RCLK) at exactly the transmitter's frequency. This RCLK is used as the PARLIO external clock input on the receiver side.

### Clock Distribution Through the Daisy Chain

```
  MASTER                      SLAVE 1                     SLAVE 2
┌───────────┐              ┌───────────┐              ┌───────────┐
│           │              │           │              │           │
│  49.152   │   LVDS #1    │  DS92LV   │   LVDS #2    │  DS92LV   │
│  MHz XO ──┼──→ SER ──────┼→ DESER   │──→ SER ──────┼→ DESER    │
│           │              │  CDR PLL  │              │  CDR PLL  │
│           │              │    ↓      │              │    ↓      │
│           │              │  RCLK ────┼──→ PARLIO    │  RCLK ────┼──→ PARLIO
│           │              │   (exact  │    TX clk    │   (exact  │    TX clk
│           │              │  49.152)  │              │  49.152)  │
│           │              │    ↓      │              │    ↓      │
│           │              │  I2S MCLK │              │  I2S MCLK │
│           │              │  (local   │              │  (local   │
│           │              │   codec)  │              │   codec)  │
└───────────┘              └───────────┘              └───────────┘

The master's crystal oscillator frequency propagates through the
entire chain via CDR recovery — every node is phase-locked to the
master. Zero drift, zero jitter accumulation.
```

### Half-Duplex Direction Switching

Both the serializer and deserializer connect to the **same twisted pair**. Direction is controlled by the serializer's PDB (output enable) pin:

```
  NODE A (upstream)                           NODE B (downstream)
┌──────────────────┐                        ┌──────────────────┐
│ DS92LV1021A      │                        │ DS92LV1021A      │
│  OUT+/OUT- ──┐   │    ┌──────────────┐    │   ┌── OUT+/OUT-  │
│  PDB=GPIO    ├───┼────┤ Twisted Pair ├────┼───┤  PDB=GPIO    │
│ DS92LV1212A  │   │    └──────────────┘    │   │ DS92LV1212A  │
│  IN+/IN-  ───┘   │         100Ω          │   └── IN+/IN-    │
│  LOCK → GPIO     │      termination       │      LOCK → GPIO │
└──────────────────┘      (each end)        └──────────────────┘

DOWNSTREAM PHASE:                    UPSTREAM PHASE:
  Node A: PDB=HIGH (TX)               Node A: PDB=LOW  (high-Z)
  Node B: PDB=LOW  (high-Z)           Node B: PDB=HIGH (TX)
  Data flows: A → B                   Data flows: B → A
  B's CDR locks to A's data           A's CDR locks to B's data
```

The guard time between direction switches is 32 idle symbols (~650 ns) — enough for the CDR PLL to reacquire phase lock on the new transmitter.

### Daisy Chain Topology

```
           Link 0                Link 1                Link 2
  Master ◄══════════► Node 1 ◄══════════► Node 2 ◄══════════► Node 3
  (1 port)           (2 ports)           (2 ports)           (1 port)
                   intermediate        intermediate          end-node

Frame propagation (pipelined store-and-forward):

T=0:    Master TX ──→ Node 1 RX
T=1:    Master TX ──→ Node 1 RX,  Node 1 TX ──→ Node 2 RX
T=2:    Master TX ──→ Node 1 RX,  Node 1 TX ──→ Node 2 RX,  Node 2 TX ──→ Node 3 RX
        (upstream returns through same pipeline in reverse)

Latency per hop: ~42 µs (store-and-forward, 2 sample periods at 48 kHz)
Round-trip for 8 nodes: ~336 µs
```

**Node types:**
- **Master**: 1 port (downstream). Generates bus clock from crystal oscillator.
- **End-node**: 1 port (upstream). Speakers, microphones, analog bridges.
- **Intermediate**: 2 ports (upstream + downstream). Repeater/bridge nodes.

Single-port nodes use one PARLIO TX + RX pair. Dual-port intermediate nodes share the PARLIO via ESP32-P4 GPIO matrix switching (or external SN74CB3Q3257 bus MUX).

### Hot-Plug Detection

Each DS92LV1212A has a **LOCK** output pin that goes HIGH when the CDR PLL is locked to incoming data. This is directly wired to an ESP32-P4 GPIO:

```
No node connected:    LOCK = LOW  → link down
Node plugged in:      LOCK = HIGH → link up (CDR locked)
Node unplugged:       LOCK = LOW  → link down event
Cable fault:          LOCK = LOW  → link down event
```

The master periodically polls LOCK status. On topology change, it re-runs discovery and redistributes the slot map.

### Tunneling (SPI, I2C, GPIO, MIDI)

The frame's auxiliary (AUX) region carries non-audio data using a simple packet format:

```
Tunnel packet:  [TYPE:4 | NODE:4] [LENGTH:8] [PAYLOAD: 0..253 bytes]

Types:
  0 = SPI tunnel   (CS + mode + data)
  1 = I2C tunnel   (addr + R/W + data)
  2 = GPIO tunnel  (16-bit pin state, no header needed — fixed 2 bytes/node)
  3 = MIDI tunnel  (1-3 raw MIDI bytes per frame)
  4 = Sideband     (1 byte per direction per frame — e.g., MIDI clock ticks)
```

At 48 kHz with 32 channels of 32-bit audio, **246 bytes per frame** are available for tunneling — enough for substantial SPI/I2C traffic alongside full audio.

## Hardware

### Bill of Materials (per node)

| Node Type | Key Components | Approx. Cost |
|-----------|---------------|-------------|
| **Master** | ESP32-P4 + DS92LV1021A + DS92LV1212A + 49.152 MHz XO | **~$14** |
| **End-node slave** | ESP32-P4 + DS92LV1021A + DS92LV1212A + I2S codec | **~$18** |
| **Intermediate** | Above + 2nd SerDes pair + bus MUX | **~$27** |

### ESP32-P4 Wiring — Single-Chip PHY (SN65LVDT41)

```
                           ESP32-P4
                        ┌────────────────────────┐
  98.304 MHz clock ────→│ GPIO 6   (EXT CLK IN)  │   Master: crystal oscillator
  (or Si5351A CLK0)     │                        │   Slave: Si5351A output
                        │                        │
                        │       AudioBus PHY      │   ← ONLY 5 PINS!
  SN65LVDT41 D     ←───│ GPIO 7   (PARLIO TX)   │   (1-bit serial out)
  SN65LVDT41 R     ───→│ GPIO 8   (PARLIO RX)   │   (1-bit serial in)
  SN65LVDT41 DE    ←───│ GPIO 9   (direction)   │   HIGH=TX, LOW=RX
                        │ GPIO 10  (PARLIO CLK)  │
                        │                        │
  Si5351A SDA      ←──→│ GPIO 3   (I2C)         │   Slave only (clock adjust)
  Si5351A SCL      ←───│ GPIO 4   (I2C)         │
                        │                        │
                        │       I2S (local codec) │
  I2S DAC/ADC BCK  ←──→│ GPIO 31                │
  I2S DAC/ADC WS   ←──→│ GPIO 32                │
  I2S DAC DOUT     ←───│ GPIO 33                │
  I2S ADC DIN      ───→│ GPIO 34                │
                        │                        │
                        │       EMAC (FREE!)      │
                        │ GPIO 35-43 → Eth PHY   │   Independent networking
                        │                        │
                        │ GPIO 44+ → UART, SPI,  │
                        │   LEDs, buttons, etc.   │   41+ GPIOs remaining!
                        └────────────────────────┘

GPIO budget: 5 (bus) + 2 (I2C) + 4 (I2S) + 9 (Ethernet) + 2 (UART) = 22 of 55
Remaining: 33 GPIOs free!
```

Compare with the 2-chip SerDes: 24 GPIO pins for the bus alone. The single-chip approach frees **19 extra GPIOs**.

### Twisted Pair Wiring

```
                Node A                                  Node B
          ┌───────────────┐                       ┌───────────────┐
          │ DS92LV1021A   │                       │ DS92LV1021A   │
          │  OUT+ ────┐   │                       │   ┌──── OUT+  │
          │  OUT- ──┐ │   │     ┌───────────┐     │   │ ┌── OUT-  │
          │         │ │   │     │           │     │   │ │         │
          │         ├─┼───┼──T──┤  Twisted  ├──T──┼───┼─┤         │
          │         │ │   │  e  │   Pair    │  e  │   │ │         │
          │ DS92LV  │ │   │  r  │           │  r  │   │ │ DS92LV  │
          │ 1212A   │ │   │  m  │  Cat5/STP │  m  │   │ │ 1212A   │
          │  IN+  ──┘ │   │     │  100Ω imp │     │   │ └── IN+   │
          │  IN-  ────┘   │     └───────────┘     │   └──── IN-   │
          └───────────────┘                       └───────────────┘

  "Term" = 100Ω termination resistor across the differential pair, at each end.

  Connector options:
  ┌─────────────┬────────────────────────────────┐
  │ RJ45        │ Pins 1,2 (pair) — standard     │
  │ 3.5mm TRS   │ Tip=+, Ring=-, Sleeve=GND      │
  │ XLR-3       │ Pin 2=+, Pin 3=-, Pin 1=GND    │
  │ Ethercon    │ Ruggedized RJ45 for live sound  │
  │ JST-PH 3pin│ Compact for internal wiring      │
  └─────────────┴────────────────────────────────┘

  Maximum cable length: ~15-20m per segment (Cat5e at 491 Mbps)
```

### Clock Source Options

| Clock | Part Number | Price | Notes |
|-------|-------------|-------|-------|
| 98.304 MHz (2× 48k base) | Custom or Si5351A | $1.50 | Master: crystal osc. Si5351A can generate this from 25 MHz. |
| 49.152 MHz (for 2-chip SerDes) | SiT8008B-49-152 | $1.50 | MEMS, ±25ppm, SOT-23 |
| Dual family | Si5351A-B-GT | $1.50 | I2C programmable, generates any audio clock |

**Single-chip PHY**: Master needs a 98.304 MHz clock source. Slave uses Si5351A (~$1.50) to generate a local clock, adjusted via I2C software PLL locked to master frame timing.

**2-chip SerDes PHY**: Slave nodes recover the clock from the bus via DS92LV1212A CDR — no local oscillator needed.

## Software Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
│   abus_audio_write/read()   abus_tunnel_send()              │
│   abus_sideband_set/get()   abus_get_stats()                │
├─────────────────────────────────────────────────────────────┤
│                     Core Protocol Engine                     │
│   State machine (INIT → DISCOVERY → CONFIG → RUNNING)       │
│   Frame packing/unpacking (deterministic slot map)           │
│   Audio ring buffers (ISR-safe, zero-copy)                   │
│   Discovery + hot-plug management                            │
├─────────────────────────────────────────────────────────────┤
│                     8b10b Codec                              │
│   Lookup-table encoder/decoder                               │
│   K-character framing (comma, SOF, turnaround, EOF)         │
│   Running disparity tracking                                 │
├─────────────────────────────────────────────────────────────┤
│                     PHY Driver (LVDS SerDes)                 │
│   PARLIO TX: 16-bit @ 49.152 MHz, DMA double-buffered      │
│   PARLIO RX: 16-bit, external clock (CDR recovered)         │
│   Direction switching via DS92LV1021A PDB GPIO               │
│   Link detection via DS92LV1212A LOCK GPIO                   │
├─────────────────────────────────────────────────────────────┤
│                     Hardware                                 │
│   DS92LV1021A (serializer) + DS92LV1212A (deserializer)    │
│   100Ω twisted pair, LVDS signaling, 491 Mbps               │
└─────────────────────────────────────────────────────────────┘
```

### State Machine

```
         abus_init()          abus_start()
  RESET ───────────→ INIT ───────────────→ DISCOVERY
                                               │
                              (all nodes found) │
                                               ↓
                     ┌─── ERROR ←── ──── CONFIG
                     │                     │
                     │   (slot map sent    │
                     │    to all nodes)    ↓
                     └────────────── RUNNING ←──→ (hot-plug triggers
                                       ↑            re-discovery)
                                       │
                                  abus_stop()
                                       ↓
                                     RESET
```

## Quick Start

### Prerequisites

- ESP-IDF v5.2+ (for ESP32-P4 PARLIO driver support)
- ESP32-P4 target

### Ethernet Mode — Fastest Way to Test (Recommended)

Plug 2+ Waveshare ESP32-P4-NANO boards into an Ethernet switch. Flash the same firmware on all of them. They auto-discover each other, elect a PTP grandmaster, stream audio, and tunnel GPIO/MIDI — zero configuration.

```bash
cd examples/ethernet_node
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor    # Board A
idf.py -p /dev/ttyUSB1 flash monitor    # Board B (separate terminal)
```

**What you'll see in the logs:**

```
I (1200) abus_test: ========================================
I (1200) abus_test:   AudioBus Ethernet Test
I (1200) abus_test:   Board: P4-Nano-A3F2
I (1200) abus_test: ========================================
I (3200) abus_test: [DISCOVER] Node "P4-Nano-B71E" (uid=0x0000B71E, type=3, streams=1)
I (3200) abus_test: [STREAM] "P4-Nano-B71E Stereo" (id=0xB710, 2ch/32bit/48000Hz)
I (3200) abus_test: [SUBSCRIBE] Listening to "P4-Nano-B71E Stereo"
I (6200) abus_test: --- P4-Nano-A3F2 stats ---
I (6200) abus_test:   PTP: GRANDMASTER  offset=0ns  delay=0ns  syncs=24
I (6200) abus_test:   Stream 0xB710 latency:
I (6200) abus_test:     Total: 3000us  Network: 42us  Buffer: 1958us
I (6200) abus_test:     Jitter: RMS=1250ns  Peak=4200ns  [800..4200]ns
I (6200) abus_test:     Packets: rx=3000  lost=0  late=0  loss=0.0000%
I (6200) abus_test:   Ping "P4-Nano-B71E" (0x0000B71E):
I (6200) abus_test:     RTT: 85us  avg=82us  [78..92]us  jitter=3us
I (8200) abus_test: [GPIO TX] Pin 0 → HIGH on node 0x0000B71E
I (8200) abus_test: [MIDI TX] Note On: note=60 vel=100 → 0x0000B71E
```

### What the Test Does

Each board automatically:

1. **Publishes** a named stereo stream ("P4-Nano-XXXX Stereo", 440Hz sine wave)
2. **Subscribes** to the first remote stream it discovers
3. **Elects** a PTP grandmaster (lowest MAC address wins)
4. **Measures** and prints every 3 seconds:
   - Per-stream: total latency, network latency, buffer depth, jitter (RMS/peak), packet loss
   - Per-node: active ping roundtrip (avg/min/max), PTP one-way, jitter
   - PTP state: grandmaster or slave, clock offset, path delay
5. **Tunnels** GPIO (toggles pin 0 on remote) and MIDI (note-on) every 2 seconds

### Minimal Ethernet Node Code

```c
#include "audiobus_net.h"

void app_main(void) {
    /* 1. Initialize Ethernet (board-specific) */
    esp_eth_handle_t eth = init_ethernet();

    /* 2. Create AudioBus network transport — no master/slave, just plug in */
    abus_net_config_t config = {
        .name = "My Node",
        .default_sample_rate = 48000,
        .default_bit_depth = 32,
        .default_packet_interval_us = 1000,  /* 1ms packets */
        .presentation_latency_us = 2000,     /* 2ms playout buffer */
    };
    abus_net_handle_t net;
    abus_net_init(&config, &net);
    abus_net_attach_eth(net, eth);
    abus_net_start(net);

    /* 3. Publish a stream (any node can be a talker) */
    uint16_t stream_id;
    abus_net_stream_create(net, "Main Out", 2, 48000, 32, 0, &stream_id);

    /* 4. Subscribe to a remote stream (by talker UID + stream ID) */
    abus_net_subscribe(net, remote_uid, remote_stream_id, 0);

    /* 5. Read/write audio */
    int32_t samples[2];
    while (1) {
        abus_net_stream_write(net, stream_id, samples, 1);   /* Talker */
        abus_net_stream_read(net, remote_stream_id, samples, 1); /* Listener */
    }
}
```

### Bus Mode (LVDS Twisted Pair)

For the single-pair LVDS bus mode with SN65LVDT41 transceiver:

```bash
cd examples/master_node    # or slave_node
idf.py set-target esp32p4
idf.py build flash monitor
```

See [HARDWARE.md](docs/HARDWARE.md) for LVDS wiring details.

## Project Structure

```
audiobus/
├── components/audiobus/
│   ├── include/
│   │   ├── audiobus.h              # Bus mode public API (LVDS)
│   │   ├── audiobus_net.h          # Ethernet mode public API
│   │   ├── audiobus_types.h        # Protocol types, frame format, constants
│   │   ├── audiobus_phy.h          # PHY abstraction (vtable)
│   │   └── audiobus_tunnel.h       # Tunnel subsystem API (bus mode)
│   ├── src/
│   │   ├── audiobus.c              # Bus mode engine, state machine, ring buffers
│   │   ├── audiobus_frame.c        # Frame packing, slot map computation
│   │   ├── audiobus_discovery.c    # Node discovery and hot-plug (bus mode)
│   │   ├── audiobus_tunnel.c       # SPI/I2C/GPIO/MIDI tunnel (bus mode)
│   │   ├── phy/
│   │   │   ├── phy_lvds_oneic.c    # SN65LVDT41 single-chip PHY (recommended)
│   │   │   └── phy_lvds_serdes.c   # DS92LV 10:1 SerDes PHY (max performance)
│   │   ├── net/
│   │   │   ├── abus_net.c          # Ethernet transport engine
│   │   │   └── abus_ptp.c          # PTP clock sync with HW timestamping
│   │   └── codec/
│   │       └── codec_8b10b.c       # 8b10b encoder/decoder
│   ├── CMakeLists.txt
│   └── Kconfig
├── examples/
│   ├── ethernet_node/              # Plug-and-play Ethernet test (P4-NANO)
│   ├── master_node/                # LVDS bus master example
│   └── slave_node/                 # LVDS bus slave example
├── docs/
│   └── HARDWARE.md                 # Schematics, BOM, PCB notes
└── README.md
```

## ESP32-S3 Compatibility

The ESP32-S3 lacks PARLIO and EMAC. It can be used for simpler end-nodes:

| Feature | ESP32-P4 | ESP32-S3 |
|---------|----------|----------|
| PARLIO | Yes (10-bit @ 49 MHz) | No (use SPI or LCD_CAM) |
| Max channels | 64/direction | 8-16/direction |
| EMAC | Yes (free for Ethernet) | No (WiFi only) |
| Role | Master, slave, intermediate | End-node slave only |

## Kconfig Options

```
AudioBus Configuration
├── ABUS_MAX_NODES          (2..16, default 16)
├── ABUS_FRAME_TASK_STACK   (default 4096)
├── ABUS_FRAME_TASK_PRIORITY (default 23)
├── ABUS_AUDIO_BUF_FRAMES   (4..64, default 8)
├── ABUS_GUARD_SYMBOLS       (8..128, default 32)
└── ABUS_SAMPLE_RATE_FAMILY  (48kHz or 44.1kHz)
```

## Comparison with Proprietary Solutions

| Feature | AudioBus | Proprietary Solutions |
|---------|----------|-----------|
| Silicon | Commodity LVDS SerDes | Custom ASIC |
| Cost per node | $14-27 | $5-15 (IC only, no MCU) |
| Max channels | 64/dir @ 48kHz | 16-32/dir |
| Sample rates | 44.1-96 kHz | 44.1-48 kHz (typical) |
| Bit depth | 16/24/32 | 16/24 (typical) |
| Tunneling | SPI + I2C + GPIO + MIDI | GPIO + I2C |
| Networking | Independent Ethernet/WiFi | None |
| Open source | Yes (MIT) | No |
| Toolchain | ESP-IDF (free) | Proprietary IDE |
| Development board | Any ESP32-P4 devkit + breakout | Dedicated eval board |

## License

MIT License. See source files for details.
