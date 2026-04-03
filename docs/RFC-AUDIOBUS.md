```
Internet Engineering Task Force (IETF)            S. Sosnowski
Request for Comments: NNNN                         DatanoiseTV
Category: Standards Track                        4 April 2026
ISSN: 2070-1721


   AudioBus: Multiplexed Audio and Control Transport
          over Ethernet and LVDS Links

Abstract

   This document specifies AudioBus, a protocol for
   deterministic, low-latency transport of multi-channel
   digital audio and control data over IEEE 802.3 Ethernet
   networks and Low-Voltage Differential Signaling serial
   links.  The protocol supports up to 64 channels of
   32-bit PCM audio at sample rates up to 96 kHz,
   multiplexed with GPIO, MIDI, SPI, and I2C control
   tunnels.  Clock synchronization is achieved using a
   profile of IEEE 1588 Precision Time Protocol with
   hardware timestamping, providing sub-microsecond
   accuracy across all nodes.  The protocol requires no
   manual configuration; nodes discover each other and
   elect a clock reference automatically.

Status of This Memo

   This is an Internet Standards Track document.

   This document is a product of the Internet Engineering
   Task Force (IETF).  It represents the consensus of the
   IETF community.  It has received public review and has
   been approved for publication by the Internet
   Engineering Steering Group (IESG).  Further information
   on Internet Standards is available in Section 2 of
   RFC 7841.

   Information about the current status of this document,
   any errata, and how to provide feedback on it may be
   obtained at https://www.rfc-editor.org/info/rfcNNNN.

Copyright Notice

   Copyright (c) 2026 IETF Trust and the persons
   identified as the document authors.  All rights
   reserved.

   This document is subject to BCP 78 and the IETF
   Trust's Legal Provisions Relating to IETF Documents
   (https://trustee.ietf.org/license-info) in effect on
   the date of publication of this document.  Please
   review these documents carefully, as they describe
   your rights and restrictions with respect to this
   document.  Code Components extracted from this
   document must include Revised BSD License text as
   described in Section 4.e of the Trust Legal Provisions
   and are provided without warranty as described in the
   Revised BSD License.

Table of Contents

   1.  Introduction
       1.1.  Motivation
       1.2.  Design Goals
       1.3.  Scope
   2.  Terminology and Conventions
       2.1.  Requirements Language
       2.2.  Definitions
   3.  Protocol Architecture
       3.1.  Layer Model
       3.2.  Transport Modes
       3.3.  Node Roles
       3.4.  Stream Model
   4.  Common Packet Format
       4.1.  Common Header
       4.2.  Packet Type Registry
       4.3.  Byte Ordering
   5.  Ethernet Transport
       5.1.  Frame Encapsulation
       5.2.  EtherType
       5.3.  Multicast Addressing
       5.4.  VLAN Tagging
       5.5.  Quality of Service
       5.6.  Coexistence with IP Traffic
   6.  LVDS Serial Transport
       6.1.  Physical Layer
       6.2.  8b10b Line Coding
       6.3.  TDM Frame Structure
       6.4.  Frame Header
       6.5.  Slot Map
       6.6.  Half-Duplex Operation
       6.7.  Daisy-Chain Forwarding
   7.  Clock Synchronization
       7.1.  PTP Profile
       7.2.  Grandmaster Election
       7.3.  Sync Message
       7.4.  Follow_Up Message
       7.5.  Delay_Req Message
       7.6.  Delay_Resp Message
       7.7.  Media Clock Recovery
       7.8.  Hardware Timestamping
   8.  Node Discovery
       8.1.  Beacon Format
       8.2.  Beacon Timing
       8.3.  Node Identification
       8.4.  Hardware Types
       8.5.  Node Expiry
   9.  Stream Management
       9.1.  Stream Announcement
       9.2.  Channel Labels
       9.3.  Stream Subscription
       9.4.  Dynamic Channel Count
       9.5.  Stream Teardown
   10. Audio Data Transport
       10.1. Audio Packet Format
       10.2. Sample Encoding
       10.3. Sample Interleaving
       10.4. Presentation Timestamps
       10.5. Packet Interval
       10.6. Sequence Numbering
       10.7. Playout Buffer
       10.8. Packet Loss Concealment
   11. Control Tunneling
       11.1. Tunnel Packet Format
       11.2. GPIO Tunnel
       11.3. MIDI Tunnel
       11.4. SPI Tunnel
       11.5. I2C Tunnel
       11.6. Sideband Channel
   12. Metadata
       12.1. Metadata Packet Format
       12.2. Metadata Entry Encoding
       12.3. Standard Metadata Keys
   13. Latency Measurement
       13.1. Ping Message
       13.2. Pong Message
       13.3. Round-Trip Computation
       13.4. Jitter Estimation
   14. Error Handling
       14.1. Unknown Packet Types
       14.2. Version Mismatch
       14.3. CRC Failure
       14.4. Sequence Gaps
       14.5. Late Packets
       14.6. Grandmaster Loss
   15. Extensibility and Versioning
       15.1. Version Field
       15.2. Reserved Fields
       15.3. Private-Use Ranges
   16. Security Considerations
       16.1. Threat Model
       16.2. Confidentiality
       16.3. Integrity
       16.4. Availability
       16.5. Authentication
       16.6. Mitigations
   17. IANA Considerations
       17.1. EtherType Assignment
       17.2. Multicast OUI Assignment
       17.3. AudioBus Packet Type Registry
       17.4. AudioBus Tunnel Type Registry
       17.5. AudioBus Hardware Type Registry
       17.6. AudioBus Metadata Key Registry
   18. References
       18.1. Normative References
       18.2. Informative References
   Appendix A.  Channel Capacity Tables
   Appendix B.  Recommended Hardware
   Appendix C.  Example Message Exchange
   Acknowledgements
   Authors' Addresses


1.  Introduction

1.1.  Motivation

   Professional and consumer audio systems require
   transport of multiple channels of high-resolution
   digital audio alongside control data over simple
   physical interconnections.  Existing solutions are
   proprietary, expensive, limited in channel count, or
   require specialized network infrastructure.

   AudioBus addresses these limitations by defining an
   open protocol that operates over two complementary
   transports: standard IEEE 802.3 Ethernet using
   commodity switches, and dedicated LVDS serial links
   over a single twisted pair for cost-optimized,
   ultra-low-latency daisy-chain topologies.

1.2.  Design Goals

   The protocol satisfies the following requirements:

   G1.  Deterministic latency: fixed, predictable
        end-to-end delay with no data-dependent
        variation.

   G2.  Zero jitter: audio playout synchronized to a
        common time base across all nodes, independent
        of network topology.

   G3.  Plug-and-play: no manual configuration.  Nodes
        discover each other, negotiate capabilities,
        and begin streaming automatically.

   G4.  Dynamic: channel counts, sample rates, and
        stream configurations may change at runtime.

   G5.  Coexistence: Ethernet transport MUST NOT
        interfere with IP traffic on the same network.

   G6.  Simplicity: implementable on microcontrollers
        with 400 MHz clock and 512 KB SRAM.

   G7.  Low cost: all required components are commodity
        parts available from multiple vendors.

1.3.  Scope

   This specification defines wire formats, transport
   encapsulation, clock synchronization, discovery,
   stream management, audio packetization, control
   tunneling, metadata distribution, and latency
   measurement.

   This specification does not define hardware
   implementation details, application-layer processing
   logic, content protection, or wireless transports.


2.  Terminology and Conventions

2.1.  Requirements Language

   The key words "MUST", "MUST NOT", "REQUIRED", "SHALL",
   "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED",
   "NOT RECOMMENDED", "MAY", and "OPTIONAL" in this
   document are to be interpreted as described in BCP 14
   [RFC2119] [RFC8174] when, and only when, they appear
   in all capitals, as shown here.

2.2.  Definitions

   Node:  A device that participates in the AudioBus
      protocol as a talker, listener, or both.

   Talker:  A node that publishes one or more audio
      streams.

   Listener:  A node that subscribes to one or more
      audio streams.

   Stream:  A unidirectional flow of audio data from
      one talker to one or more listeners.  A stream
      has a fixed sample rate and bit depth, and a
      dynamic channel count.

   Stream ID:  A 16-bit identifier unique within the
      scope of its talker.

   Node UID:  A 32-bit identifier unique within an
      AudioBus network.

   Presentation Timestamp:  A 64-bit nanosecond
      timestamp in the PTP time base indicating when
      audio samples SHOULD begin playout.

   Grandmaster:  The node whose clock is the reference
      for all other nodes, elected via the Best Master
      Clock algorithm.

   Packet Interval:  The time in microseconds between
      consecutive audio packets within a stream.

   Slot Map:  (LVDS only) A fixed assignment of byte
      positions within the TDM frame to audio channels
      and tunnel streams.

   Guard Time:  (LVDS only) Idle symbols inserted at
      direction boundaries for CDR re-lock.


3.  Protocol Architecture

3.1.  Layer Model

   +-------------------------------------------------+
   |            Application Layer                     |
   +-------------------------------------------------+
   |            Stream Management                     |
   |  (discovery, announce, subscribe, metadata)      |
   +-------------------------------------------------+
   |            Audio Transport                       |
   |  (packetization, timestamps, sequencing)         |
   +-------------------------------------------------+
   |            Clock Synchronization                 |
   |  (PTP profile, grandmaster election)             |
   +-------------------------------------------------+
   |            Control Tunnel                        |
   |  (GPIO, MIDI, SPI, I2C)                          |
   +-------------------------------------------------+
   |            Framing                               |
   |  (common header, CRC)                            |
   +-------------------------------------------------+
   |            Transport                             |
   |  +-------------------+  +-------------------+   |
   |  | Ethernet (L2)     |  | LVDS Serial (TDM) |   |
   |  +-------------------+  +-------------------+   |
   +-------------------------------------------------+
   |            Physical                              |
   |  +-------------------+  +-------------------+   |
   |  | 100/1000BASE-T    |  | LVDS twisted pair |   |
   |  +-------------------+  +-------------------+   |
   +-------------------------------------------------+

         Figure 1: AudioBus Protocol Layer Model

3.2.  Transport Modes

   AudioBus defines two transport modes:

   Ethernet Mode:  Packets are encapsulated in IEEE
      802.3 frames with a dedicated EtherType.  Nodes
      connect to commodity switches in star or tree
      topologies.  Up to 64 nodes are supported.

   LVDS Mode:  Audio and control data are multiplexed
      into a synchronous TDM frame on a single twisted
      pair using LVDS signaling with 8b10b coding.
      Nodes connect in a daisy-chain topology with
      latency of approximately 21 microseconds per hop.

   An implementation MAY support one or both modes.
   A bridge node MAY interconnect an Ethernet segment
   with an LVDS daisy chain.

3.3.  Node Roles

   In Ethernet mode, all nodes are peers.  Any node MAY
   be a talker, listener, or both.  The grandmaster is
   elected automatically.

   In LVDS mode, one node is the bus master (node ID 0).
   The master generates the bus clock and initiates
   frame transmissions.  All other nodes are slaves.

3.4.  Stream Model

   A stream is a unidirectional, multi-channel audio
   flow characterized by:

   -  Stream ID (16-bit, unique per talker)
   -  Talker UID (32-bit)
   -  Channel count (1 to 64, dynamic)
   -  Sample rate (44100, 48000, 88200, or 96000 Hz)
   -  Bit depth (16, 24, or 32 bits)
   -  Packet interval (125 to 4000 microseconds)
   -  Encoding (0 = linear PCM)

   A talker MAY publish multiple concurrent streams.
   A listener MAY subscribe to multiple streams.
   Listeners MAY subscribe to a channel subset via
   a bitmask.


4.  Common Packet Format

4.1.  Common Header

   All AudioBus packets begin with a 12-octet header:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    Version    |   Pkt Type    |            Flags              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Source UID                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Sequence Number         |       Payload Length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 2: AudioBus Common Header

   Version:  8 bits.  Protocol version.  This document
      defines version 1.  Receivers MUST discard packets
      with an unrecognized version (Section 14.2).

   Pkt Type:  8 bits.  Payload type (Section 4.2).

   Flags:  16 bits.

      Bit 0:  VLAN tag present (Ethernet only).
      Bits 1-3:  Reserved.  MUST be zero on transmit.
         Receivers MUST ignore these bits.
      Bits 4-9:  DSCP value for QoS (Section 5.5).
      Bits 10-15:  Reserved.  MUST be zero.

   Source UID:  32 bits.  Unique identifier of the
      originating node (Section 8.3).

   Sequence Number:  16 bits.  Per-source counter.
      Incremented by one for each packet.  Wraps from
      65535 to 0.

   Payload Length:  16 bits.  Length of the payload
      following this header, in octets.  Does not
      include the 12-octet header.

4.2.  Packet Type Registry

   +---------+------------------+----------------------------+
   | Value   | Name             | Reference                  |
   +---------+------------------+----------------------------+
   | 0x00    | Reserved         |                            |
   | 0x01    | AUDIO            | Section 10.1               |
   | 0x02    | BEACON           | Section 8.1                |
   | 0x03    | SUBSCRIBE        | Section 9.3                |
   | 0x04    | UNSUBSCRIBE      | Section 9.3                |
   | 0x05    | STREAM_ANNOUNCE  | Section 9.1                |
   | 0x06    | STREAM_DELETE    | Section 9.5                |
   | 0x10    | PTP_SYNC         | Section 7.3                |
   | 0x11    | PTP_FOLLOW_UP    | Section 7.4                |
   | 0x12    | PTP_DELAY_REQ    | Section 7.5                |
   | 0x13    | PTP_DELAY_RESP   | Section 7.6                |
   | 0x20    | TUNNEL           | Section 11.1               |
   | 0x30    | METADATA         | Section 12.1               |
   | 0x40    | PING             | Section 13.1               |
   | 0x41    | PONG             | Section 13.2               |
   | 0x50-7F | Private Use      | Section 15.3               |
   | 0x80-FF | Reserved         |                            |
   +---------+------------------+----------------------------+

            Table 1: Packet Type Values

   Receivers MUST discard packets with unrecognized
   type values (Section 14.1).

4.3.  Byte Ordering

   All multi-octet integer fields are encoded in network
   byte order (big-endian) per [RFC791].

   Audio sample data is encoded in big-endian byte order
   (most significant octet first) within each sample
   word (Section 10.2).


5.  Ethernet Transport

5.1.  Frame Encapsulation

   AudioBus packets are carried in IEEE 802.3 frames:

   +---------------------+
   | Destination MAC      |  6 octets
   +---------------------+
   | Source MAC            |  6 octets
   +---------------------+
   | [802.1Q Tag]          |  4 octets (OPTIONAL)
   +---------------------+
   | EtherType             |  2 octets
   +---------------------+
   | AudioBus Common Hdr   | 12 octets
   +---------------------+
   | Payload               |  variable
   +---------------------+
   | FCS                   |  4 octets (by hardware)
   +---------------------+

         Figure 3: Ethernet Encapsulation

   An Ethernet frame MUST carry exactly one AudioBus
   packet.  Implementations MUST NOT concatenate
   multiple AudioBus packets in a single frame.

5.2.  EtherType

   AudioBus uses EtherType 0x88B6 (IEEE 802 Local
   Experimental Ethertype 1).

   Note: A dedicated EtherType assignment will be
   requested from the IEEE Registration Authority
   upon standardization.

5.3.  Multicast Addressing

   AudioBus uses locally administered multicast MAC
   addresses with prefix 01:60:AB.

   Discovery and Control:  01:60:AB:FF:FF:00

      Used for BEACON, SUBSCRIBE, UNSUBSCRIBE,
      TUNNEL, METADATA, PING, and PONG packets.

   PTP Synchronization:  01:60:AB:FF:FF:01

      Used for PTP_SYNC, PTP_FOLLOW_UP,
      PTP_DELAY_REQ, and PTP_DELAY_RESP packets.

   Audio Streams:  01:60:AB:HH:LL:00

      HH and LL are the high and low octets of
      the 16-bit Stream ID.  Each active stream
      has a unique multicast address.

   Implementations MUST join the discovery and PTP
   groups on initialization.  Implementations MUST
   join stream groups upon subscription and leave
   them upon unsubscription.

5.4.  VLAN Tagging

   Implementations MAY use IEEE 802.1Q VLAN tagging.

   When used, the VLAN ID MUST be configurable with a
   default of no tagging.  The Priority Code Point
   SHOULD be 6 (voice/network control).  All AudioBus
   nodes on one network MUST use the same VLAN ID.

5.5.  Quality of Service

   Implementations SHOULD set DSCP values as follows:

   +-----------------------+------------+------------------+
   | Packet Type           | DSCP Value | PHB              |
   +-----------------------+------------+------------------+
   | AUDIO                 | 46         | EF               |
   | PTP_SYNC/FOLLOW_UP    | 48         | CS6              |
   | PTP_DELAY_REQ/RESP    | 48         | CS6              |
   | TUNNEL                | 34         | AF41             |
   | All others            |  0         | Best Effort      |
   +-----------------------+------------+------------------+

            Table 2: DSCP Assignments

   Implementations MUST function correctly on networks
   without QoS support.

5.6.  Coexistence with IP Traffic

   AudioBus uses EtherType 0x88B6, which is distinct
   from IPv4 (0x0800), IPv6 (0x86DD), ARP (0x0806),
   and all other assigned EtherTypes.

   An implementation MUST NOT interfere with IP
   protocol stacks on the same interface.  AudioBus
   and IP traffic MAY share the same physical port
   and switch infrastructure.

   Implementations MUST self-police bandwidth by
   limiting aggregate audio packet rate to declared
   stream parameters.  The maximum bandwidth of a
   single stream is:

      BW_bits = (C * D * S) + (S / N) * (OH * 8)

   Where C is channels, D is bit depth, S is sample
   rate in Hz, N is samples per packet, and OH is
   the total per-packet overhead in octets
   (Ethernet header + AudioBus header + audio header
   = 14 + 12 + 20 = 46 octets).


6.  LVDS Serial Transport

6.1.  Physical Layer

   The LVDS transport uses Low-Voltage Differential
   Signaling conforming to TIA/EIA-644 [TIA644] over
   a single twisted pair.

   Cable:  Category 5e or better, 100-ohm differential
      impedance.

   Maximum length:  15 meters per segment at the
      maximum line rate.

   Termination:  Each end of the link MUST be
      terminated with a 100-ohm differential resistor
      placed within 10 mm of the receiver input pins.

   Transceiver:  A device with separate LVDS driver
      and receiver on the same differential pair, with
      a driver-enable control signal, is REQUIRED.

6.2.  8b10b Line Coding

   All data is encoded using the 8b10b line code
   defined in [IEEE802.3] Clause 36.

   The following K-characters are defined:

   +--------+-------+----------------------------------+
   | Symbol | Value | Usage                            |
   +--------+-------+----------------------------------+
   | K28.5  | 0xBC  | Comma (alignment, frame sync)    |
   | K28.1  | 0x3C  | Start of Frame                   |
   | K27.7  | 0xFB  | Direction turnaround             |
   | K28.3  | 0x7C  | Idle                             |
   | K29.7  | 0xFD  | End of Frame                     |
   | K23.7  | 0xF7  | Discovery beacon                 |
   +--------+-------+----------------------------------+

            Table 3: K-Character Assignments

6.3.  TDM Frame Structure

   One frame is transmitted per audio sample period:

   +------+--------+------------------------------+
   | Byte | Length | Field                        |
   +------+--------+------------------------------+
   |    0 |      1 | K28.5 (Comma)                |
   |    1 |      1 | K28.1 (Start of Frame)       |
   |    2 |      8 | Frame Header (Section 6.4)   |
   |   10 |      1 | Downstream Sideband          |
   |   11 |    var | Downstream Audio Slots        |
   |      |    var | Downstream Aux Data           |
   |      |      2 | Guard (K28.5 + K27.7)        |
   |      |      1 | Upstream Sideband            |
   |      |    var | Upstream Audio Slots          |
   |      |    var | Upstream Aux Data             |
   |  F-4 |      4 | CRC-32                       |
   +------+--------+------------------------------+

            Table 4: TDM Frame Layout

   Frame sizes:

   -  1024 octets at 48000 or 44100 Hz
   -   512 octets at 96000 or 88200 Hz

   Parallel clock frequencies:

   -  49,152,000 Hz for 48 kHz family
   -  45,158,400 Hz for 44.1 kHz family

6.4.  Frame Header

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Frame Counter           |  Frame Type   |    Flags      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Node Count   |DN Audio Slots |UP Audio Slots | SlotMap CRC   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 4: LVDS Frame Header

   Frame Counter:  16 bits.  Monotonically increasing.

   Frame Type:  8 bits.
      0x00 = Normal, 0x01 = Discovery,
      0x02 = Configuration, 0x03 = Status.

   Flags:  8 bits.
      Bits 0-1:  Sample rate (0=48k, 1=96k,
         2=44.1k, 3=88.2k).
      Bits 2-3:  Bit depth (0=16, 1=24, 2=32).
      Bit 4:  Sideband valid.
      Bits 5-7:  Reserved.  MUST be zero.

   Node Count:  8 bits.  Active nodes in chain.

   DN/UP Audio Slots:  8 bits each.  Slot counts.

   SlotMap CRC:  8 bits.  CRC-8 of the active
      slot map for configuration verification.

6.5.  Slot Map

   The slot map assigns byte offsets within the frame
   to audio channels and tunnel streams.  It is
   computed by the master during configuration and
   distributed to all slaves.

   The slot map MUST NOT change during operation.
   Reconfiguration requires the master to re-enter
   the configuration state.

   Each audio slot specifies:

   -  Byte offset within frame (16 bits)
   -  Byte width: 2, 3, or 4 (8 bits)
   -  Logical channel ID: 0-127 (8 bits)
   -  Owning node ID (8 bits)
   -  Direction: 0=downstream, 1=upstream (8 bits)

   The packing algorithm:

   1.  For each node in ID order, allocate downstream
       audio channels.
   2.  Allocate downstream tunnel bandwidth.
   3.  Insert guard.
   4.  For each node in ID order, allocate upstream
       audio channels.
   5.  Allocate upstream tunnel bandwidth.
   6.  Verify total does not exceed frame size
       minus 4 (CRC).
   7.  Compute CRC-8 over the slot map.

6.6.  Half-Duplex Operation

   The link is half-duplex on a single pair.  The
   downstream phase precedes the upstream phase
   within each frame.

   Direction change is signaled by K28.5 followed
   by K27.7.

   After the guard sequence, the transmitter MUST
   disable its LVDS driver within 10 nanoseconds.
   The new transmitter MUST NOT enable its driver
   until a minimum of 650 nanoseconds (32 symbol
   periods) have elapsed, to allow CDR re-lock at
   the receiver.

   Failure to observe the guard time may result in
   bit errors in the first symbols of the new
   direction.  Implementations SHOULD monitor CRC
   errors and increase guard time if the error rate
   exceeds one per 10,000 frames.

6.7.  Daisy-Chain Forwarding

   Nodes connect in a chain:

      Master <-> Node 1 <-> Node 2 <-> ... <-> Node N

   Each intermediate node receives the frame, extracts
   assigned channels, inserts upstream data, and
   retransmits.  This store-and-forward operation adds
   one frame period of latency per hop.

   Total round-trip latency for H hops at sample rate
   Fs:

      RTT = 2 * H * (1 / Fs)

   Example: 8 nodes at 48 kHz = 333 microseconds.


7.  Clock Synchronization

7.1.  PTP Profile

   AudioBus defines a PTP profile based on
   IEEE 1588-2019 [IEEE1588].

   +-----------------------------+---------------------------+
   | Parameter                   | Value                     |
   +-----------------------------+---------------------------+
   | Transport                   | IEEE 802.3 (Layer 2)      |
   | Delay mechanism             | End-to-End                |
   | Sync interval               | 125 ms                    |
   | Delay_Req interval          | 500 ms                    |
   | Clock domain                | 0                         |
   | Operation mode              | Two-step                  |
   | Hardware timestamping       | REQUIRED                  |
   | Target accuracy             | < 1 microsecond           |
   +-----------------------------+---------------------------+

            Table 5: PTP Profile Parameters

7.2.  Grandmaster Election

   The grandmaster is elected using the Best Master
   Clock (BMC) algorithm.  Comparison criteria in
   priority order:

   1.  Clock Class: lower wins.
       -  6:  Locked to external reference.
       -  13: Application-specific.
       -  248: Free-running (default).

   2.  Priority: lower wins.  Range 1-255.
       Default: 128.

   3.  Node UID: lower wins (tiebreaker).

   When the grandmaster is unreachable for 3
   consecutive Sync intervals (375 ms), all nodes
   MUST re-run BMC.  The winning node MUST begin
   Sync transmission within one Sync interval.

   If all remaining nodes have Clock Class 248
   (free-running), the node with the lowest UID
   becomes grandmaster.  There MUST always be
   exactly one grandmaster.

7.3.  Sync Message (Type 0x10)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       PTP Sequence            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |              Origin Timestamp (64 bits, ns)                   |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 5: PTP Sync Message

   Transmitted by the grandmaster.  Origin Timestamp
   is a coarse estimate; precise time is in the
   Follow_Up.

7.4.  Follow_Up Message (Type 0x11)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       PTP Sequence            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |         Precise Origin Timestamp (64 bits, ns)                |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 6: PTP Follow_Up Message

   The PTP Sequence MUST match the preceding Sync.
   The Precise Origin Timestamp is the hardware-
   captured time of the Sync departure.

7.5.  Delay_Req Message (Type 0x12)

   Identical format to Sync (Figure 5).  Sent by a
   slave.  The slave records its hardware departure
   timestamp locally as t3.

7.6.  Delay_Resp Message (Type 0x13)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       PTP Sequence            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |          Receive Timestamp (64 bits, ns)                      |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                      Requester UID                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 7: PTP Delay_Resp Message

   Receive Timestamp is the hardware-captured arrival
   time of the Delay_Req at the grandmaster (t4).

   The slave computes offset and delay:

      offset = ((t2 - t1) - (t4 - t3)) / 2
      delay  = ((t2 - t1) + (t4 - t3)) / 2

   Where t1 = Precise Origin (Follow_Up),
   t2 = Sync arrival (local HW timestamp),
   t3 = Delay_Req departure (local HW timestamp),
   t4 = Receive Timestamp (Delay_Resp).

   Implementations SHOULD filter offset and delay
   using an exponential moving average with a
   coefficient of 1/16.

7.7.  Media Clock Recovery

   Receivers MUST derive their audio sample clock
   from the PTP time base.  Implementations SHOULD
   use a PLL or NCO to generate a low-jitter sample
   clock locked to the PTP reference.

   Audio packets carry presentation timestamps in the
   PTP time base (Section 10.4).

7.8.  Hardware Timestamping

   Conformant implementations MUST capture timestamps
   at the MAC/PHY boundary for PTP event messages
   (Sync and Delay_Req).

   Timestamp resolution MUST be 10 nanoseconds or
   better.  A resolution of 1 nanosecond is
   RECOMMENDED.

   The timestamping mechanism MUST NOT introduce more
   than 100 nanoseconds of non-deterministic error.


8.  Node Discovery

8.1.  Beacon Format (Type 0x02)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          Node UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Name Len    |    HW Type    |Talker Streams |Listener Slots |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |PTP Priority   |PTP Clock Class|           Flags               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Node Name (UTF-8)                       ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 8: Beacon Message

   Name Len:  Length of the node name in octets.
      Maximum 31.

   HW Type:  See Section 8.4.

   Talker Streams:  Number of streams this node
      currently publishes.

   Listener Slots:  Number of additional streams this
      node can subscribe to.

   Flags:
      Bit 0:  Node is current grandmaster.
      Bits 1-15:  Reserved.  MUST be zero.

8.2.  Beacon Timing

   Beacons MUST be transmitted every 1000 milliseconds,
   with a jitter tolerance of +/- 100 milliseconds.

   Implementations SHOULD randomize the initial beacon
   transmission time within the first 1000 milliseconds
   after startup to avoid synchronization of beacon
   transmissions across simultaneously booting nodes.

8.3.  Node Identification

   The Node UID SHOULD be derived from the Ethernet
   MAC address:

      UID = (MAC[2] << 24) | (MAC[3] << 16)
          | (MAC[4] << 8)  | MAC[5]

   The Node Name is a UTF-8 string.  Implementations
   SHOULD include a model identifier and a suffix
   derived from the MAC address.

8.4.  Hardware Types

   +-------+---------------------------------+
   | Value | Description                     |
   +-------+---------------------------------+
   |     0 | Generic                         |
   |     1 | Speaker / output device         |
   |     2 | Microphone / input device       |
   |     3 | Speaker + Microphone            |
   |     4 | Analog bridge (ADC/DAC)         |
   |     5 | Digital bridge (format conv.)   |
   |     6 | Mixer / DSP                     |
   |     7 | Recording device                |
   | 8-254 | Unassigned                      |
   |   255 | Vendor-specific                 |
   +-------+---------------------------------+

            Table 6: Hardware Type Values

8.5.  Node Expiry

   A node MUST be considered offline if no beacon
   has been received within 3000 milliseconds.

   Upon expiry, the implementation MUST:

   1.  Remove the node from the local node table.
   2.  Notify the application.
   3.  If the node was grandmaster, re-run BMC
       (Section 7.2).
   4.  Release any subscriptions to streams
       from the expired node.


9.  Stream Management

9.1.  Stream Announcement (Type 0x05)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |   Channels    |  Bit Depth    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sample Rate                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    Packet Interval (us)       |   Encoding    |  Name Len     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Stream Name (UTF-8)                     ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   Channel Labels (TLV)                     ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 9: Stream Announcement

   A talker MUST transmit a stream announcement for
   each published stream at least once per beacon
   interval (1000 ms).

   Encoding values:
      0 = Linear PCM.
      1-255: Reserved.

9.2.  Channel Labels

   Channel labels follow the stream name as a
   sequence of length-prefixed UTF-8 strings:

      [length: 1 octet] [label: length octets]

   One entry per channel, in channel order.  A
   length of 0 indicates an unnamed channel.

9.3.  Stream Subscription (Type 0x03)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Talker UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Ch Mask Len   |   Channel Bitmask (variable)               ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 10: Stream Subscription

   Ch Mask Len:  Length of the bitmask in octets.
      0 = subscribe to all channels.

   Channel Bitmask:  Bit N = 1 subscribes to
      channel N.  LSB of first octet is channel 0.

   Talkers MUST transmit audio to the stream
   multicast group regardless of subscriptions.
   This enables stateless multicast operation.

   Unsubscription (Type 0x04) uses the same format.
   The bitmask is ignored for unsubscription.

9.4.  Dynamic Channel Count

   A talker MAY change the channel count by
   sending a new announcement with the updated
   count.

   On channel addition, listeners MUST initialize
   new channels to silence.  On channel removal,
   listeners MUST cease reading removed channels.

   Stream ID, sample rate, and bit depth MUST NOT
   change.  To change these, the talker MUST delete
   and recreate the stream.

9.5.  Stream Teardown (Type 0x06)

   Payload: Stream ID (2 octets).

   Listeners MUST release all resources for the
   stream within one beacon interval.


10.  Audio Data Transport

10.1.  Audio Packet Format (Type 0x01)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |   Channels    |  Bit Depth    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sample Rate                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Samples per Channel       |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |           Presentation Timestamp (64 bits, ns)                |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                   Audio Sample Data                           |
   |                       (variable)                              |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 11: Audio Packet

   The audio header is 20 octets.

10.2.  Sample Encoding

   Samples are signed two's complement integers in
   big-endian byte order:

   +----------+--------+---------------------------+
   | Depth    | Octets | Range                     |
   +----------+--------+---------------------------+
   | 16-bit   |      2 | -32768 to 32767           |
   | 24-bit   |      3 | -8388608 to 8388607       |
   | 32-bit   |      4 | -2147483648 to 2147483647 |
   +----------+--------+---------------------------+

            Table 7: Sample Encoding

10.3.  Sample Interleaving

   Samples are interleaved by channel, then by
   frame:

      S(0,0) S(0,1) ... S(0,C-1)
      S(1,0) S(1,1) ... S(1,C-1)
      ...
      S(N-1,0) S(N-1,1) ... S(N-1,C-1)

   Where S(f,c) is frame f, channel c.  C is the
   channel count.  N is samples per channel.

   Total audio data:

      C * N * (bit_depth / 8) octets

10.4.  Presentation Timestamps

   The Presentation Timestamp is a signed 64-bit
   integer in nanoseconds, referenced to the PTP
   time base.

   The talker computes:

      PTS = current_PTP_time + presentation_latency

   Where presentation_latency is configurable
   (default: 2,000,000 ns = 2 ms).

   All listeners MUST begin playout of the same
   samples at the same PTP-referenced instant.

   A listener MUST buffer audio and release it
   only when the local PTP clock reaches the
   presentation timestamp.

   Packets arriving after their presentation time
   MUST be counted as late and SHOULD be discarded.

10.5.  Packet Interval

   +------------+-----------+-----------------------+
   | Interval   | Samples   | Use Case              |
   | (us)       | at 48 kHz |                       |
   +------------+-----------+-----------------------+
   |        125 |         6 | Ultra-low latency     |
   |        250 |        12 | Low latency           |
   |        500 |        24 | Balanced              |
   |       1000 |        48 | Default               |
   |       2000 |        96 | High efficiency       |
   |       4000 |       192 | Maximum efficiency    |
   +------------+-----------+-----------------------+

            Table 8: Packet Intervals

   Implementations MUST support 1000 us.  Support
   for other values is RECOMMENDED.

10.6.  Sequence Numbering

   The common header sequence number increments per
   audio packet per stream.  Wraps from 65535 to 0.

   Listeners SHOULD track expected sequence numbers
   and report gaps as loss.

   Loss ratio:

      loss = lost / (received + lost)

10.7.  Playout Buffer

   The buffer depth MUST be at least:

      depth >= pres_latency - min_network_delay

   Implementations SHOULD dynamically adjust depth
   based on observed jitter.

10.8.  Packet Loss Concealment

   On loss (sequence gap), listeners SHOULD conceal:

   -  Zero insertion (silence)
   -  Repetition of last valid frame
   -  Linear interpolation

   The strategy is implementation-defined.


11.  Control Tunneling

11.1.  Tunnel Packet Format (Type 0x20)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Target UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Tunnel Type  |  Tunnel Len   |    Payload                 ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 12: Tunnel Packet

   Target UID:  0x00000000 = broadcast.

   +-------+----------+------------------------------+
   | Value | Name     | Section                      |
   +-------+----------+------------------------------+
   |     0 | SPI      | 11.4                         |
   |     1 | I2C      | 11.5                         |
   |     2 | GPIO     | 11.2                         |
   |     3 | MIDI     | 11.3                         |
   |     4 | SIDEBAND | 11.6                         |
   | 5-15  | Reserved |                              |
   +-------+----------+------------------------------+

            Table 9: Tunnel Type Values

11.2.  GPIO Tunnel (Type 2)

   Payload: 2 octets.

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        Pin States (16 bits)   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         Figure 13: GPIO Tunnel Payload

   Bit N = pin N.  1 = HIGH, 0 = LOW.

   GPIO packets are idempotent.  The receiver MUST
   apply the full 16-bit state on each reception.

11.3.  MIDI Tunnel (Type 3)

   Payload: 1 to 3 octets of raw MIDI data.

    0
    0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+
   | Status        |
   +-+-+-+-+-+-+-+-+
   | Data 1        |  (OPTIONAL)
   +-+-+-+-+-+-+-+-+
   | Data 2        |  (OPTIONAL)
   +-+-+-+-+-+-+-+-+

         Figure 14: MIDI Tunnel Payload

   System Exclusive messages exceeding 3 octets
   MUST be fragmented.  The first fragment starts
   with 0xF0; the last ends with 0xF7.

   At 1 ms packet interval, throughput is
   3000 octets/sec, approximately 10x standard
   MIDI 1.0 (3125 octets/sec at 31250 baud).

11.4.  SPI Tunnel (Type 0)

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   CS Pin      |   SPI Mode    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            SPI Data        ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         Figure 15: SPI Tunnel Payload

   CS Pin:  Chip-select index (0-255).
   SPI Mode:  0-3 (CPOL/CPHA).

   Response data MAY be returned via a reverse
   tunnel packet.

11.5.  I2C Tunnel (Type 1)

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  I2C Address  |    Flags      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            I2C Data        ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         Figure 16: I2C Tunnel Payload

   I2C Address:  7-bit address in bits 6:0.
      Bit 7 MUST be zero.

   Flags:
      Bit 0: 1 = Read, 0 = Write.
      Bits 1-7: Reserved.  MUST be zero.

   For reads, the data field contains the register
   address.  Response is returned via reverse tunnel.

11.6.  Sideband Channel (Type 4)

   Payload: 1 octet of arbitrary data per
   direction per frame.

   Typical use: MIDI clock tick, sync trigger,
   or 1-bit flags.


12.  Metadata

12.1.  Metadata Packet Format (Type 0x30)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |  Num Entries  |   Reserved    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   Metadata Entries                          ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 17: Metadata Packet

   Stream ID 0x0000 = node-level metadata.

12.2.  Metadata Entry Encoding

   Each entry:

      [key_length: 1 octet]
      [value_length: 2 octets]
      [key: key_length octets, UTF-8]
      [value: value_length octets, UTF-8]

   Maximum key length: 255 octets.
   Maximum value length: 65535 octets.

12.3.  Standard Metadata Keys

   +------------------+----------------------------------+
   | Key              | Description                      |
   +------------------+----------------------------------+
   | "vendor"         | Manufacturer name                |
   | "model"          | Model identifier                 |
   | "firmware"       | Firmware version                 |
   | "serial"         | Serial number                    |
   | "location"       | Physical location                |
   | "purpose"        | Intended use                     |
   | "icon"           | URL or data URI for icon         |
   +------------------+----------------------------------+

            Table 10: Standard Metadata Keys

   Vendor-specific keys MUST use reverse-domain
   notation (e.g., "com.example.custom-key").


13.  Latency Measurement

13.1.  Ping Message (Type 0x40)

   Payload: 8 octets.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |             Sender Timestamp (64 bits, ns)                    |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 18: Ping Message

   The Sender Timestamp is the sender's PTP time at
   transmission.

13.2.  Pong Message (Type 0x41)

   Payload: exact echo of the Ping payload (8 octets).

   The receiver MUST transmit the Pong within
   1 millisecond of receiving the Ping.

13.3.  Round-Trip Computation

   The sender computes:

      RTT = current_PTP_time - echoed_timestamp

   Estimated one-way (symmetric path assumption):

      one_way = RTT / 2

   For asymmetric paths, use PTP path delay instead.

13.4.  Jitter Estimation

   Listeners SHOULD compute interarrival jitter per
   [RFC3550] Section 6.4.1:

      D(i,j) = (Rj - Ri) - (Sj - Si)

   Where R = arrival time, S = presentation time.

      J(i) = J(i-1) + (|D(i,j)| - J(i-1)) / 16

   Implementations SHOULD also track peak, minimum,
   and maximum jitter, and packet loss count, loss
   ratio, and late packet count.


14.  Error Handling

14.1.  Unknown Packet Types

   A receiver MUST silently discard packets with
   unrecognized Pkt Type values.  The receiver
   MUST NOT close connections, reset state, or send
   error responses.

14.2.  Version Mismatch

   A receiver MUST silently discard packets with a
   Version field that does not match any version it
   supports.

14.3.  CRC Failure

   (LVDS only) Frames with CRC-32 mismatch MUST be
   discarded entirely.  Implementations MUST count
   CRC errors.  If the error rate exceeds 1 per
   1000 frames sustained over 10 seconds, the
   implementation SHOULD report a link quality
   warning.

14.4.  Sequence Gaps

   Listeners MUST track expected sequence numbers.
   A gap indicates one or more lost packets.  The
   listener MUST:

   1.  Increment the lost packet counter by the
       gap size.
   2.  Apply packet loss concealment (Section 10.8).
   3.  Continue processing from the new sequence
       number.

14.5.  Late Packets

   Audio packets arriving after their presentation
   timestamp MUST be counted in late-packet
   statistics.  The audio data SHOULD be discarded.

   If more than 10% of packets in any 1-second
   window are late, the implementation SHOULD
   report a latency warning to the application.

14.6.  Grandmaster Loss

   If no PTP Sync is received for 3 * Sync interval
   (375 ms), the node MUST:

   1.  Declare the grandmaster unreachable.
   2.  Re-run the BMC algorithm (Section 7.2).
   3.  If this node becomes grandmaster, begin
       transmitting Sync within 125 ms.
   4.  Continue audio playout using the last known
       clock offset (holdover mode).

   Holdover accuracy depends on the local oscillator.
   At 50 ppm, drift is approximately 50 microseconds
   per second.


15.  Extensibility and Versioning

15.1.  Version Field

   The Version field in the common header enables
   future protocol revisions.  A receiver supporting
   version N MAY also support version N-1 for
   backwards compatibility, but this is OPTIONAL.

   When a node receives a packet with a higher
   version than it supports, it MUST silently
   discard it.

15.2.  Reserved Fields

   All fields marked "Reserved" in this document
   MUST be set to zero on transmission and MUST be
   ignored on reception.  This ensures forward
   compatibility as reserved fields are assigned
   meaning in future versions.

15.3.  Private-Use Ranges

   Packet types 0x50 through 0x7F are designated
   for private use.  Implementations MAY use these
   for vendor-specific extensions.  Private-use
   packets MUST still carry a valid common header.

   Tunnel types 5 through 15 are available for
   future standardization or private use.


16.  Security Considerations

16.1.  Threat Model

   AudioBus is designed for trusted local-area
   networks.  The primary threats are:

   -  Unauthorized injection of audio or control data
   -  Eavesdropping on audio content
   -  Denial of service via packet flooding
   -  Clock disruption via rogue PTP messages
   -  Unauthorized remote control via tunnel packets

16.2.  Confidentiality

   AudioBus transmits all data in cleartext.  Audio
   content and control data are visible to any device
   on the same Layer 2 segment.

   Deployments requiring confidentiality SHOULD use
   MACsec [IEEE802.1AE] or physically isolated
   networks.

16.3.  Integrity

   The LVDS transport includes CRC-32 for error
   detection.  The Ethernet transport relies on
   the Ethernet FCS.

   Neither mechanism provides cryptographic integrity.
   An attacker on the same network segment can forge
   or modify packets.

   Deployments requiring integrity SHOULD use MACsec
   or a future AudioBus security extension.

16.4.  Availability

   An attacker can disrupt audio by:

   -  Flooding the network with audio packets,
      exhausting switch bandwidth.
   -  Injecting PTP Sync messages to disrupt clock
      synchronization.
   -  Sending STREAM_DELETE to tear down active
      streams.

   Mitigations:

   -  Use dedicated VLANs for AudioBus traffic.
   -  Use IEEE 802.1X port-based access control.
   -  Rate-limit AudioBus multicast groups at the
      switch.
   -  Implementations SHOULD validate that PTP Sync
      messages originate from the expected grandmaster
      UID before processing.

16.5.  Authentication

   AudioBus does not define an authentication
   mechanism.  All nodes on the network are
   implicitly trusted.

   Future versions MAY define:

   -  HMAC-SHA256 authentication of the common header
   -  Node certificate-based authentication
   -  Authenticated PTP (Annex P of IEEE 1588-2019)

16.6.  Mitigations

   Deployments in environments with untrusted devices
   SHOULD implement the following:

   1.  Physical network isolation or VLAN separation.
   2.  IEEE 802.1X port-based network access control.
   3.  MACsec (IEEE 802.1AE) for link-layer
       encryption and integrity.
   4.  Grandmaster UID whitelisting: only accept
       PTP Sync from known node UIDs.
   5.  Tunnel target validation: only process tunnel
       packets addressed to the local UID or
       broadcast.

   Implementations MUST NOT be used in safety-
   critical systems without additional integrity
   verification.


17.  IANA Considerations

17.1.  EtherType Assignment

   This document requests assignment of an EtherType
   from the IEEE Registration Authority for
   "AudioBus Protocol."  Pending assignment, the
   locally administered value 0x88B6 is used.

17.2.  Multicast OUI Assignment

   This document requests assignment of an Ethernet
   multicast OUI from the IEEE Registration Authority.
   Pending assignment, the locally administered prefix
   01:60:AB is used.

17.3.  AudioBus Packet Type Registry

   IANA is requested to create the "AudioBus Packet
   Types" registry with the initial values in
   Table 1.

   Registration policy:

   -  0x00:        Reserved.
   -  0x01-0x4F:   Standards Action.
   -  0x50-0x7F:   Private Use.
   -  0x80-0xFF:   Reserved.

17.4.  AudioBus Tunnel Type Registry

   IANA is requested to create the "AudioBus Tunnel
   Types" registry with the initial values in
   Table 9.

   Registration policy:

   -  0-4:    Standards Action.
   -  5-15:   Expert Review.

17.5.  AudioBus Hardware Type Registry

   IANA is requested to create the "AudioBus
   Hardware Types" registry with the initial values
   in Table 6.

   Registration policy:

   -  0-7:    Standards Action.
   -  8-254:  Specification Required.
   -  255:    Private Use.

17.6.  AudioBus Metadata Key Registry

   IANA is requested to create the "AudioBus
   Metadata Keys" registry with the initial values
   in Table 10.

   Registration policy: First Come First Served.

   Vendor-specific keys MUST use reverse-domain
   notation and do not require registration.


18.  References

18.1.  Normative References

   [RFC2119]  Bradner, S., "Key words for use in RFCs to
              Indicate Requirement Levels", BCP 14,
              RFC 2119, DOI 10.17487/RFC2119,
              March 1997,
              <https://www.rfc-editor.org/info/rfc2119>.

   [RFC8174]  Leiba, B., "Ambiguity of Uppercase vs
              Lowercase in RFC 2119 Key Words", BCP 14,
              RFC 8174, DOI 10.17487/RFC8174, May 2017,
              <https://www.rfc-editor.org/info/rfc8174>.

   [RFC791]   Postel, J., "Internet Protocol", STD 5,
              RFC 791, DOI 10.17487/RFC0791,
              September 1981,
              <https://www.rfc-editor.org/info/rfc791>.

   [IEEE802.3]
              IEEE, "IEEE Standard for Ethernet",
              IEEE Std 802.3-2022.

   [IEEE1588]
              IEEE, "IEEE Standard for a Precision
              Clock Synchronization Protocol for
              Networked Measurement and Control Systems",
              IEEE Std 1588-2019.

18.2.  Informative References

   [RFC3550]  Schulzrinne, H., Casner, S., Frederick, R.,
              and V. Jacobson, "RTP: A Transport Protocol
              for Real-Time Applications", STD 64,
              RFC 3550, DOI 10.17487/RFC3550, July 2003,
              <https://www.rfc-editor.org/info/rfc3550>.

   [RFC3552]  Rescorla, E. and B. Korver, "Guidelines
              for Writing RFC Text on Security
              Considerations", BCP 72, RFC 3552,
              DOI 10.17487/RFC3552, July 2003,
              <https://www.rfc-editor.org/info/rfc3552>.

   [RFC8126]  Cotton, M., Leiba, B., and T. Narten,
              "Guidelines for Writing an IANA
              Considerations Section in RFCs", BCP 26,
              RFC 8126, DOI 10.17487/RFC8126, June 2017,
              <https://www.rfc-editor.org/info/rfc8126>.

   [AES67]    Audio Engineering Society, "AES67-2018:
              AES standard for audio applications of
              networks - High-performance streaming
              audio-over-IP interoperability", 2018.

   [TIA644]   TIA, "TIA/EIA-644: Electrical
              Characteristics of Low Voltage
              Differential Signaling (LVDS) Interface
              Circuits", 2001.

   [IEEE802.1AE]
              IEEE, "IEEE Standard for Local and
              Metropolitan Area Networks: Media Access
              Control (MAC) Security",
              IEEE Std 802.1AE-2018.


Appendix A.  Channel Capacity Tables

A.1.  Ethernet Transport (100 Mbps)

   +----------+---------+----------+--------------------+
   | Rate(Hz) | Depth   | Interval | Max Channels       |
   +----------+---------+----------+--------------------+
   |    48000 | 32-bit  | 1000 us  | 64                 |
   |    48000 | 24-bit  | 1000 us  | 86                 |
   |    96000 | 32-bit  | 1000 us  | 32                 |
   |    96000 | 24-bit  | 1000 us  | 43                 |
   +----------+---------+----------+--------------------+

            Table A-1: Ethernet Capacity

A.2.  LVDS Transport (491 Mbps, per direction)

   +----------+---------+----------+--------------------+
   | Rate(Hz) | Depth   | Frame    | Max Ch/Direction   |
   +----------+---------+----------+--------------------+
   |    48000 | 32-bit  | 1024 B   | 64                 |
   |    48000 | 24-bit  | 1024 B   | 64                 |
   |    96000 | 32-bit  |  512 B   | 62                 |
   |    96000 | 24-bit  |  512 B   | 64                 |
   |    44100 | 32-bit  | 1024 B   | 64                 |
   +----------+---------+----------+--------------------+

            Table A-2: LVDS Capacity


Appendix B.  Recommended Hardware

B.1.  Ethernet Transport

   Microcontroller: Espressif ESP32-P4 (dual RISC-V
   400 MHz, EMAC with IEEE 1588 HW timestamping).
   PHY: IP101GRI, RTL8201, or LAN8720 (100BASE-TX
   RMII).  Switch: any commodity 100 Mbps Ethernet
   switch.

B.2.  LVDS Transport (Single-Chip)

   Transceiver: TI SN65LVDT41 (single LVDS
   transceiver, driver + receiver, ~$2).  Clock:
   Si5351A programmable clock generator.  Cable:
   Cat5e, single pair.  Termination: 100 ohm
   differential.

B.3.  LVDS Transport (Maximum Performance)

   Serializer: TI DS92LV1021A (10:1 LVDS).
   Deserializer: TI DS92LV1212A (1:10 LVDS, CDR).
   Clock: 49.152 MHz crystal oscillator (master).
   Cable: Cat5e, single pair.  Termination: 100 ohm
   differential.


Appendix C.  Example Message Exchange

   The following illustrates a typical Ethernet-mode
   startup sequence between two nodes A and B on
   the same switch.

   T=0.0s  A boots.
           A joins multicast 01:60:AB:FF:FF:00.
           A joins multicast 01:60:AB:FF:FF:01.
           A sends BEACON (uid=0x1234, pri=128,
              class=248, name="Node-A").
           A assumes grandmaster (only node).
           A begins PTP Sync at 125 ms intervals.

   T=0.2s  B boots.
           B joins multicast groups.
           B sends BEACON (uid=0x5678, pri=128,
              class=248, name="Node-B").

   T=0.2s  A receives B's beacon.
              BMC: 0x1234 < 0x5678, A remains GM.
           B receives A's beacon.
              BMC: 0x1234 < 0x5678, B accepts A as GM.

   T=0.3s  A sends PTP Sync (seq=1).
           A sends PTP Follow_Up (seq=1, precise_ts).
           B receives Sync, records t2.
           B receives Follow_Up, records t1.

   T=0.5s  B sends Delay_Req (seq=1), records t3.
           A receives Delay_Req, records t4.
           A sends Delay_Resp (seq=1, t4, uid=0x5678).
           B computes offset and delay.

   T=1.0s  A creates stream "Node-A Stereo"
              (id=0x1230, 2ch, 48kHz, 32-bit).
           A sends STREAM_ANNOUNCE.
           A joins 01:60:AB:12:30:00.
           A begins sending AUDIO packets.

   T=1.0s  B receives stream announcement.
           B sends SUBSCRIBE (stream=0x1230,
              talker=0x1234, mask=0 (all ch)).
           B joins 01:60:AB:12:30:00.

   T=1.001s B receives first AUDIO packet.
            B buffers audio, waits for PTS.

   T=1.003s PTS reached.  B begins playout.

            Figure C-1: Example Startup Sequence


Acknowledgements

   The authors thank the open-source audio and
   embedded systems communities for their ongoing
   contributions to accessible audio networking
   technology.


Authors' Addresses

   Sylwester Sosnowski
   DatanoiseTV
   Email: tbd@datanoise.tv
```
