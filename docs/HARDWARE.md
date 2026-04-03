# AudioBus Hardware Design Guide

## Architecture Overview

```
Master Node                    Slave Node 1                  Slave Node 2 (end)
┌─────────────┐  Twisted  ┌─────────────────┐  Twisted  ┌─────────────┐
│ ESP32-P4    │  Pair #1  │   ESP32-P4      │  Pair #2  │ ESP32-P4    │
│             │           │                 │           │             │
│ PARLIO TX──→│DS92LV1021A│←──PARLIO TX     │DS92LV1021A│←──PARLIO TX │
│   (10-bit)  │  ↕ LVDS   │  (10-bit)       │  ↕ LVDS   │  (10-bit)   │
│ PARLIO RX←──│DS92LV1212A│──→PARLIO RX     │DS92LV1212A│──→PARLIO RX │
│             │  (CDR)    │                 │  (CDR)    │             │
│ EMAC → free │           │ EMAC → free     │           │ EMAC → free │
│ (Ethernet/  │           │ (Ethernet/      │           │ (Ethernet/  │
│  WiFi/etc)  │           │  WiFi/etc)      │           │  WiFi/etc)  │
└─────────────┘           │                 │           └─────────────┘
                          │ DS92LV1021A ──→ │  (downstream TX)
                          │ DS92LV1212A ←── │  (downstream RX)
                          └─────────────────┘
                           Intermediate node
                           has 2 port pairs
```

## Key Design Decisions

### Why DS92LV1021A/DS92LV1212A?

These TI 10:1 LVDS serializer/deserializer ICs are designed for video links but
we repurpose them as a self-clocking audio bus PHY:

- **Self-clocking**: The DS92LV1212A has a built-in CDR (Clock and Data Recovery)
  PLL that recovers the clock from 8b10b-encoded data transitions. No separate
  clock wire needed.
- **High bandwidth**: 491 Mbps LVDS line rate at 49.152 MHz parallel clock,
  giving 1024 data bytes per frame at 48 kHz — enough for 64 channels of 32-bit audio.
- **Low jitter**: LVDS differential signaling + CDR PLL provides clean recovered clock
  suitable for audio-grade timing.
- **Cheap**: ~$3-4 each, $7-8 per port pair.
- **10-bit parallel interface**: Matches 8b10b symbol width perfectly.
  PARLIO at 16-bit width connects directly (bits 10-15 unused).

### Why half-duplex on single pair?

Full-duplex on a single pair requires echo cancellation (like 100BASE-T1 Ethernet
PHYs), which needs the EMAC. By using half-duplex LVDS with direction switching,
we keep the ESP32-P4 EMAC free for normal networking (Ethernet, WiFi, etc).

The bandwidth penalty of half-duplex is minimal: at 491 Mbps, even splitting
the frame 50/50 between downstream and upstream gives 245+ Mbps per direction —
far more than needed for 64 channels of audio.

### Clock Distribution Through the Chain

```
49.152 MHz       DS92LV1021A     LVDS pair      DS92LV1212A      49.152 MHz
oscillator  ───→ TCLK (master) ──────────────→ CDR PLL ─────→ RCLK (recovered)
                                                                    │
                                                                    ├──→ PARLIO RX ext clk
                                                                    ├──→ PARLIO TX ext clk
                                                                    │      (for downstream)
                                                                    └──→ I2S MCLK (local DAC/ADC)
```

The master generates the bus clock from a 49.152 MHz crystal oscillator.
Each slave recovers the exact same clock via the DS92LV1212A CDR. This recovered
clock is used for:
1. PARLIO RX capture (incoming data)
2. PARLIO TX output (forwarding/upstream data to downstream nodes)
3. Local I2S DAC/ADC clocking (sample-accurate sync with master)

This gives zero-jitter, phase-locked audio across the entire chain.

## Bill of Materials (per node)

### Master Node

| Component | Part Number | Qty | Description | ~Price |
|-----------|-------------|-----|-------------|--------|
| MCU | ESP32-P4 | 1 | Dual RISC-V 400MHz, PARLIO + EMAC | $4-5 |
| Serializer | DS92LV1021A | 1 | 10:1 LVDS serializer | $3.50 |
| Deserializer | DS92LV1212A | 1 | 1:10 LVDS deserializer w/ CDR | $3.50 |
| Clock | 49.152 MHz osc | 1 | CMOS crystal oscillator (3.3V) | $1.00 |
| Magnetics | Pulse PE-65612 | 1 | Common-mode choke for twisted pair (optional) | $0.80 |
| Connector | RJ45 or 3.5mm | 1 | Twisted pair connector | $0.30 |
| Passives | Various | ~10 | Decoupling caps, termination resistors | $0.50 |
| **Total** | | | | **~$14** |

### Slave End-Node (speaker, mic, etc.)

| Component | Part Number | Qty | Description | ~Price |
|-----------|-------------|-----|-------------|--------|
| MCU | ESP32-P4 | 1 | (or ESP32-S3 for simpler nodes) | $4-5 |
| Serializer | DS92LV1021A | 1 | Upstream TX | $3.50 |
| Deserializer | DS92LV1212A | 1 | Upstream RX (CDR = bus clock!) | $3.50 |
| Audio DAC | PCM5102A | 1 | I2S stereo DAC (optional) | $2.50 |
| Audio ADC | PCM1808 | 1 | I2S stereo ADC (optional) | $3.00 |
| Connector | RJ45 or 3.5mm | 1 | Upstream twisted pair | $0.30 |
| Passives | Various | ~12 | Decoupling, termination, I2S passives | $0.70 |
| **Total** | | | | **~$18** |

### Intermediate (Repeater) Node

Same as slave end-node, plus:

| Component | Part Number | Qty | Description | ~Price |
|-----------|-------------|-----|-------------|--------|
| Serializer | DS92LV1021A | 1 | Downstream TX | $3.50 |
| Deserializer | DS92LV1212A | 1 | Downstream RX | $3.50 |
| Bus MUX | SN74CB3Q3257 | 3 | Quad bus switch for GPIO mux (or use GPIO matrix) | $1.50 |
| Connector | RJ45 or 3.5mm | 1 | Downstream twisted pair | $0.30 |
| **Extra** | | | | **~$9** |
| **Total** | | | | **~$27** |

## Schematic Details

### DS92LV1021A Serializer Wiring

```
                    DS92LV1021A
                   ┌───────────┐
ESP32-P4           │           │
PARLIO TX D0  ────→│ DIN0      │
PARLIO TX D1  ────→│ DIN1      │
PARLIO TX D2  ────→│ DIN2      │           ┌─────────────┐
PARLIO TX D3  ────→│ DIN3      │           │ Twisted Pair │
PARLIO TX D4  ────→│ DIN4   OUT+ ─────────→│ wire A       │
PARLIO TX D5  ────→│ DIN5   OUT- ─────────→│ wire B       │
PARLIO TX D6  ────→│ DIN6      │           └─────────────┘
PARLIO TX D7  ────→│ DIN7      │
PARLIO TX D8  ────→│ DIN8      │
PARLIO TX D9  ────→│ DIN9      │
                   │           │
PARLIO TX CLK ────→│ TCLK      │  ← 49.152 MHz
                   │           │
GPIO (OE)     ────→│ PDB       │  ← HIGH = output enabled
                   │           │     LOW = high-Z (receive mode)
3.3V ─────────────→│ VCC       │
GND ──────────────→│ GND       │
                   └───────────┘

Decoupling: 100nF ceramic on VCC, close to pin.
OUT+/OUT- termination: 100Ω differential across pair at receiver end.
```

### DS92LV1212A Deserializer Wiring

```
                    DS92LV1212A
                   ┌───────────┐
                   │           │           ┌─────────────┐
                   │    IN+ ←──────────────│ wire A       │ Twisted Pair
                   │    IN- ←──────────────│ wire B       │ (same pair as TX)
                   │           │           └─────────────┘
ESP32-P4           │           │
PARLIO RX D0  ←────│ DOUT0     │
PARLIO RX D1  ←────│ DOUT1     │
PARLIO RX D2  ←────│ DOUT2     │
PARLIO RX D3  ←────│ DOUT3     │
PARLIO RX D4  ←────│ DOUT4     │
PARLIO RX D5  ←────│ DOUT5     │
PARLIO RX D6  ←────│ DOUT6     │
PARLIO RX D7  ←────│ DOUT7     │
PARLIO RX D8  ←────│ DOUT8     │
PARLIO RX D9  ←────│ DOUT9     │
                   │           │
PARLIO RX CLK ←────│ RCLK      │  → Recovered 49.152 MHz (self-clocking!)
                   │           │
GPIO (input)  ←────│ LOCK      │  → HIGH = CDR PLL locked (link active)
                   │           │     LOW = no signal / lost lock
3.3V ─────────────→│ VCC       │
GND ──────────────→│ GND       │
                   └───────────┘

IMPORTANT: IN+/IN- share the same twisted pair as the serializer's OUT+/OUT-.
The serializer is high-Z when PDB=LOW, allowing the remote serializer to drive.
The deserializer input is always high-impedance (doesn't load the line).

Termination: 100Ω differential resistor across IN+/IN-, placed close to the IC.
```

### Half-Duplex Direction Switching

```
Local Serializer                     Remote Serializer
OUT+/OUT- ──┐                   ┌── OUT+/OUT-
             ├── Twisted Pair ──┤
IN+/IN-   ──┘                   └── IN+/IN-
Local Deserializer               Remote Deserializer

TX mode (local transmitting):
  Local PDB = HIGH  (serializer drives the pair)
  Remote PDB = LOW  (remote serializer high-Z)
  Local RCLK = echo of own data (CDR stays locked)
  Remote RCLK = recovers our data clock

RX mode (local receiving):
  Local PDB = LOW   (serializer high-Z)
  Remote PDB = HIGH (remote serializer drives)
  Local RCLK = recovers remote data clock
  Remote RCLK = echo of own data
```

### Twisted Pair Specifications

- **Cable**: Cat5/Cat5e/Cat6 (single pair used), or dedicated twisted pair
- **Impedance**: 100Ω differential (standard for Cat5)
- **Maximum distance**: ~15-20 meters per segment at 491 Mbps
  (depends on cable quality; shorter with cheap cable)
- **Connector options**:
  - RJ45 (use pins 1,2 or 3,6 — standard Ethernet pair)
  - 3.5mm TRS jack (tip = +, ring = -, sleeve = shield/GND)
  - JST-PH 3-pin (compact, for internal connections)
  - Ethercon (ruggedized RJ45 for pro audio)
- **Optional**: Common-mode choke on each end for EMI reduction
- **Power over bus**: NOT included by default. Use a separate pair for phantom
  power, or add a DC bias tee circuit if power-over-single-pair is needed.

### Power-over-Bus Option (Advanced)

For powering slave nodes over the same cable (requires 2+ pairs):
- Pair 1: LVDS data (audio bus)
- Pair 2: DC power (e.g., 12V/24V with local regulator on each node)

If using Cat5 cable, pairs 4,5 and 7,8 are available for power.
Standard PoE injectors/splitters can be adapted for this.

## ESP32-P4 Pin Budget

### Single-Port Node (Master or End-Slave)

| Function | Pins | Count |
|----------|------|-------|
| PARLIO TX data | D0-D9 | 10 |
| PARLIO TX clock | CLK_OUT | 1 |
| Serializer OE | GPIO | 1 |
| PARLIO RX data | D0-D9 | 10 |
| PARLIO RX clock | CLK_IN (RCLK) | 1 |
| Deserializer LOCK | GPIO input | 1 |
| **Subtotal: bus** | | **24** |
| I2S (local audio) | BCK, WS, DOUT, DIN | 4 |
| SPI (optional) | CLK, MOSI, MISO, CS | 4 |
| UART (debug) | TX, RX | 2 |
| **Subtotal: peripherals** | | **10** |
| **Total used** | | **34** |
| **ESP32-P4 GPIO available** | | **55** |
| **Remaining** | | **21** |

### Dual-Port Intermediate Node (with GPIO Matrix MUX)

Uses same PARLIO pins for both ports; external SN74CB3Q3257 bus switches
route signals to the active port's SerDes pair:

| Function | Pins | Count |
|----------|------|-------|
| PARLIO TX/RX (shared) | | 24 |
| MUX select | 1 GPIO | 1 |
| Upstream serializer OE | GPIO | 1 |
| Downstream serializer OE | GPIO | 1 |
| Upstream LOCK | GPIO input | 1 |
| Downstream LOCK | GPIO input | 1 |
| **Subtotal: bus** | | **29** |

## Clock Source Options

### 49.152 MHz (48 kHz family: 48/96 kHz)

Standard audio clock, widely available:
- **SiT8008B-49-152** (SiTime MEMS, $1.50, ±25ppm, 3.3V, tiny SOT-23)
- **ECS-3953M-490** (ECS, $0.80, ±50ppm, 3.3V)
- **ASE-49.152MHZ** (Abracon, $1.20, ±50ppm)

### 45.1584 MHz (44.1 kHz family: 44.1/88.2 kHz)

Less common but available:
- Can be generated from Si5351A PLL ($1.50, I2C programmable)

### Si5351A for Dual-Family Support

The Si5351A clock generator can output both 49.152 and 45.1584 MHz
(switchable via I2C). This allows supporting both sample rate families
from a single 25/27 MHz reference crystal:
- **Si5351A-B-GT** ($1.50, 3 outputs, I2C, 8-MSOP)
- Input: 25 or 27 MHz crystal
- Output A: 49.152 MHz (for 48kHz family)
- Output B: 45.1584 MHz (for 44.1kHz family)
- Output C: I2S MCLK for local codec (24.576 or 22.5792 MHz)

## Performance Summary

| Config | Channels/direction | Audio bytes/direction | Aux bytes available |
|--------|-------------------|-----------------------|--------------------|
| 48 kHz / 32-bit | 64 | 256 | 246 |
| 48 kHz / 24-bit | 64 | 192 | 310 |
| 96 kHz / 32-bit | 62 | 248 | 0 |
| 96 kHz / 24-bit | 64 | 192 | 54 |
| 44.1 kHz / 32-bit | 64 | 256 | 246 |

Frame size: 1024 bytes at 48/44.1 kHz, 512 bytes at 96/88.2 kHz.
Overhead: 16 bytes (sync + header + guard + CRC).
Latency per hop: ~21 µs (1 sample period) at 48 kHz, ~42 µs for store-and-forward.
Round-trip 8 nodes: ~336 µs at 48 kHz.

## ESP32-S3 Compatibility Notes

The ESP32-S3 does NOT have PARLIO. For simpler end-nodes on ESP32-S3:
- Use SPI master at 49.152 MHz (with external clock) to drive the serializer
- SPI sends pre-encoded 8b10b bitstream as a serial byte stream
- Data width is 1 (serial) instead of 10 (parallel), reducing throughput
- Or use the LCD_CAM parallel output interface (8-bit, up to ~40 MHz)
- ESP32-S3 also has no EMAC, so networking requires WiFi or SPI Ethernet

ESP32-S3 as an end-node with reduced channel count (8-16 channels) is feasible.
For intermediate/bridge nodes and full 64-channel operation, use ESP32-P4.
