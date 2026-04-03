# AudioBus

**Multi-channel audio + IO multiplexing over a single twisted pair**

An open-source ESP-IDF component that carries up to 64 channels of 32-bit audio, SPI/I2C/GPIO/MIDI tunneling, and a sideband data channel over a single twisted-pair wire. Daisy-chainable, hot-pluggable, self-clocking, deterministic, and zero-jitter.

Built on commodity components — no proprietary silicon required.

```
┌──────────┐  1 twisted  ┌──────────┐  1 twisted  ┌──────────┐  1 twisted  ┌──────────┐
│  MASTER  │────pair────→│  SLAVE 1 │────pair────→│  SLAVE 2 │────pair────→│  SLAVE 3 │
│ ESP32-P4 │←───────────-│ speaker  │←───────────-│ spk+mic  │←────────-───│ analog   │
│          │  491 Mbps   │  node    │  491 Mbps   │  node    │  491 Mbps   │  bridge  │
│ EMAC free│  half-dpx   │ EMAC free│             │ EMAC free│             │ EMAC free│
└──────────┘             └──────────┘             └──────────┘             └──────────┘
     ↕                                                                        (end)
  Ethernet                     Each node: ~$14-18
  or WiFi                      Single Cat5 pair per link
  (independent)                Hot-plug: just plug in a new node
```

## Key Features

| Feature | Specification |
|---------|---------------|
| Audio channels | Up to **64 per direction** (128 total) at 48 kHz/32-bit |
| Sample rates | 44.1, 48, 88.2, 96 kHz |
| Bit depth | 16, 24, or 32 bit (configurable per bus) |
| Wire | **Single twisted pair** per link (Cat5 pair, STP, etc.) |
| Topology | **Daisy chain**, up to 16 nodes |
| Duplex | Half-duplex with direction switching (~650 ns guard) |
| Line rate | **491.52 Mbps** LVDS |
| Encoding | 8b10b (DC-balanced, self-clocking, error detection) |
| Latency | ~21 µs per hop (1 sample @ 48 kHz) |
| Jitter | Deterministic — fixed slot map, no per-frame allocation |
| Hot-plug | CDR lock detection on each port |
| Tunneling | SPI, I2C, GPIO (16 pins/node), MIDI |
| Sideband | 1 byte/frame/direction (MIDI clock, sync triggers) |
| ESP32 EMAC | **Free** — use Ethernet/WiFi independently |
| Cost per node | ~$14 (master), ~$18 (slave with codec) |

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
│ Channel 0 (Node 1, DN, 32-bit) → frame bytes [10..13]           │
│ Channel 1 (Node 1, DN, 32-bit) → frame bytes [14..17]           │
│ Channel 2 (Node 2, DN, 32-bit) → frame bytes [18..21]           │
│ ...                                                             │
│ GPIO tunnel (Node 1, DN)       → frame bytes [266..267]         │
│ MIDI tunnel (Node 2, DN)       → frame bytes [268..270]         │
│ ─── GUARD (direction turnaround) ───                            │
│ Channel 64 (Node 1, UP, 32-bit) → frame bytes [514..517]        │
│ ...                                                             │
└─────────────────────────────────────────────────────────────────┘
```

Every frame uses identical byte positions. Packing and unpacking is a simple `memcpy` per slot — no branching, no allocation, no jitter.

### Physical Layer — Repurposing LVDS Video SerDes

The key hardware trick: we use **TI DS92LV1021A** (10:1 LVDS serializer) and **DS92LV1212A** (1:10 LVDS deserializer with CDR) — chips designed for high-speed video links — as our audio bus transceiver.

```
  ESP32-P4                    DS92LV1021A              Twisted Pair
 ┌─────────┐                ┌─────────────┐           ╔═══════════╗
 │ PARLIO  │  10-bit @      │ 10:1 LVDS   │  491 Mbps ║           ║
 │ TX unit │──49.152 MHz──-→│ Serializer  │──LVDS──--─→║  Single   ║
 │         │  (16-bit bus,  │             │           ║  Twisted  ║
 │         │   bits 0-9)    │ PDB=OE ctrl │←──GPIO    ║  Pair     ║
 │         │                └─────────────┘           ║           ║
 │         │                                          ║  Cat5     ║
 │         │                DS92LV1212A               ║  or STP   ║
 │         │                ┌─────────────┐           ║           ║
 │ PARLIO  │  10-bit +      │ 1:10 LVDS   │  CDR      ║  up to    ║
 │ RX unit │←─recov. clk─--─│ Deserializer│←─LVDS─-─-─║  15-20m   ║
 │         │                │   with CDR  │           ║           ║
 │         │  LOCK status─-─│ PLL locked! │→──GPIO    ╚═══════════╝
 └─────────┘                └─────────────┘
                              
                          RCLK = recovered 49.152 MHz
                          (self-clocking! no clock wire needed)
```

**Why these chips?**
- **Self-clocking**: DS92LV1212A CDR PLL recovers the clock from 8b10b data transitions. No separate clock wire needed on the twisted pair.
- **High bandwidth**: 491 Mbps at 49.152 MHz parallel clock.
- **Low jitter**: LVDS + CDR PLL provides audio-grade recovered clock.
- **Simple interface**: 10-bit parallel maps directly to 8b10b symbol width. ESP32-P4 PARLIO at 16-bit width connects with bits [10:15] unused.
- **Half-duplex**: Serializer has a PDB (power-down/output-enable) pin. Disable it → LVDS output goes high-Z → remote end can now drive the pair.
- **Cheap**: ~$3.50 each, ~$7 per port.

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
│  MHz XO ──┼──→ SER ──────┼→ DESER    │──→ SER ──────┼→ DESER    │
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
│  IN+/IN-  ───┘   │         100Ω           │   └── IN+/IN-    │
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

### ESP32-P4 Wiring (Single-Port Node)

```
                           ESP32-P4
                        ┌─────────────────────────┐
  49.152 MHz XO    ────→│ GPIO 6  (CLK IN)        │
  (master only,         │                         │
   slave uses RCLK)     │                         │
                        │          PARLIO TX      │
  DS92LV1021A DIN0 ←───│ GPIO 7   (TX D0)         │
  DS92LV1021A DIN1 ←───│ GPIO 8   (TX D1)         │
  DS92LV1021A DIN2 ←───│ GPIO 9   (TX D2)         │
  DS92LV1021A DIN3 ←───│ GPIO 10  (TX D3)         │
  DS92LV1021A DIN4 ←───│ GPIO 11  (TX D4)         │
  DS92LV1021A DIN5 ←───│ GPIO 12  (TX D5)         │
  DS92LV1021A DIN6 ←───│ GPIO 13  (TX D6)         │
  DS92LV1021A DIN7 ←───│ GPIO 14  (TX D7)         │
  DS92LV1021A DIN8 ←───│ GPIO 15  (TX D8)         │
  DS92LV1021A DIN9 ←───│ GPIO 16  (TX D9)         │
  DS92LV1021A TCLK ←───│ GPIO 17  (TX CLK OUT)    │
  DS92LV1021A PDB  ←───│ GPIO 18  (TX OE)         │
                       │          PARLIO RX        │
  DS92LV1212A DOUT0 ──→│ GPIO 19  (RX D0)         │
  DS92LV1212A DOUT1 ──→│ GPIO 20  (RX D1)         │
  DS92LV1212A DOUT2 ──→│ GPIO 21  (RX D2)         │
  DS92LV1212A DOUT3 ──→│ GPIO 22  (RX D3)         │
  DS92LV1212A DOUT4 ──→│ GPIO 23  (RX D4)         │
  DS92LV1212A DOUT5 ──→│ GPIO 24  (RX D5)         │
  DS92LV1212A DOUT6 ──→│ GPIO 25  (RX D6)         │
  DS92LV1212A DOUT7 ──→│ GPIO 26  (RX D7)         │
  DS92LV1212A DOUT8 ──→│ GPIO 27  (RX D8)         │
  DS92LV1212A DOUT9 ──→│ GPIO 28  (RX D9)         │
  DS92LV1212A RCLK  ──→│ GPIO 29  (RX CLK IN)     │
  DS92LV1212A LOCK  ──→│ GPIO 30  (LOCK status)   │
                        │                          │
                        │          I2S (local codec)│
  I2S DAC/ADC BCK  ←──→│ GPIO 31                  │
  I2S DAC/ADC WS   ←──→│ GPIO 32                  │
  I2S DAC DOUT     ←───│ GPIO 33                  │
  I2S ADC DIN      ───→│ GPIO 34                  │
                        │                          │
                        │          EMAC (free!)     │
                        │ GPIO 35-43 → Ethernet PHY│
                        │   (independent of bus)    │
                        │                          │
                        │ GPIO 44+ → UART, SPI,    │
                        │   LEDs, buttons, etc.     │
                        └──────────────────────────┘

GPIO budget: 24 (bus) + 4 (I2S) + 9 (Ethernet) + 2 (UART) = 39 of 55 used
Remaining: 16 GPIOs free
```

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
| 49.152 MHz (48k family) | SiT8008B-49-152 | $1.50 | MEMS, ±25ppm, SOT-23 |
| 49.152 MHz (48k family) | ECS-3953M-490 | $0.80 | ±50ppm |
| Dual family | Si5351A-B-GT | $1.50 | I2C programmable, 49.152 + 45.158 MHz |

Slave nodes do **not** need a clock oscillator — they recover the clock from the bus via the DS92LV1212A CDR.

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

### Build the Master Example

```bash
cd examples/master_node
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

### Build the Slave Example

```bash
cd examples/slave_node
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

### Minimal Master Code

```c
#include "audiobus.h"

void app_main(void) {
    abus_config_t config = {
        .role = ABUS_ROLE_MASTER,
        .phy_type = ABUS_PHY_LVDS_SERDES,
        .sample_rate = ABUS_SR_48000,
        .bit_depth = ABUS_DEPTH_32,
        .pins.lvds_pins = {
            .upstream_tx_data = {7,8,9,10,11,12,13,14,15,16},
            .upstream_tx_clk  = 17,
            .upstream_tx_oe   = 18,
            .upstream_rx_data = {19,20,21,22,23,24,25,26,27,28},
            .upstream_rx_clk  = 29,
            .upstream_rx_lock = 30,
            .downstream_tx_clk = -1,  /* single-port master */
        },
    };

    abus_handle_t bus;
    abus_init(&config, &bus);
    abus_start(bus);

    /* Write 2-channel audio downstream */
    int32_t samples[2] = {0, 0};
    while (1) {
        /* Fill samples with your audio data... */
        abus_audio_write(bus, samples, 1);
    }
}
```

### Minimal Slave Code

```c
#include "audiobus.h"

void app_main(void) {
    abus_config_t config = {
        .role = ABUS_ROLE_SLAVE,
        .phy_type = ABUS_PHY_LVDS_SERDES,
        .sample_rate = ABUS_SR_48000,
        .bit_depth = ABUS_DEPTH_32,
        .node_desc = {
            .hw_type = 1,               /* speaker */
            .max_dn_channels = 2,
            .max_up_channels = 0,
            .tunnel_request = (1 << ABUS_TUNNEL_GPIO),
            .tunnel_bw_request = 2,
            .uid = 0x00000001,
        },
        .pins.lvds_pins = {
            /* same pin layout as master... */
            .downstream_tx_clk = -1,  /* end-node, no downstream port */
        },
    };

    abus_handle_t bus;
    abus_init(&config, &bus);
    abus_start(bus);

    /* Read 2-channel audio from bus → feed to I2S DAC */
    int32_t samples[2];
    while (1) {
        if (abus_audio_read(bus, samples, 1) > 0) {
            /* Output samples to I2S... */
        }
    }
}
```

## Project Structure

```
audiobus/
├── components/audiobus/
│   ├── include/
│   │   ├── audiobus.h              # Public API
│   │   ├── audiobus_types.h        # Protocol types, frame format, constants
│   │   ├── audiobus_phy.h          # PHY abstraction (vtable)
│   │   └── audiobus_tunnel.h       # Tunnel subsystem API
│   ├── src/
│   │   ├── audiobus.c              # Core engine, state machine, audio ring buffers
│   │   ├── audiobus_frame.c        # Frame packing, slot map computation
│   │   ├── audiobus_discovery.c    # Node discovery and hot-plug
│   │   ├── audiobus_tunnel.c       # SPI/I2C/GPIO/MIDI tunnel pack/unpack
│   │   ├── phy/
│   │   │   └── phy_lvds_serdes.c   # LVDS PHY driver (PARLIO + DS92LV)
│   │   └── codec/
│   │       └── codec_8b10b.c       # 8b10b encoder/decoder (lookup tables)
│   ├── CMakeLists.txt
│   └── Kconfig                     # Build-time configuration
├── examples/
│   ├── master_node/                # Master node example (sine wave generator)
│   └── slave_node/                 # Slave node example (speaker + mic bridge)
├── docs/
│   └── HARDWARE.md                 # Detailed schematics, BOM, PCB notes
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
