```
AudioBus Protocol Specification                              S. Sosnowski
Internet-Draft                                               DatanoiseTV
Intended status: Standards Track                            4 April 2026
Expires: 4 October 2026


         AudioBus: A Multiplexed Audio and Control Transport
              Protocol over Ethernet and LVDS Links

Abstract

   AudioBus is a deterministic, low-latency protocol for transporting
   multi-channel digital audio, MIDI, and control data (SPI, I2C, GPIO)
   over standard Ethernet networks or dedicated LVDS serial links.

   The protocol provides sample-accurate clock synchronization across
   all participating nodes using IEEE 1588 Precision Time Protocol
   (PTP) with hardware timestamping, enabling jitter-free audio
   playout at configurable latencies as low as 125 microseconds.

   AudioBus supports plug-and-play operation with automatic node
   discovery, dynamic stream creation, per-channel metadata, and
   transparent coexistence with standard IP traffic on shared
   Ethernet infrastructure.

Status of This Memo

   This Internet-Draft is submitted in full conformance with the
   provisions of BCP 78 and BCP 79.

   Internet-Drafts are working documents of the Internet Engineering
   Task Force (IETF).  Note that other groups may also distribute
   working documents as Internet-Drafts.  The list of current
   Internet-Drafts is at https://datatracker.ietf.org/drafts/current/.

   Internet-Drafts are draft documents valid for a maximum of six
   months and may be updated, replaced, or obsoleted by other documents
   at any time.  It is inappropriate to use Internet-Drafts as
   reference material or to cite them other than as "work in progress."

   This Internet-Draft will expire on 4 October 2026.

Copyright Notice

   Copyright (c) 2026 IETF Trust and the persons identified as the
   document authors.  All rights reserved.

   This document is subject to BCP 78 and the IETF Trust's Legal
   Provisions Relating to IETF Documents
   (https://trustee.ietf.org/license-info) in effect on the date of
   publication of this document.  Please review these documents
   carefully, as they describe your rights and restrictions with
   respect to this document.


Table of Contents

   1.  Introduction  . . . . . . . . . . . . . . . . . . . . . .   3
     1.1.  Motivation  . . . . . . . . . . . . . . . . . . . . .   3
     1.2.  Design Goals  . . . . . . . . . . . . . . . . . . . .   4
     1.3.  Scope . . . . . . . . . . . . . . . . . . . . . . . .   4
   2.  Terminology . . . . . . . . . . . . . . . . . . . . . . .   5
   3.  Protocol Architecture . . . . . . . . . . . . . . . . . .   6
     3.1.  Layer Model . . . . . . . . . . . . . . . . . . . . .   6
     3.2.  Transport Modes . . . . . . . . . . . . . . . . . . .   7
     3.3.  Node Roles  . . . . . . . . . . . . . . . . . . . . .   7
     3.4.  Stream Model  . . . . . . . . . . . . . . . . . . . .   8
   4.  Common Packet Format  . . . . . . . . . . . . . . . . . .   9
     4.1.  AudioBus Common Header  . . . . . . . . . . . . . . .   9
     4.2.  Packet Types  . . . . . . . . . . . . . . . . . . . .  10
     4.3.  Byte Ordering . . . . . . . . . . . . . . . . . . . .  11
   5.  Ethernet Transport  . . . . . . . . . . . . . . . . . . .  12
     5.1.  Ethernet Frame Encapsulation  . . . . . . . . . . . .  12
     5.2.  EtherType . . . . . . . . . . . . . . . . . . . . . .  12
     5.3.  Multicast Addressing  . . . . . . . . . . . . . . . .  12
     5.4.  VLAN Tagging  . . . . . . . . . . . . . . . . . . . .  13
     5.5.  Quality of Service  . . . . . . . . . . . . . . . . .  13
     5.6.  Coexistence with IP Traffic . . . . . . . . . . . . .  14
   6.  LVDS Serial Transport . . . . . . . . . . . . . . . . . .  15
     6.1.  Physical Layer  . . . . . . . . . . . . . . . . . . .  15
     6.2.  8b10b Line Coding . . . . . . . . . . . . . . . . . .  15
     6.3.  TDM Frame Structure . . . . . . . . . . . . . . . . .  16
     6.4.  Slot Map  . . . . . . . . . . . . . . . . . . . . . .  17
     6.5.  Half-Duplex Direction Switching . . . . . . . . . . .  17
     6.6.  Daisy-Chain Forwarding  . . . . . . . . . . . . . . .  18
   7.  Clock Synchronization . . . . . . . . . . . . . . . . . .  19
     7.1.  PTP Profile for AudioBus  . . . . . . . . . . . . . .  19
     7.2.  Grandmaster Election  . . . . . . . . . . . . . . . .  20
     7.3.  Synchronization Messages  . . . . . . . . . . . . . .  21
     7.4.  Media Clock Recovery  . . . . . . . . . . . . . . . .  22
     7.5.  Hardware Timestamping Requirements  . . . . . . . . .  22
   8.  Node Discovery  . . . . . . . . . . . . . . . . . . . . .  23
     8.1.  Beacon Message  . . . . . . . . . . . . . . . . . . .  23
     8.2.  Beacon Interval and Expiry  . . . . . . . . . . . . .  24
     8.3.  Node Identification . . . . . . . . . . . . . . . . .  24
   9.  Stream Management . . . . . . . . . . . . . . . . . . . .  25
     9.1.  Stream Announcement . . . . . . . . . . . . . . . . .  25
     9.2.  Stream Subscription . . . . . . . . . . . . . . . . .  26
     9.3.  Dynamic Channel Count . . . . . . . . . . . . . . . .  27
     9.4.  Stream Teardown . . . . . . . . . . . . . . . . . . .  27
   10. Audio Data Transport  . . . . . . . . . . . . . . . . . .  28
     10.1. Audio Packet Format . . . . . . . . . . . . . . . . .  28
     10.2. Sample Encoding . . . . . . . . . . . . . . . . . . .  29
     10.3. Presentation Timestamps . . . . . . . . . . . . . . .  30
     10.4. Packet Interval . . . . . . . . . . . . . . . . . . .  31
     10.5. Sequence Numbering  . . . . . . . . . . . . . . . . .  31
     10.6. Playout Buffer  . . . . . . . . . . . . . . . . . . .  32
     10.7. Packet Loss Concealment . . . . . . . . . . . . . . .  32
   11. Control Tunneling . . . . . . . . . . . . . . . . . . . .  33
     11.1. Tunnel Packet Format  . . . . . . . . . . . . . . . .  33
     11.2. GPIO Tunnel . . . . . . . . . . . . . . . . . . . . .  34
     11.3. MIDI Tunnel . . . . . . . . . . . . . . . . . . . . .  34
     11.4. SPI Tunnel  . . . . . . . . . . . . . . . . . . . . .  35
     11.5. I2C Tunnel  . . . . . . . . . . . . . . . . . . . . .  36
   12. Metadata  . . . . . . . . . . . . . . . . . . . . . . . .  37
     12.1. Metadata Packet Format  . . . . . . . . . . . . . . .  37
     12.2. Standard Metadata Keys  . . . . . . . . . . . . . . .  38
   13. Latency Measurement . . . . . . . . . . . . . . . . . . .  39
     13.1. Ping/Pong Protocol  . . . . . . . . . . . . . . . . .  39
     13.2. Jitter Measurement  . . . . . . . . . . . . . . . . .  39
   14. Security Considerations . . . . . . . . . . . . . . . . .  40
   15. IANA Considerations . . . . . . . . . . . . . . . . . . .  41
   16. References  . . . . . . . . . . . . . . . . . . . . . . .  42
     16.1. Normative References  . . . . . . . . . . . . . . . .  42
     16.2. Informative References  . . . . . . . . . . . . . . .  42
   Appendix A.  Channel Capacity Tables  . . . . . . . . . . . .  43
   Appendix B.  Recommended Hardware . . . . . . . . . . . . . .  44
   Authors' Addresses  . . . . . . . . . . . . . . . . . . . . .  45


1.  Introduction

1.1.  Motivation

   Professional and consumer audio systems increasingly require the
   transport of multiple channels of high-resolution digital audio
   alongside control data (MIDI, GPIO, SPI, I2C) over simple physical
   interconnections.  Existing solutions are either proprietary,
   expensive, limited in channel count, or require specialized
   infrastructure.

   AudioBus addresses these limitations by defining an open protocol
   that operates over two complementary transport modes:

   (a)  Standard IEEE 802.3 Ethernet with no special infrastructure
        requirements — any commodity switch is sufficient; and

   (b)  Dedicated LVDS (Low-Voltage Differential Signaling) serial
        links over a single twisted pair for cost-optimized,
        ultra-low-latency daisy-chain topologies.

   Both transports share a common framing, encoding, and control
   plane, allowing mixed networks where Ethernet-connected nodes
   interoperate with LVDS-connected nodes through bridge devices.

1.2.  Design Goals

   The protocol is designed to satisfy the following requirements:

   G1.  Deterministic latency: Fixed, predictable end-to-end delay
        with no data-dependent variation.

   G2.  Zero jitter: Audio playout synchronized to a common time
        base across all nodes, independent of network topology.

   G3.  Plug-and-play: No manual configuration.  Nodes discover
        each other, negotiate capabilities, and begin streaming
        automatically.

   G4.  Dynamic: Channel counts, sample rates, and stream
        configurations may change at runtime without restarting
        the network.

   G5.  Coexistence: AudioBus Ethernet traffic MUST NOT interfere
        with standard IP traffic sharing the same physical network.

   G6.  Simplicity: The protocol SHOULD be implementable on
        resource-constrained microcontrollers (e.g., 400 MHz
        RISC-V with 512 KB SRAM).

   G7.  Low cost: All required components SHOULD be commodity
        parts available from multiple vendors.

1.3.  Scope

   This specification defines:

   -  The wire format for all AudioBus packets
   -  The Ethernet transport encapsulation
   -  The LVDS serial transport framing
   -  The clock synchronization profile (based on IEEE 1588)
   -  The node discovery protocol
   -  The stream announcement and subscription protocol
   -  The audio sample encoding and packetization
   -  The control tunnel protocols (GPIO, MIDI, SPI, I2C)
   -  The metadata distribution mechanism
   -  The latency measurement protocol

   This specification does not define:

   -  Hardware implementation details (reference designs are
      provided in Appendix B as informative guidance)
   -  Application-layer mixing, routing, or processing logic
   -  Digital rights management or content protection
   -  Wireless transports (these may be defined in future
      companion specifications)


2.  Terminology

   The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",
   "SHOULD", "SHOULD NOT", "RECOMMENDED", "NOT RECOMMENDED", "MAY",
   and "OPTIONAL" in this document are to be interpreted as described
   in BCP 14 [RFC2119] [RFC8174] when, and only when, they appear in
   all capitals, as shown here.

   AudioBus Node:
      Any device that participates in the AudioBus protocol, either
      as a talker, listener, or both.

   Talker:
      A node that publishes one or more audio streams to the network.

   Listener:
      A node that subscribes to and receives one or more audio streams
      from talkers on the network.

   Stream:
      A unidirectional flow of audio data from exactly one talker to
      one or more listeners.  A stream has a fixed sample rate and
      bit depth, and a dynamic channel count.

   Stream ID:
      A 16-bit identifier that uniquely identifies a stream within
      the scope of its talker node.

   Node UID:
      A 32-bit identifier that uniquely identifies a node within an
      AudioBus network.  The UID SHOULD be derived from the node's
      Ethernet MAC address.

   Presentation Timestamp:
      A nanosecond-resolution timestamp, referenced to the PTP time
      base, that indicates the precise instant at which a group of
      audio samples SHOULD begin playout at all listeners.

   Grandmaster:
      The node whose clock serves as the reference for all other
      nodes in the AudioBus network.  The grandmaster is elected
      automatically using the Best Master Clock (BMC) algorithm.

   Packet Interval:
      The time between consecutive audio packets within a stream,
      expressed in microseconds.  This determines the trade-off
      between latency and protocol overhead.

   Slot Map:
      (LVDS transport only) A deterministic assignment of byte
      positions within the TDM frame to specific audio channels
      and tunnel data streams.

   Guard Time:
      (LVDS transport only) A period of idle symbols inserted
      between the downstream and upstream phases of a half-duplex
      TDM frame to allow direction switching and CDR re-lock.


3.  Protocol Architecture

3.1.  Layer Model

   AudioBus is structured in the following layers:

       +--------------------------------------------------+
       |              Application Layer                    |
       |    (audio routing, mixing, UI, user logic)        |
       +--------------------------------------------------+
       |              Stream Management Layer              |
       |    (discovery, announcement, subscription,        |
       |     metadata, latency measurement)                |
       +--------------------------------------------------+
       |              Audio Transport Layer                |
       |    (packetization, presentation timestamps,       |
       |     sequence numbering, playout buffering)        |
       +--------------------------------------------------+
       |              Clock Synchronization Layer          |
       |    (PTP profile, grandmaster election,            |
       |     media clock recovery)                         |
       +--------------------------------------------------+
       |              Control Tunnel Layer                 |
       |    (GPIO, MIDI, SPI, I2C multiplexing)            |
       +--------------------------------------------------+
       |              Framing Layer                        |
       |    (common header, CRC, packetization)            |
       +--------------------------------------------------+
       |              Transport Layer                      |
       |    +-------------------+  +--------------------+  |
       |    | Ethernet (L2)     |  | LVDS Serial (TDM)  |  |
       |    | 802.3 / 802.1Q    |  | 8b10b encoded      |  |
       |    +-------------------+  +--------------------+  |
       +--------------------------------------------------+
       |              Physical Layer                       |
       |    +-------------------+  +--------------------+  |
       |    | 100/1000BASE-T    |  | LVDS over twisted  |  |
       |    | (standard switch) |  | pair (491 Mbps)    |  |
       |    +-------------------+  +--------------------+  |
       +--------------------------------------------------+

3.2.  Transport Modes

   AudioBus defines two transport modes that share a common framing
   and control plane:

   Ethernet Mode:
      Packets are encapsulated in IEEE 802.3 Ethernet frames with a
      dedicated EtherType.  Nodes connect to standard Ethernet
      switches in a star or tree topology.  This mode supports up to
      64 nodes and provides plug-and-play operation.

   LVDS Mode:
      Audio and control data are multiplexed into a synchronous TDM
      frame transmitted over a single twisted pair using LVDS
      signaling with 8b10b line coding.  Nodes are connected in a
      daisy-chain topology.  This mode provides the lowest possible
      latency (~21 microseconds per hop).

   An implementation MAY support one or both transport modes.  A
   bridge node MAY interconnect an Ethernet segment with an LVDS
   daisy chain.

3.3.  Node Roles

   In Ethernet mode, all nodes are peers.  Any node MAY be a talker,
   a listener, or both simultaneously.  The grandmaster role is
   elected automatically and may migrate between nodes.

   In LVDS mode, one node is designated as the bus master (always
   node ID 0), and all other nodes are slaves.  The master generates
   the bus clock and initiates all frame transmissions.  Slave nodes
   respond within their assigned time slots.

3.4.  Stream Model

   A stream is a unidirectional, multi-channel audio flow from a
   single talker to one or more listeners.  Each stream is
   characterized by:

   -  Stream ID (16-bit, unique per talker)
   -  Talker UID (32-bit)
   -  Channel count (1-64, dynamic)
   -  Sample rate (44100, 48000, 88200, or 96000 Hz)
   -  Bit depth (16, 24, or 32 bits per sample)
   -  Packet interval (125-4000 microseconds)
   -  Encoding (PCM, linear)

   A single talker MAY publish multiple concurrent streams with
   different parameters.  A single listener MAY subscribe to
   multiple streams from different talkers.

   Listeners MAY subscribe to a subset of channels within a stream
   using a channel bitmask.


4.  Common Packet Format

4.1.  AudioBus Common Header

   All AudioBus packets, regardless of transport mode, begin with
   a 12-byte common header:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    Version    |   Pkt Type    |             Flags             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          Source UID                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        Sequence Number        |        Payload Length         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Version (8 bits):
      Protocol version.  This specification defines version 1.
      Receivers MUST silently discard packets with an unrecognized
      version number.

   Pkt Type (8 bits):
      Identifies the type of payload that follows this header.
      See Section 4.2.

   Flags (16 bits):

       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |V|  Rsvd |     DSCP      | Rsvd|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

      V (1 bit):  VLAN tag present (Ethernet mode only).

      DSCP (6 bits):  Differentiated Services Code Point value.
         Implementations SHOULD set this to 46 (Expedited
         Forwarding) for audio packets.  Switches that support
         IEEE 802.1p priority mapping SHOULD map DSCP EF to
         the highest priority queue.

   Source UID (32 bits):
      The unique identifier of the node that originated this packet.

   Sequence Number (16 bits):
      Monotonically increasing per-source sequence number.  Wraps
      from 65535 to 0.  Receivers use this to detect packet loss
      and reordering.

   Payload Length (16 bits):
      Length of the payload following this header, in bytes.  Does
      not include the 12-byte header itself.

4.2.  Packet Types

   The following packet types are defined:

   +--------+-------------------+-----------------------------------+
   | Value  | Name              | Description                       |
   +--------+-------------------+-----------------------------------+
   | 0x01   | AUDIO             | Audio sample data                 |
   | 0x02   | BEACON            | Node discovery beacon             |
   | 0x03   | SUBSCRIBE         | Stream subscription request       |
   | 0x04   | UNSUBSCRIBE       | Stream unsubscription             |
   | 0x05   | STREAM_ANNOUNCE   | Stream parameter announcement     |
   | 0x06   | STREAM_DELETE     | Stream removal notification       |
   | 0x10   | PTP_SYNC          | PTP Sync message                  |
   | 0x11   | PTP_FOLLOW_UP     | PTP Follow_Up message             |
   | 0x12   | PTP_DELAY_REQ     | PTP Delay_Req message             |
   | 0x13   | PTP_DELAY_RESP    | PTP Delay_Resp message            |
   | 0x20   | TUNNEL            | Control tunnel data               |
   | 0x30   | METADATA          | Key-value metadata                |
   | 0x40   | PING              | Latency measurement request       |
   | 0x41   | PONG              | Latency measurement response      |
   +--------+-------------------+-----------------------------------+

   Values 0x00 and 0x80-0xFF are reserved.  Values 0x50-0x7F are
   available for private/experimental use.

   Receivers MUST silently discard packets with unrecognized types.

4.3.  Byte Ordering

   All multi-byte integer fields in AudioBus packets are encoded
   in network byte order (big-endian), as per Internet convention.

   Audio sample data is encoded in big-endian byte order (most
   significant byte first) within each sample word.


5.  Ethernet Transport

5.1.  Ethernet Frame Encapsulation

   In Ethernet mode, AudioBus packets are encapsulated directly in
   IEEE 802.3 Ethernet frames:

   +-----------------+
   | Destination MAC |  6 bytes
   +-----------------+
   | Source MAC      |  6 bytes
   +-----------------+
   | [802.1Q Tag]    |  4 bytes (optional)
   +-----------------+
   | EtherType       |  2 bytes (0x88B6)
   +-----------------+
   | AudioBus Header |  12 bytes (Section 4.1)
   +-----------------+
   | Payload         |  Variable
   +-----------------+
   | FCS             |  4 bytes (computed by MAC hardware)
   +-----------------+

   Multiple AudioBus packets MUST NOT be concatenated within a
   single Ethernet frame.

5.2.  EtherType

   AudioBus uses EtherType 0x88B6, which is designated for
   Local Experimental use by IEEE 802.

   Note: Prior to ratification, implementations SHOULD use
   0x88B6.  A dedicated EtherType will be requested from
   IEEE upon standardization.

5.3.  Multicast Addressing

   AudioBus uses locally administered multicast MAC addresses
   with the OUI prefix 01:60:AB:

   Discovery and Control:
      01:60:AB:FF:FF:00
      Used for beacon, subscribe, unsubscribe, tunnel, metadata,
      ping, and pong packets.

   PTP:
      01:60:AB:FF:FF:01
      Used for all PTP synchronization messages.

   Audio Streams:
      01:60:AB:SS:SS:00
      Where SS:SS is the 16-bit stream ID.  Each stream has a
      unique multicast group.

   Implementations MUST join the discovery and PTP multicast
   groups upon initialization.  Implementations MUST join
   stream-specific multicast groups upon subscription and
   leave them upon unsubscription.

   Note: The use of locally administered multicast addresses
   ensures no conflict with globally assigned IEEE OUIs.

5.4.  VLAN Tagging

   Implementations MAY use IEEE 802.1Q VLAN tagging to isolate
   AudioBus traffic from other network traffic.

   When VLAN tagging is used:

   -  The VLAN ID MUST be configurable (default: none).
   -  The Priority Code Point (PCP) SHOULD be set to 6
      (corresponding to network control or voice traffic).
   -  All AudioBus nodes on the same network MUST use the same
      VLAN ID.

5.5.  Quality of Service

   AudioBus implementations SHOULD mark packets with DSCP values
   appropriate for real-time audio:

   -  Audio packets (0x01): DSCP 46 (EF, Expedited Forwarding)
   -  PTP packets (0x10-0x13): DSCP 48 (CS6, Network Control)
   -  Discovery/Control packets: DSCP 0 (Best Effort)
   -  Tunnel packets: DSCP 34 (AF41)

   These markings allow QoS-aware switches to prioritize audio
   and timing traffic over bulk data.

   Implementations MUST function correctly on networks that do
   not support QoS marking (i.e., best-effort networks).

5.6.  Coexistence with IP Traffic

   AudioBus operates at Layer 2 using a dedicated EtherType that
   is distinct from IPv4 (0x0800), IPv6 (0x86DD), ARP (0x0806),
   and all other standard EtherTypes.

   An AudioBus implementation MUST NOT interfere with the
   operation of IP protocol stacks on the same network interface.
   Both AudioBus and IP traffic MAY share the same physical
   Ethernet port and switch infrastructure simultaneously.

   Implementations SHOULD self-police their bandwidth usage by
   limiting the aggregate audio packet rate to the declared
   stream parameters.  The theoretical maximum bandwidth consumed
   by a single stream is:

      BW = (channels * bit_depth/8 * sample_rate) +
           (sample_rate / samples_per_packet) *
           (14 + 12 + audio_header_size) * 8

   For example, a 64-channel, 32-bit, 48 kHz stream with 48
   samples per packet consumes approximately 100 Mbps including
   Ethernet framing overhead.


6.  LVDS Serial Transport

6.1.  Physical Layer

   The LVDS transport uses Low-Voltage Differential Signaling
   (conforming to TIA/EIA-644) over a single twisted pair with
   100-ohm differential impedance.

   Recommended cable types:
   -  Category 5e or better (single pair used)
   -  Shielded Twisted Pair (STP)
   -  Maximum segment length: 15 meters at 491 Mbps

   Each end of the link MUST be terminated with a 100-ohm
   differential termination resistor placed as close to the
   receiver as practical.

   The link operates in half-duplex mode.  A transceiver IC
   with separate driver and receiver on the same differential
   pair, with a driver-enable control signal, is REQUIRED at
   each end.

6.2.  8b10b Line Coding

   All data on the LVDS link is encoded using the 8b10b line
   code as defined in IEEE 802.3 Clause 36.

   The 8b10b encoding provides:

   -  DC balance: The running disparity mechanism ensures equal
      numbers of ones and zeros over time, preventing baseline
      wander.

   -  Clock recovery: The maximum run length of 5 identical
      consecutive bits ensures sufficient data transitions for
      clock and data recovery circuits.

   -  Error detection: Invalid 10-bit codes and disparity
      violations indicate bit errors.

   -  Framing: Special K-characters (control symbols) are used
      for frame synchronization and delimiter purposes.

   The following K-characters are used:

   +----------+-------+--------------------------------------------+
   | Symbol   | Value | Usage                                      |
   +----------+-------+--------------------------------------------+
   | K28.5    | 0xBC  | Comma - byte alignment, frame sync         |
   | K28.1    | 0x3C  | Start of Frame (SOF) delimiter             |
   | K27.7    | 0xFB  | Direction turnaround marker                |
   | K28.3    | 0x7C  | Idle - transmitted when no data            |
   | K29.7    | 0xFD  | End of Frame (EOF) delimiter               |
   | K23.7    | 0xF7  | Discovery beacon marker                    |
   +----------+-------+--------------------------------------------+

6.3.  TDM Frame Structure

   The LVDS transport uses a Time-Division Multiplexed (TDM) frame
   structure.  One frame is transmitted per audio sample period.

   Frame sizes:

   -  48 kHz / 44.1 kHz: 1024 data bytes (1024 symbols)
   -  96 kHz / 88.2 kHz:  512 data bytes (512 symbols)

   The parallel clock frequency MUST be:

   -  49.152 MHz for 48 kHz family sample rates
   -  45.1584 MHz for 44.1 kHz family sample rates

   Frame layout (byte offsets):

    +------+--------+------------------------------------------+
    | Byte | Length | Field                                    |
    +------+--------+------------------------------------------+
    |    0 |      1 | K28.5 (Comma)                            |
    |    1 |      1 | K28.1 (SOF)                              |
    |    2 |      8 | Frame Header (Section 6.3.1)             |
    |   10 |      1 | Downstream Sideband Byte                 |
    |   11 |      N | Downstream Audio Slots                   |
    |      |      M | Downstream Aux Data (tunnels)            |
    |      |      2 | Guard (K28.5 + K27.7)                    |
    |      |      1 | Upstream Sideband Byte                   |
    |      |      N | Upstream Audio Slots                     |
    |      |      M | Upstream Aux Data (tunnels)              |
    | F-4  |      4 | CRC-32                                   |
    +------+--------+------------------------------------------+

   Where F is the total frame size (1024 or 512).

6.3.1.  Frame Header

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Frame Counter           |  Frame Type   |     Flags     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Node Count   | DN Audio Slots| UP Audio Slots| SlotMap CRC   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Flags:
      Bits 0-1: Sample rate index (0=48k, 1=96k, 2=44.1k, 3=88.2k)
      Bits 2-3: Bit depth index (0=16, 1=24, 2=32)
      Bit 4: Sideband valid
      Bits 5-7: Reserved (MUST be 0)

6.4.  Slot Map

   The slot map is a deterministic assignment of byte offsets
   within the TDM frame to audio channels and tunnel streams.

   The slot map is computed by the bus master during the
   configuration phase and distributed to all slave nodes.  Once
   active, the slot map MUST NOT change until the bus master
   initiates reconfiguration.

   Each audio slot entry specifies:
   -  Byte offset within the frame
   -  Byte width (2, 3, or 4 for 16/24/32-bit audio)
   -  Logical channel ID (0-127)
   -  Owning node ID
   -  Direction (downstream or upstream)

   The packing algorithm is greedy by node order:

   1.  For each node in ascending ID order, allocate downstream
       audio channels sequentially.
   2.  Allocate downstream tunnel bandwidth per node.
   3.  Insert guard symbols.
   4.  For each node in ascending ID order, allocate upstream
       audio channels.
   5.  Allocate upstream tunnel bandwidth per node.
   6.  Verify total does not exceed frame size minus CRC.
   7.  Compute CRC-8 over the slot map for configuration
       verification.

6.5.  Half-Duplex Direction Switching

   The LVDS link operates in half-duplex mode on a single
   differential pair.  Within each TDM frame, data flows in
   the downstream direction (master to slave) during the first
   portion, and in the upstream direction (slave to master)
   during the second portion.

   Direction switching is signaled by the guard sequence:
   K28.5 (comma) followed by K27.7 (turnaround marker).

   After transmitting the guard sequence, the transmitting node
   MUST disable its LVDS driver within 10 nanoseconds.  The
   receiving node MUST enable its LVDS driver after a minimum
   guard time of 650 nanoseconds (32 symbol periods at
   49.152 MHz) to allow the remote receiver's CDR to reacquire
   phase lock.

6.6.  Daisy-Chain Forwarding

   In LVDS mode, nodes are connected in a daisy chain:

      Master <--> Node 1 <--> Node 2 <--> ... <--> Node N

   Each intermediate node receives the complete TDM frame on its
   upstream port, extracts its assigned audio channels, inserts
   its upstream audio data, and retransmits the modified frame
   on its downstream port.

   This store-and-forward operation adds one frame period of
   latency per hop.  For 8 nodes at 48 kHz, the total
   round-trip latency is approximately 336 microseconds.


7.  Clock Synchronization

7.1.  PTP Profile for AudioBus

   AudioBus defines a PTP profile based on IEEE 1588-2019,
   optimized for local-area audio networks.

   Profile parameters:

   +----------------------------+----------------------------------+
   | Parameter                  | Value                            |
   +----------------------------+----------------------------------+
   | Transport                  | IEEE 802.3 (Layer 2)             |
   | Delay mechanism            | End-to-End (E2E)                 |
   | Sync interval              | 125 ms (8 per second)            |
   | Delay_Req interval         | 500 ms (once per 4 Syncs)        |
   | Announce interval          | N/A (beacon replaces Announce)   |
   | Domain                     | 0 (default)                      |
   | One-step / Two-step        | Two-step (Sync + Follow_Up)      |
   | Hardware timestamping      | REQUIRED                         |
   | Target accuracy            | < 1 microsecond                  |
   +----------------------------+----------------------------------+

   Implementations MUST support hardware timestamping at the
   MAC/PHY boundary for PTP event messages (Sync and Delay_Req).
   Software-only timestamping MAY be used for initial development
   but is NOT RECOMMENDED for production deployments.

7.2.  Grandmaster Election

   The grandmaster is elected using the Best Master Clock (BMC)
   algorithm, communicated via the beacon protocol (Section 8).

   Comparison criteria (in priority order):

   1.  Clock Class:  Lower value wins.  Standard values:
       -  6:  PTP-locked (external reference)
       -  13: Application-specific
       -  248: Default (free-running)

   2.  Priority:  Lower value wins.  Range 1-255, default 128.

   3.  Node UID:  Lower value wins (tiebreaker).

   When the current grandmaster becomes unreachable (no Sync
   received for 3 consecutive Sync intervals), all nodes MUST
   re-run the BMC algorithm.  The node with the best clock
   among remaining nodes becomes the new grandmaster.

   A newly elected grandmaster MUST begin transmitting Sync
   messages within one Sync interval of election.

7.3.  Synchronization Messages

7.3.1.  Sync (Type 0x10)

   Transmitted by the grandmaster at the Sync interval.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          PTP Sequence         |          Reserved             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                   Origin Timestamp (64-bit)                   |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Origin Timestamp:  The grandmaster's best estimate of the time
   at which the Sync was transmitted, in nanoseconds.  In two-step
   mode, this is a rough estimate; the precise timestamp is carried
   in the Follow_Up.

7.3.2.  Follow_Up (Type 0x11)

   Transmitted by the grandmaster after each Sync.  Contains the
   precise hardware-captured timestamp of the Sync departure.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          PTP Sequence         |          Reserved             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |              Precise Origin Timestamp (64-bit)                |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

7.3.3.  Delay_Req (Type 0x12)

   Transmitted by a slave node to measure the path delay to the
   grandmaster.

   The payload is identical to the Sync message.  The slave
   records the hardware timestamp of transmission (t3).

7.3.4.  Delay_Resp (Type 0x13)

   Transmitted by the grandmaster in response to a Delay_Req.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          PTP Sequence         |          Reserved             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                Receive Timestamp (64-bit)                     |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                       Requester UID                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

7.4.  Media Clock Recovery

   The PTP-synchronized time base provides a common reference for
   audio sample timing.  Audio packets carry presentation
   timestamps that are expressed in this common time base.

   Receivers MUST derive their local audio sample clock (used to
   feed DACs or I2S interfaces) from the PTP time base.
   Implementations SHOULD use a Phase-Locked Loop (PLL) or
   Numerically Controlled Oscillator (NCO) to generate a
   low-jitter sample clock locked to the PTP reference.

7.5.  Hardware Timestamping Requirements

   Conformant implementations MUST capture timestamps at the
   Ethernet MAC/PHY boundary (MII/RMII/RGMII interface) for
   PTP event messages.

   The timestamp resolution MUST be at least 10 nanoseconds.
   A resolution of 1 nanosecond or better is RECOMMENDED.

   The timestamping mechanism MUST NOT introduce more than
   100 nanoseconds of non-deterministic error.


8.  Node Discovery

8.1.  Beacon Message (Type 0x02)

   Every AudioBus node MUST periodically transmit a beacon
   message to the discovery multicast group.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           Node UID                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Name Len    |   HW Type     | Talker Streams| Listener Slots|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | PTP Priority  |PTP Clock Class|          Flags                |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                     Node Name (UTF-8)                     ... |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   HW Type:  Indicates the general function of the node.

   +-------+------------------------------+
   | Value | Description                  |
   +-------+------------------------------+
   |   0   | Generic                      |
   |   1   | Speaker / output device      |
   |   2   | Microphone / input device    |
   |   3   | Speaker + Microphone         |
   |   4   | Analog bridge (ADC/DAC)      |
   |   5   | Digital bridge (format conv) |
   |   6   | Mixer / DSP                  |
   |   7   | Recording device             |
   | 8-254 | Reserved                     |
   |  255  | Vendor-specific              |
   +-------+------------------------------+

   Flags (16 bits):
      Bit 0:  Node is current PTP grandmaster
      Bits 1-15: Reserved (MUST be 0)

8.2.  Beacon Interval and Expiry

   Beacons MUST be transmitted every 1000 milliseconds (+/- 10%).

   A node MUST be considered offline if no beacon has been
   received from it within 3000 milliseconds.  Upon declaring
   a node offline, implementations SHOULD:

   -  Remove the node from the local node table
   -  Notify the application via callback
   -  If the lost node was the grandmaster, re-run BMC

8.3.  Node Identification

   The Node UID SHOULD be derived from the Ethernet MAC address
   as follows:

      UID = (MAC[2] << 24) | (MAC[3] << 16) | (MAC[4] << 8) | MAC[5]

   This provides uniqueness on any single LAN segment.

   The Node Name is a human-readable UTF-8 string of up to 31
   bytes.  It is RECOMMENDED that implementations include a
   board or model identifier and a unique suffix derived from
   the MAC address.


9.  Stream Management

9.1.  Stream Announcement (Type 0x05)

   A talker MUST announce each of its published streams by
   periodically transmitting a stream announcement packet to the
   discovery multicast group.  Announcements MUST be sent at
   least once per beacon interval.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |   Channels    |   Bit Depth   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sample Rate                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      Packet Interval (us)     |   Encoding    |   Name Len    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Stream Name (UTF-8)                    ... |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   Channel Labels (TLV)                    ... |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Encoding values:
   -  0: Linear PCM (only value defined in this specification)
   -  1-255: Reserved for future encoding types

   Channel Labels are encoded as a sequence of:
      [label_length: 1 byte] [label_data: label_length bytes (UTF-8)]
   One entry per channel, in channel order.  A label_length of 0
   indicates an unnamed channel.

9.2.  Stream Subscription (Type 0x03)

   A listener requests a stream by sending a subscription packet
   to the discovery multicast group.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |          Reserved             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Talker UID                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Ch Mask Len   |      Channel Bitmask (variable)           ... |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Channel Mask Length:  Number of bytes in the channel bitmask.
   A value of 0 indicates subscription to all channels.

   Channel Bitmask:  Bit N set to 1 indicates subscription to
   channel N.  Bits are numbered from 0 (LSB of first byte).

   The talker SHOULD acknowledge receipt of a subscription by
   including the listener in subsequent stream announcements.
   However, talkers MUST transmit audio to the stream's multicast
   group regardless of whether any subscriptions have been
   received, enabling stateless operation.

9.3.  Dynamic Channel Count

   A talker MAY change the channel count of an active stream at
   any time by transmitting a new stream announcement with the
   updated channel count.

   Listeners MUST handle channel count changes gracefully:

   -  If channels are added, new channels SHOULD be initialized
      to silence.
   -  If channels are removed, listeners MUST stop reading the
      removed channels after processing the updated announcement.

   The stream ID, sample rate, and bit depth MUST NOT change for
   an active stream.  To change these parameters, the talker MUST
   delete the stream and create a new one.

9.4.  Stream Teardown (Type 0x06)

   A talker removes a stream by sending a stream delete packet.
   The payload contains only the Stream ID (2 bytes).  Listeners
   MUST release resources associated with the stream upon receipt.


10. Audio Data Transport

10.1.  Audio Packet Format (Type 0x01)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |   Channels    |   Bit Depth   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sample Rate                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Samples per Channel     |          Reserved             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                Presentation Timestamp (64-bit)                |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                      Audio Sample Data                        |
   |                          (variable)                           |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   The audio header is 20 bytes, followed by the sample data.

10.2.  Sample Encoding

   Audio samples are encoded as signed integers in two's complement
   representation, in big-endian byte order.

   +------------+--------+--------------------------------------+
   | Bit Depth  | Bytes  | Range                                |
   +------------+--------+--------------------------------------+
   | 16-bit     |   2    | -32768 to +32767                     |
   | 24-bit     |   3    | -8388608 to +8388607                 |
   | 32-bit     |   4    | -2147483648 to +2147483647           |
   +------------+--------+--------------------------------------+

   Samples are interleaved by channel:

      S(0,0), S(0,1), ..., S(0,C-1),    [Frame 0, all channels]
      S(1,0), S(1,1), ..., S(1,C-1),    [Frame 1, all channels]
      ...
      S(N-1,0), S(N-1,1), ..., S(N-1,C-1) [Frame N-1, all channels]

   Where S(f,c) denotes the sample at frame f, channel c, and C
   is the channel count, and N is samples_per_channel.

   Total audio data size:
      channels * samples_per_channel * (bit_depth / 8) bytes

10.3.  Presentation Timestamps

   The Presentation Timestamp is a 64-bit signed integer
   representing nanoseconds in the PTP time base.  It indicates
   the exact instant at which the first sample in the packet
   SHOULD begin playout at the listener.

   The talker MUST compute the presentation timestamp as:

      PTS = current_PTP_time + presentation_latency

   Where presentation_latency is a fixed, configurable value
   (default: 2000 microseconds = 2,000,000 nanoseconds).

   All listeners subscribed to the same stream MUST begin playout
   of the same audio data at the same PTP-referenced instant.
   This ensures sample-accurate synchronization across all
   listeners, regardless of their position in the network.

   A listener MUST buffer incoming audio and release it for
   playout only when the local PTP clock reaches the presentation
   timestamp.  Packets that arrive after their presentation time
   SHOULD be discarded and counted as "late packets" in
   diagnostic statistics.

10.4.  Packet Interval

   The packet interval determines how many audio sample frames
   are grouped into each network packet.

   +------------------+------------------+-------------------------+
   | Interval (us)    | Samples @ 48kHz  | Typical use case        |
   +------------------+------------------+-------------------------+
   |      125         |        6         | Ultra-low latency       |
   |      250         |       12         | Low latency             |
   |      500         |       24         | Balanced                |
   |     1000         |       48         | Default (recommended)   |
   |     2000         |       96         | High efficiency         |
   |     4000         |      192         | Maximum efficiency      |
   +------------------+------------------+-------------------------+

   Implementations MUST support a packet interval of 1000 us.
   Support for other intervals is RECOMMENDED.

   Shorter intervals reduce latency but increase per-packet
   overhead and CPU load.  Longer intervals improve efficiency
   but increase minimum achievable latency.

10.5.  Sequence Numbering

   The sequence number in the AudioBus common header is
   incremented by the talker for each audio packet transmitted
   on a given stream.  The sequence number wraps from 65535 to 0.

   Listeners SHOULD track the expected sequence number for each
   subscribed stream and report gaps as packet loss.

   The packet loss ratio is computed as:

      loss_ratio = packets_lost / (packets_received + packets_lost)

10.6.  Playout Buffer

   Listeners MUST maintain a playout buffer of sufficient depth
   to absorb network jitter while meeting the target presentation
   latency.

   The minimum playout buffer depth MUST be at least:

      buffer_depth >= presentation_latency - min_network_delay

   Implementations SHOULD dynamically adjust the playout buffer
   depth based on observed jitter statistics.

10.7.  Packet Loss Concealment

   When a packet is lost (detected via sequence number gap),
   listeners SHOULD apply packet loss concealment.  Acceptable
   strategies include:

   -  Zero insertion (silence)
   -  Repetition of the last received frame
   -  Linear interpolation between adjacent valid frames

   The concealment strategy is implementation-defined.


11. Control Tunneling

11.1.  Tunnel Packet Format (Type 0x20)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Target UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Tunnel Type  |  Tunnel Len   |     Tunnel Payload         ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Target UID:  The intended recipient.  A value of 0x00000000
   indicates broadcast (all nodes).

   Tunnel Type values:

   +-------+----------+--------------------------------------------+
   | Value | Name     | Description                                |
   +-------+----------+--------------------------------------------+
   |   0   | SPI      | SPI bus transaction                        |
   |   1   | I2C      | I2C bus transaction                        |
   |   2   | GPIO     | GPIO pin state (16 pins per node)          |
   |   3   | MIDI     | MIDI message (1-3 bytes)                   |
   |   4   | SIDEBAND | Generic sideband data                      |
   | 5-15  | Reserved |                                            |
   +-------+----------+--------------------------------------------+

11.2.  GPIO Tunnel (Type 2)

   GPIO tunnel payload is exactly 2 bytes:

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         Pin States (16)       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Bit N corresponds to GPIO pin N on the target node.
   1 = HIGH, 0 = LOW.

   GPIO tunnel packets are idempotent.  The receiver MUST apply
   the complete 16-bit state on each reception (not toggle).

11.3.  MIDI Tunnel (Type 3)

   MIDI tunnel payload contains 1 to 3 bytes of raw MIDI data:

    0
    0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+
   | Status Byte   |
   +-+-+-+-+-+-+-+-+
   | Data Byte 1   |  (optional)
   +-+-+-+-+-+-+-+-+
   | Data Byte 2   |  (optional)
   +-+-+-+-+-+-+-+-+

   The tunnel length field indicates the number of MIDI bytes
   (1, 2, or 3).

   MIDI System Exclusive (SysEx) messages that exceed 3 bytes
   MUST be fragmented across multiple tunnel packets, with the
   SysEx start (0xF0) in the first packet and SysEx end (0xF7)
   in the last packet.

   At 48 kHz with 1 ms packet interval, the tunnel provides a
   theoretical MIDI throughput of 3000 bytes/second, which is
   approximately 10x the bandwidth of a standard MIDI 1.0
   serial connection (31250 baud = ~3125 bytes/sec).

11.4.  SPI Tunnel (Type 0)

   SPI tunnel payload:

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   CS Pin      |   SPI Mode    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            SPI Data        ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   CS Pin:  Which chip-select line to assert on the target node
   (0-255).

   SPI Mode:  Standard SPI mode (0-3, representing CPOL/CPHA).

   The receiver executes the SPI transaction on its local SPI
   bus and MAY return the received data via a tunnel packet in
   the reverse direction.

11.5.  I2C Tunnel (Type 1)

   I2C tunnel payload:

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  I2C Address  |     Flags     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            I2C Data        ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   I2C Address:  7-bit I2C slave address (in bits 6:0).

   Flags:
      Bit 0: Read (1) or Write (0)
      Bits 1-7: Reserved

   For write operations, the data bytes are written to the
   I2C device.  For read operations, the data bytes are the
   register address(es), and the response is returned via a
   tunnel packet in the reverse direction.


12. Metadata

12.1.  Metadata Packet Format (Type 0x30)

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |  Num Entries  |   Reserved    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                     Metadata Entries (TLV)                 ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Stream ID:  0x0000 for node-level metadata.

   Each metadata entry:
      [key_length: 1 byte] [value_length: 2 bytes]
      [key: key_length bytes (UTF-8)]
      [value: value_length bytes (UTF-8)]

12.2.  Standard Metadata Keys

   The following metadata keys are defined:

   +------------------+--------------------------------------------+
   | Key              | Description                                |
   +------------------+--------------------------------------------+
   | "vendor"         | Device manufacturer name                   |
   | "model"          | Device model identifier                    |
   | "firmware"       | Firmware version string                    |
   | "serial"         | Device serial number                       |
   | "location"       | Physical location description              |
   | "purpose"        | Intended use (e.g., "main-pa", "monitor")  |
   | "icon"           | URL or data URI for device icon             |
   +------------------+--------------------------------------------+

   Implementations MAY define additional keys using reverse-domain
   notation (e.g., "com.example.custom-key") to avoid collisions.


13. Latency Measurement

13.1.  Ping/Pong Protocol

   Any node MAY measure the round-trip time to any other node
   using the ping/pong mechanism.

   Ping (Type 0x40):
      Payload: 8-byte timestamp (sender's current PTP time in
      nanoseconds).

   Pong (Type 0x41):
      Payload: Echo of the received ping payload (8 bytes).

   The sender computes round-trip time as:

      RTT = current_PTP_time - echoed_timestamp

   Estimated one-way latency (assuming symmetric path):

      one_way = RTT / 2

   For more accurate one-way measurements, implementations
   SHOULD use the PTP path delay value.

13.2.  Jitter Measurement

   Listeners SHOULD measure interarrival jitter for each
   subscribed stream using the algorithm defined in RFC 3550
   Section 6.4.1:

      D(i,j) = (Rj - Ri) - (Sj - Si)

   Where:
      Ri, Rj = arrival timestamps of packets i and j
      Si, Sj = presentation timestamps of packets i and j

   The jitter estimate is updated per packet:

      J(i) = J(i-1) + (|D(i,j)| - J(i-1)) / 16

   Implementations SHOULD also track:
   -  Peak jitter (maximum |D| observed)
   -  Minimum jitter
   -  Packet loss count and ratio
   -  Late packet count (arrived after presentation time)


14. Security Considerations

   AudioBus in its current form provides no authentication,
   encryption, or integrity protection for audio data or control
   messages.  All data is transmitted in cleartext.

   Deployments in environments where security is a concern
   SHOULD use one or more of the following mitigations:

   -  Physical network isolation (dedicated switch or VLAN)
   -  IEEE 802.1X port-based access control
   -  MACsec (IEEE 802.1AE) for link-layer encryption

   Future versions of this specification MAY define optional
   security extensions, such as:

   -  HMAC-SHA256 authentication of the AudioBus common header
   -  DTLS-based encryption of audio payload
   -  Node certificate-based authentication

   Implementations MUST NOT rely on the AudioBus protocol for
   safety-critical applications without additional integrity
   verification.


15. IANA Considerations

   This specification requests the following assignments:

15.1.  EtherType Assignment

   This document requests the assignment of an EtherType value
   from the IEEE Registration Authority for "AudioBus Protocol."
   Pending assignment, the locally administered value 0x88B6 is
   used.

15.2.  Multicast Address Assignment

   This document requests the assignment of an Ethernet multicast
   OUI from the IEEE Registration Authority.  Pending assignment,
   the locally administered prefix 01:60:AB is used.

15.3.  AudioBus Packet Type Registry

   This document establishes the "AudioBus Packet Types" registry
   with the initial values defined in Section 4.2.  New packet
   types in the range 0x50-0x7F may be registered via Expert
   Review.  Values 0x80-0xFF are reserved for private use.

15.4.  AudioBus Tunnel Type Registry

   This document establishes the "AudioBus Tunnel Types" registry
   with the initial values defined in Section 11.1.  New tunnel
   types in the range 5-15 may be registered via Expert Review.

15.5.  AudioBus Hardware Type Registry

   This document establishes the "AudioBus Hardware Types" registry
   with the initial values defined in Section 8.1.  New values
   in the range 8-254 may be registered via Specification Required.

15.6.  AudioBus Metadata Key Registry

   This document establishes the "AudioBus Metadata Keys" registry
   with the initial values defined in Section 12.2.  New keys
   may be registered via First Come First Served.


16. References

16.1.  Normative References

   [RFC2119]  Bradner, S., "Key words for use in RFCs to Indicate
              Requirement Levels", BCP 14, RFC 2119,
              DOI 10.17487/RFC2119, March 1997.

   [RFC8174]  Leiba, B., "Ambiguity of Uppercase vs Lowercase in
              RFC 2119 Key Words", BCP 14, RFC 8174,
              DOI 10.17487/RFC8174, May 2017.

   [IEEE802.3]
              IEEE, "IEEE Standard for Ethernet", IEEE Std 802.3.

   [IEEE1588]
              IEEE, "IEEE Standard for a Precision Clock
              Synchronization Protocol for Networked Measurement
              and Control Systems", IEEE Std 1588-2019.

   [IEEE802.3-36]
              IEEE, "IEEE 802.3 Clause 36: Physical Coding
              Sublayer (PCS) and Physical Medium Attachment (PMA)
              sublayer, type 1000BASE-X", (defines 8b10b coding).

16.2.  Informative References

   [RFC3550]  Schulzrinne, H., Casner, S., Frederick, R., and V.
              Jacobson, "RTP: A Transport Protocol for Real-Time
              Applications", STD 64, RFC 3550,
              DOI 10.17487/RFC3550, July 2003.

   [AES67]    Audio Engineering Society, "AES67-2018: AES standard
              for audio applications of networks - High-performance
              streaming audio-over-IP interoperability", 2018.

   [TIA644]   TIA, "TIA/EIA-644: Electrical Characteristics of
              Low Voltage Differential Signaling (LVDS) Interface
              Circuits", 2001.


Appendix A.  Channel Capacity Tables

A.1.  Ethernet Transport (100 Mbps, per stream)

   +-------------+-----------+------------------+-------------------+
   | Sample Rate | Bit Depth | Interval 1000 us | Max Channels      |
   +-------------+-----------+------------------+-------------------+
   |   48000     |    32     |    48 samples    |       64          |
   |   48000     |    24     |    48 samples    |       86          |
   |   96000     |    32     |    96 samples    |       32          |
   |   96000     |    24     |    96 samples    |       43          |
   +-------------+-----------+------------------+-------------------+

A.2.  LVDS Transport (491 Mbps, per direction)

   +-------------+-----------+------------------+-------------------+
   | Sample Rate | Bit Depth | Frame Size       | Max Channels/Dir  |
   +-------------+-----------+------------------+-------------------+
   |   48000     |    32     |   1024 bytes     |       64          |
   |   48000     |    24     |   1024 bytes     |       64          |
   |   96000     |    32     |    512 bytes     |       62          |
   |   96000     |    24     |    512 bytes     |       64          |
   |   44100     |    32     |   1024 bytes     |       64          |
   +-------------+-----------+------------------+-------------------+

   Note: LVDS channel counts assume 50/50 split between downstream
   and upstream, with 16 bytes overhead and no tunnel allocation.


Appendix B.  Recommended Hardware

B.1.  Ethernet Transport

   -  Microcontroller: Espressif ESP32-P4 (dual RISC-V, 400 MHz,
      EMAC with IEEE 1588 hardware timestamping)
   -  Ethernet PHY: IP101GRI, RTL8201, or LAN8720
      (any 100BASE-TX RMII PHY)
   -  Connector: Standard RJ45
   -  Switch: Any unmanaged or managed 100 Mbps Ethernet switch

B.2.  LVDS Transport (Single-Chip, Recommended)

   -  Microcontroller: Espressif ESP32-P4 (PARLIO peripheral)
   -  Transceiver: TI SN65LVDT41 (single LVDS transceiver)
   -  Clock: Si5351A programmable clock generator
   -  Cable: Cat5e twisted pair, single pair used
   -  Termination: 100-ohm differential at each end

B.3.  LVDS Transport (Two-Chip, Maximum Performance)

   -  Microcontroller: Espressif ESP32-P4 (PARLIO peripheral)
   -  Serializer: TI DS92LV1021A (10:1 LVDS)
   -  Deserializer: TI DS92LV1212A (1:10 LVDS with CDR)
   -  Clock: 49.152 MHz crystal oscillator (master only)
   -  Cable: Cat5e twisted pair, single pair used
   -  Termination: 100-ohm differential at each end


Authors' Addresses

   Sylwester Sosnowski
   DatanoiseTV
   Email: tbd@datanoise.tv
```
