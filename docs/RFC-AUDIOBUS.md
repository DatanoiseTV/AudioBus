```
Internet Engineering Task Force (IETF)        S. Sosnowski
Request for Comments: NNNN                     DatanoiseTV
Category: Standards Track                    April 3, 2026
ISSN: 2070-1721


    AudioBus: Multiplexed Audio and Control Transport
           over Ethernet and LVDS Links

Abstract

   This document specifies AudioBus, a deterministic
   protocol for low-latency transport of multi-channel
   digital audio and control data over IEEE 802.3
   Ethernet networks and Low-Voltage Differential
   Signaling (LVDS) serial links.

   The protocol supports up to 64 channels of 32-bit
   linear PCM audio at sample rates up to 96 kHz,
   multiplexed with GPIO, MIDI, SPI, and I2C control
   tunnels.  Clock synchronization is achieved using a
   constrained profile of IEEE 1588 Precision Time
   Protocol with mandatory hardware timestamping,
   providing sub-microsecond accuracy across all
   participating nodes.  The protocol is fully
   self-configuring: nodes discover one another and
   elect a clock reference automatically using the Best
   Master Clock algorithm.

   AudioBus addresses two complementary deployment
   scenarios.  In Ethernet mode, nodes attach to
   commodity IEEE 802.3 switches in star or tree
   topologies and communicate via Layer 2 multicast
   using a dedicated EtherType.  In LVDS mode, nodes
   form a daisy-chain over a single twisted pair using
   8b10b-coded half-duplex TDM framing with a
   master/slave relationship.

   An implementation MAY support one or both transport
   modes.  A bridge node MAY interconnect an Ethernet
   segment with an LVDS daisy chain.

Status of This Memo

   This is an Internet Standards Track document.

   This document is a product of the Internet
   Engineering Task Force (IETF).  It represents the
   consensus of the IETF community.  It has received
   public review and has been approved for publication
   by the Internet Engineering Steering Group (IESG).
   Further information on Internet Standards is
   available in Section 2 of RFC 7841.

   Information about the current status of this
   document, any errata, and how to provide feedback
   on it may be obtained at
   https://www.rfc-editor.org/info/rfcNNNN.

Copyright Notice

   Copyright (c) 2026 IETF Trust and the persons
   identified as the document authors.  All rights
   reserved.

   This document is subject to BCP 78 and the IETF
   Trust's Legal Provisions Relating to IETF Documents
   (https://trustee.ietf.org/license-info) in effect
   on the date of publication of this document.
   Please review these documents carefully, as they
   describe your rights and restrictions with respect
   to this document.  Code Components extracted from
   this document must include Revised BSD License text
   as described in Section 4.e of the Trust Legal
   Provisions and are provided without warranty as
   described in the Revised BSD License.


Table of Contents

   1.  Introduction  . . . . . . . . . . . . . . . .   4
       1.1.  Motivation  . . . . . . . . . . . . . .   4
       1.2.  Design Goals  . . . . . . . . . . . . .   5
       1.3.  Scope . . . . . . . . . . . . . . . . .   7
       1.4.  Relationship to Other Protocols . . . .   7
       1.5.  Document Organization . . . . . . . . .   8
   2.  Terminology and Conventions . . . . . . . . .   9
       2.1.  Requirements Language . . . . . . . . .   9
       2.2.  Definitions . . . . . . . . . . . . . .   9
       2.3.  Notation  . . . . . . . . . . . . . . .  12
   3.  Protocol Architecture . . . . . . . . . . . .  13
       3.1.  Layer Model . . . . . . . . . . . . . .  13
       3.2.  Transport Modes . . . . . . . . . . . .  15
       3.3.  Node Roles  . . . . . . . . . . . . . .  16
       3.4.  Stream Model  . . . . . . . . . . . . .  17
       3.5.  Node Lifecycle State Machine  . . . . .  18
       3.6.  Timing Model . . . . . . . . . . . . .  20
   4.  Common Packet Format  . . . . . . . . . . . .  22
       4.1.  Common Header . . . . . . . . . . . . .  22
       4.2.  ABNF for Common Header  . . . . . . . .  25
       4.3.  Packet Type Registry  . . . . . . . . .  27
       4.4.  Byte Ordering . . . . . . . . . . . . .  28
       4.5.  Maximum Packet Size . . . . . . . . . .  28
       4.6.  Packet Integrity  . . . . . . . . . . .  29
   5.  Ethernet Transport  . . . . . . . . . . . . .  30
       5.1.  Frame Encapsulation . . . . . . . . . .  30
       5.2.  EtherType . . . . . . . . . . . . . . .  32
       5.3.  Multicast Addressing  . . . . . . . . .  33
       5.4.  VLAN Tagging  . . . . . . . . . . . . .  35
       5.5.  Quality of Service  . . . . . . . . . .  36
       5.6.  Coexistence with IP Traffic . . . . . .  37
       5.7.  Bandwidth Budget  . . . . . . . . . . .  38
       5.8.  Switch Requirements . . . . . . . . . .  39
   6.  LVDS Serial Transport . . . . . . . . . . . .  41
       6.1.  Physical Layer  . . . . . . . . . . . .  41
       6.2.  8b10b Line Coding . . . . . . . . . . .  43
       6.3.  TDM Frame Structure . . . . . . . . . .  45
       6.4.  Frame Header  . . . . . . . . . . . . .  47
       6.5.  Slot Map  . . . . . . . . . . . . . . .  50
       6.6.  Half-Duplex Operation . . . . . . . . .  53
       6.7.  Daisy-Chain Forwarding  . . . . . . . .  55
       6.8.  LVDS Node State Machine . . . . . . . .  57
       6.9.  Error Detection . . . . . . . . . . . .  59
   7.  Clock Synchronization . . . . . . . . . . . .  60
       7.1.  PTP Profile . . . . . . . . . . . . . .  60
       7.2.  Grandmaster Election  . . . . . . . . .  62
       7.3.  Sync Message  . . . . . . . . . . . . .  65
       7.4.  Follow_Up Message . . . . . . . . . . .  67
       7.5.  Delay_Req Message . . . . . . . . . . .  69
       7.6.  Delay_Resp Message  . . . . . . . . . .  71
       7.7.  Offset and Delay Computation  . . . . .  73
       7.8.  Clock Servo . . . . . . . . . . . . . .  75
       7.9.  Media Clock Recovery  . . . . . . . . .  77
       7.10. Hardware Timestamping . . . . . . . . .  78
       7.11. LVDS Clock Synchronization  . . . . . .  80
   8.  Node Discovery  . . . . . . . . . . . . . . .
   9.  Stream Management . . . . . . . . . . . . . .
   10. Audio Data Transport  . . . . . . . . . . . .
   11. Control Tunneling . . . . . . . . . . . . . .
   12. Metadata  . . . . . . . . . . . . . . . . . .
   13. Latency Measurement . . . . . . . . . . . . .
   14. Error Handling  . . . . . . . . . . . . . . .
   15. Extensibility and Versioning  . . . . . . . .
   16. Security Considerations . . . . . . . . . . .
   17. IANA Considerations . . . . . . . . . . . . .
   18. References  . . . . . . . . . . . . . . . . .
   Appendix A.  Channel Capacity Tables  . . . . . .
   Appendix B.  Recommended Hardware . . . . . . . .
   Appendix C.  Example Message Exchange . . . . . .
   Acknowledgements  . . . . . . . . . . . . . . . .
   Authors' Addresses  . . . . . . . . . . . . . . .


1.  Introduction

1.1.  Motivation

   Professional and consumer audio systems require
   the transport of multiple channels of high-
   resolution digital audio alongside control data
   over simple, low-cost physical interconnections.
   Existing solutions in this space -- including
   Dante, AVB/Milan, AES67, and proprietary serial
   buses -- suffer from one or more of the following
   limitations:

   (a) Reliance on proprietary licensing, which
       restricts adoption and increases per-unit
       cost.

   (b) Dependence on specialized network
       infrastructure such as AVB-capable switches,
       which limits deployment flexibility.

   (c) Inability to operate over low-cost serial
       links, precluding use in embedded, battery-
       powered, or space-constrained designs.

   (d) Rigid channel counts and sample rates that
       cannot be changed at runtime without tearing
       down and re-establishing all streams.

   (e) Complex configuration requirements that
       demand manual setup by trained personnel.

   AudioBus addresses these limitations by defining
   an open protocol that operates over two
   complementary transports: standard IEEE 802.3
   Ethernet using commodity switches, and dedicated
   LVDS serial links over a single twisted pair for
   cost-optimized, ultra-low-latency daisy-chain
   topologies.

   The protocol is designed for full self-
   configuration.  Nodes discover one another via
   periodic beacons, elect a clock reference
   automatically, announce and subscribe to audio
   streams without user intervention, and adapt
   dynamically to changes in network topology and
   stream configuration.

1.2.  Design Goals

   The AudioBus protocol satisfies the following
   requirements, ordered by priority:

   G1.  Deterministic latency.

        The end-to-end delay from talker sample
        capture to listener sample playout MUST be
        fixed and predictable for a given network
        topology and configuration.  There MUST be
        no data-dependent variation in latency.  In
        Ethernet mode, the worst-case one-way latency
        through a single switch is bounded by:

           L_eth = T_pkt + T_switch + T_buf

        where T_pkt is the packetization delay
        (packet interval), T_switch is the switch
        fabric delay (typically 3-10 microseconds
        for a store-and-forward switch), and T_buf
        is the playout buffer depth.

        In LVDS mode, the one-way latency is:

           L_lvds = H * T_frame

        where H is the number of hops and T_frame
        is the frame period (1/Fs).

   G2.  Zero-jitter playout.

        Audio playout MUST be synchronized to a
        common time base across all nodes,
        independent of network topology and packet
        arrival jitter.  All listeners receiving the
        same stream MUST begin playout of each audio
        frame at the same PTP-referenced instant,
        with sample-level accuracy.

   G3.  Plug-and-play operation.

        No manual configuration is required.  Nodes
        MUST discover each other, negotiate
        capabilities, elect a clock reference, and
        begin streaming automatically.  Adding or
        removing a node MUST NOT disrupt active
        streams on other nodes, except for the
        transient loss of any stream originated by a
        departing node.

   G4.  Dynamic reconfiguration.

        Channel counts MAY change at runtime without
        stream teardown.  Sample rates and bit depths
        require stream deletion and re-creation.
        Nodes MAY join or leave at any time.  The
        protocol MUST converge to a stable state
        within a bounded time after any topology
        change.  The convergence time MUST NOT exceed
        4 seconds (the sum of the node expiry timeout
        of 3 seconds plus one beacon interval of
        1 second).

   G5.  Coexistence.

        In Ethernet mode, AudioBus traffic MUST NOT
        interfere with IP protocol stacks on the
        same interface.  AudioBus and IP traffic MUST
        be able to share the same physical port and
        switch infrastructure.

   G6.  Implementation simplicity.

        The protocol MUST be implementable on
        microcontrollers with 400 MHz clock and
        512 KB SRAM.  The common header is 12 octets.
        Packet parsing requires no variable-length
        fields before the payload.  No floating-point
        arithmetic is required for any protocol
        operation.

   G7.  Low cost.

        All required components (Ethernet PHY, LVDS
        transceiver, crystal oscillator) are commodity
        parts available from multiple vendors at
        consumer price points.

   G8.  Extensibility.

        The packet type space accommodates future
        extensions.  Reserved fields and a version
        number support backward-compatible evolution.
        A private-use range (0x50-0x7F) allows
        vendor-specific extensions without
        coordination.

1.3.  Scope

   This specification defines:

   -  Wire formats for all packet types.
   -  Transport encapsulation for IEEE 802.3
      Ethernet and LVDS serial links.
   -  A constrained PTP profile for clock
      synchronization, including grandmaster
      election, message formats, and servo
      requirements.
   -  Node discovery via periodic beacons.
   -  Stream management: announcement, subscription,
      dynamic channel changes, and teardown.
   -  Audio packetization with presentation
      timestamps and sequence numbering.
   -  Control tunneling for GPIO, MIDI, SPI, and
      I2C.
   -  Metadata distribution.
   -  Latency measurement via ping/pong.
   -  Error handling and recovery procedures.
   -  Extensibility mechanisms.

   This specification does not define:

   -  Hardware implementation details beyond the
      minimum requirements for interoperability.
   -  Application-layer processing logic (e.g.,
      mixing, effects, routing matrices).
   -  Digital rights management or content
      protection.
   -  Wireless transports.
   -  Bridging algorithms between Ethernet and LVDS
      segments (the bridge behavior is described at
      the protocol level, but bridge implementation
      is out of scope).

1.4.  Relationship to Other Protocols

   AudioBus is informed by, but distinct from,
   several existing protocols:

   IEEE 1722 (AVTP):  AudioBus shares the concept
      of presentation timestamps but does not
      require AVB-capable switches, gPTP, or
      Stream Reservation Protocol.

   AES67:  AudioBus targets similar audio quality
      but operates at Layer 2 rather than Layer 3,
      eliminating IP/UDP overhead and ARP/IGMP
      dependencies.

   IEEE 1588 (PTP):  AudioBus uses a constrained
      profile of PTP for clock synchronization
      (Section 7).  The profile is not compatible
      with the default PTP profile or gPTP; it
      uses a dedicated multicast address and
      AudioBus packet encapsulation.

   MIDI 2.0 (UMP):  AudioBus tunnels raw MIDI
      bytes; it does not implement the Universal
      MIDI Packet format.  An application layer
      MAY encapsulate UMP within the MIDI tunnel.

1.5.  Document Organization

   The remainder of this document is organized as
   follows:

   Section 2 defines terminology and conventions.

   Section 3 describes the protocol architecture,
   including the layer model, transport modes, node
   roles, stream model, node lifecycle, and timing
   model.

   Section 4 specifies the common packet format
   shared by all AudioBus packet types in both
   transport modes.

   Section 5 specifies the Ethernet transport,
   including frame encapsulation, multicast
   addressing, VLAN tagging, and QoS.

   Section 6 specifies the LVDS serial transport,
   including the physical layer, 8b10b coding, TDM
   frame structure, and daisy-chain forwarding.

   Section 7 specifies clock synchronization using
   the AudioBus PTP profile.

   Sections 8 through 17 (published separately)
   cover node discovery, stream management, audio
   transport, control tunneling, metadata, latency
   measurement, error handling, extensibility,
   security, and IANA considerations.


2.  Terminology and Conventions

2.1.  Requirements Language

   The key words "MUST", "MUST NOT", "REQUIRED",
   "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT",
   "RECOMMENDED", "NOT RECOMMENDED", "MAY", and
   "OPTIONAL" in this document are to be interpreted
   as described in BCP 14 [RFC2119] [RFC8174] when,
   and only when, they appear in all capitals, as
   shown here.

2.2.  Definitions

   Node:  A device that participates in the AudioBus
      protocol by sending, receiving, or forwarding
      AudioBus packets.  A node has exactly one Node
      UID and at least one network interface on which
      it operates the protocol.

   Talker:  A node that publishes one or more audio
      streams.  A talker originates AUDIO packets and
      transmits STREAM_ANNOUNCE messages describing
      each stream it offers.

   Listener:  A node that subscribes to one or more
      audio streams.  A listener receives AUDIO
      packets, buffers them, and presents them at the
      indicated presentation timestamp.

   Talker-Listener:  A node that acts simultaneously
      as both a talker and a listener.  This is the
      common case for devices such as mixing consoles
      and digital signal processors.

   Stream:  A unidirectional flow of audio data from
      exactly one talker to one or more listeners.
      A stream has a fixed sample rate and bit depth,
      and a dynamic channel count that may change
      during the lifetime of the stream.

   Stream ID:  A 16-bit unsigned integer that
      uniquely identifies a stream within the scope
      of its originating talker.  The combination of
      Talker UID and Stream ID is globally unique
      within the AudioBus network.

   Node UID:  A 32-bit unsigned integer that
      uniquely identifies a node within an AudioBus
      network.  The UID is derived from the node's
      MAC address (Section 8.3) or assigned by other
      means, provided uniqueness is guaranteed.

   Presentation Timestamp (PTS):  A 64-bit signed
      integer representing a time in nanoseconds
      relative to the PTP epoch (1 January 1970
      00:00:00 TAI).  The PTS indicates the instant
      at which the first audio sample in a packet
      SHOULD begin playout at all listeners.

   Grandmaster (GM):  The node whose clock serves as
      the reference for all other nodes in the
      AudioBus network.  The grandmaster is elected
      via the Best Master Clock algorithm
      (Section 7.2).  In LVDS mode, the bus master
      is always the grandmaster.

   Slave:  In the context of PTP, a node that
      synchronizes its clock to the grandmaster.  In
      LVDS mode, a slave is any node that is not the
      bus master.

   Bus Master:  (LVDS mode only.)  The node at
      position 0 in the daisy chain that generates
      the bus clock, initiates frame transmissions,
      and computes the slot map.

   Packet Interval:  The time in microseconds
      between consecutive AUDIO packets within a
      single stream.  The default is 1000
      microseconds (1 millisecond).

   Beacon Interval:  The time in milliseconds
      between consecutive BEACON packets from a
      single node.  The nominal value is 1000
      milliseconds.

   Slot Map:  (LVDS mode only.)  A table computed
      by the bus master that assigns byte offsets
      within the TDM frame to audio channels and
      tunnel data for each node in the chain.

   Guard Time:  (LVDS mode only.)  A period of idle
      symbols inserted at the boundary between
      downstream and upstream phases within a TDM
      frame, allowing the clock and data recovery
      circuit at the receiver to re-lock after the
      direction change.

   Frame Period:  The duration of one TDM frame in
      LVDS mode, equal to the reciprocal of the
      audio sample rate (e.g., 20.833 microseconds
      at 48 kHz).

   Convergence Time:  The maximum time from a
      topology change event (node join, departure,
      or grandmaster loss) until the protocol
      reaches a stable state.  The convergence time
      MUST NOT exceed 4000 milliseconds.

   Playout Buffer:  A FIFO buffer at a listener
      that accumulates audio samples received ahead
      of their presentation timestamp and releases
      them for digital-to-analog conversion at the
      correct instant.

   CRC-32:  The 32-bit cyclic redundancy check
      defined in IEEE 802.3 [IEEE802.3], polynomial
      0x04C11DB7, used for frame integrity
      verification.

   CRC-8:  The 8-bit cyclic redundancy check with
      polynomial 0x07 (x^8 + x^2 + x + 1), used
      for slot map verification in LVDS mode.

2.3.  Notation

   This document uses the following notational
   conventions:

   -  Packet diagrams use the format defined in
      [RFC9562], with bit 0 as the most significant
      bit of the first octet.

   -  All multi-octet integer fields are in network
      byte order (big-endian) unless stated
      otherwise.

   -  Hexadecimal values are prefixed with "0x".

   -  Binary values are prefixed with "0b".

   -  ABNF notation follows [RFC5234] and
      [RFC7405].

   -  The operator "|" in ABNF denotes alternatives.

   -  The notation "N*M" in ABNF denotes repetition
      of at least N and at most M occurrences.

   -  Field sizes are given in octets unless
      explicitly noted as bits.


3.  Protocol Architecture

3.1.  Layer Model

   AudioBus is organized into the following
   protocol layers.  Each layer depends only on
   the services of the layer immediately below it.

   +-----------------------------------------------+
   |          Application Layer                     |
   |  (user-facing audio routing, UI, control)      |
   +-----------------------------------------------+
   |          Stream Management Layer               |
   |  (discovery, announce, subscribe, metadata)    |
   +-----------------------------------------------+
   |          Audio Transport Layer                 |
   |  (packetization, presentation timestamps,      |
   |   sequencing, playout buffering)               |
   +-----------------------------------------------+
   |          Clock Synchronization Layer           |
   |  (PTP profile, grandmaster election,           |
   |   offset/delay computation, clock servo)       |
   +-----------------------------------------------+
   |          Control Tunnel Layer                  |
   |  (GPIO, MIDI, SPI, I2C multiplexing)           |
   +-----------------------------------------------+
   |          Framing Layer                         |
   |  (common header, packet type dispatch,         |
   |   integrity check)                             |
   +-----------------------------------------------+
   |          Transport Layer                       |
   | +-------------------+ +---------------------+ |
   | | Ethernet (L2)     | | LVDS Serial (TDM)   | |
   | | IEEE 802.3        | | 8b10b half-duplex    | |
   | +-------------------+ +---------------------+ |
   +-----------------------------------------------+
   |          Physical Layer                        |
   | +-------------------+ +---------------------+ |
   | | 100/1000BASE-T    | | LVDS twisted pair    | |
   | | (copper/fiber)    | | (TIA/EIA-644)        | |
   | +-------------------+ +---------------------+ |
   +-----------------------------------------------+

         Figure 1: AudioBus Protocol Layer Model

   The Application Layer is outside the scope of
   this specification.  It consumes decoded audio
   samples, processes control tunnel data, and
   presents stream information to the user.

   The Stream Management Layer handles node
   discovery (Section 8), stream announcement and
   subscription (Section 9), metadata distribution
   (Section 12), and latency measurement
   (Section 13).

   The Audio Transport Layer handles packetization
   of raw audio samples, attachment of presentation
   timestamps referenced to the PTP time base,
   sequence numbering for loss detection, and
   playout buffer management (Section 10).

   The Clock Synchronization Layer implements the
   AudioBus PTP profile (Section 7), including
   grandmaster election, two-step Sync/Follow_Up
   exchange, Delay_Req/Delay_Resp measurement, and
   the clock servo that disciplines the local
   oscillator to the grandmaster reference.

   The Control Tunnel Layer multiplexes GPIO, MIDI,
   SPI, and I2C data into tunnel packets
   (Section 11).  Tunnel data is transported
   alongside audio but with independent timing.

   The Framing Layer prepends the 12-octet common
   header (Section 4.1) to every payload and
   dispatches received packets to the appropriate
   upper layer based on the Pkt Type field.

   The Transport Layer performs encapsulation into
   Ethernet frames (Section 5) or LVDS TDM frames
   (Section 6).  The two transports are mutually
   exclusive on a given interface; a single node
   MAY operate both transports on different
   interfaces simultaneously.

   The Physical Layer is the electrical and
   mechanical specification of the medium.
   Ethernet mode uses standard IEEE 802.3 copper
   or fiber optic media.  LVDS mode uses a single
   differential twisted pair conforming to
   TIA/EIA-644 [TIA644].

3.2.  Transport Modes

   AudioBus defines two transport modes:

   3.2.1.  Ethernet Mode

   In Ethernet mode, AudioBus packets are
   encapsulated in IEEE 802.3 frames using
   EtherType 0x88B6 (Section 5.2).  Nodes connect
   to commodity Layer 2 switches in star or tree
   topologies.  Communication uses multicast MAC
   addresses (Section 5.3) with dedicated groups
   for discovery/control, PTP synchronization, and
   per-stream audio delivery.

   Up to 64 nodes are supported on a single
   Ethernet segment.  The practical limit is
   determined by available bandwidth, which depends
   on the aggregate channel count, sample rate, bit
   depth, and packet interval of all active streams
   (Section 5.7).

   All Ethernet-mode nodes are peers.  There is no
   master/slave distinction; the grandmaster role
   is elected dynamically and may migrate between
   nodes.

   3.2.2.  LVDS Mode

   In LVDS mode, audio and control data are
   multiplexed into a synchronous TDM frame
   transmitted over a single LVDS twisted pair
   using 8b10b line coding (Section 6.2).

   Nodes connect in a daisy-chain topology:

     Master <-> Node 1 <-> Node 2 <-> ... <-> N

   One node (node ID 0) is the bus master.  The
   bus master generates the line clock, initiates
   every frame transmission, and computes the slot
   map that assigns bandwidth to each node.  All
   other nodes are slaves.

   Up to 16 nodes (1 master + 15 slaves) are
   supported on a single LVDS chain.  The maximum
   is limited by the frame size and per-hop
   forwarding latency.

   The LVDS link is half-duplex: downstream data
   (master to slaves) and upstream data (slaves to
   master) occupy separate phases within each frame
   (Section 6.6).

   3.2.3.  Bridge Operation

   An implementation MAY bridge between an Ethernet
   segment and an LVDS chain.  A bridge node
   participates in the Ethernet-mode protocol on
   its Ethernet interface and acts as the bus master
   on its LVDS interface.  The bridge translates
   between the two encapsulations, forwarding audio,
   control, and synchronization traffic in both
   directions.

   The bridge MUST propagate the PTP time base from
   the Ethernet grandmaster to the LVDS chain.  If
   the bridge itself is the Ethernet grandmaster,
   the LVDS chain uses the bridge's clock directly.

   Detailed bridge algorithms are outside the scope
   of this specification.

3.3.  Node Roles

   3.3.1.  Ethernet Mode Roles

   In Ethernet mode, every node is a peer.  A node
   MAY assume any combination of the following
   roles:

   Talker:  A node that originates one or more
      audio streams.  A talker transmits
      STREAM_ANNOUNCE and AUDIO packets.

   Listener:  A node that consumes one or more
      audio streams.  A listener transmits
      SUBSCRIBE packets and receives AUDIO packets.

   Grandmaster:  The node elected by the Best
      Master Clock algorithm (Section 7.2) to
      serve as the PTP reference.  The grandmaster
      transmits PTP_SYNC and PTP_FOLLOW_UP messages
      and responds to PTP_DELAY_REQ messages.

   A node MUST be prepared to assume or relinquish
   the grandmaster role at any time as a result of
   BMC re-election.

   3.3.2.  LVDS Mode Roles

   In LVDS mode, nodes have a fixed master/slave
   relationship:

   Bus Master:  Node ID 0.  Generates the line
      clock.  Transmits downstream frame data.
      Receives upstream frame data.  Computes and
      distributes the slot map.  Acts as the PTP
      grandmaster for the chain.

   Slave:  Node IDs 1 through 15.  Receives
      downstream frame data.  Extracts assigned
      audio and tunnel slots.  Inserts upstream
      audio and tunnel data.  Forwards the frame
      to the next node in the chain.

   The master/slave assignment is determined by
   physical wiring and cannot change at runtime
   without rewiring the chain.

3.4.  Stream Model

   A stream is a unidirectional, multi-channel
   audio flow characterized by the following
   immutable and mutable properties:

   Immutable properties (fixed at creation):

   -  Stream ID: a 16-bit unsigned integer, unique
      within the scope of the originating talker.
      Valid range: 0x0001 to 0xFFFF.  The value
      0x0000 is reserved and MUST NOT be used.

   -  Talker UID: the 32-bit Node UID of the
      originating talker.

   -  Sample rate: one of 44100, 48000, 88200, or
      96000 Hz.

   -  Bit depth: one of 16, 24, or 32 bits per
      sample.

   -  Encoding: a single octet identifying the
      sample encoding.  This specification defines
      only encoding 0 (linear PCM).  Values 1
      through 255 are reserved for future use.

   Mutable properties (may change at runtime):

   -  Channel count: 1 to 64.  Changes are
      signaled by a new STREAM_ANNOUNCE with the
      updated count (Section 9.4).

   -  Packet interval: 125, 250, 500, 1000, 2000,
      or 4000 microseconds.  A change in packet
      interval is signaled by a new
      STREAM_ANNOUNCE.

   -  Channel labels: UTF-8 strings identifying
      each channel (e.g., "Left", "Right",
      "Sub").

   A talker MAY publish up to 255 concurrent
   streams.  The practical limit is determined
   by available bandwidth.

   A listener MAY subscribe to up to 255 streams
   simultaneously.

   A listener MAY subscribe to a subset of channels
   within a stream by providing a channel bitmask
   in the SUBSCRIBE message (Section 9.3).  The
   talker transmits all channels regardless of
   subscriber interest; channel selection is
   performed at the listener.

3.5.  Node Lifecycle State Machine

   Every AudioBus node progresses through the
   following states from power-on to steady-state
   operation.  The state machine applies to both
   Ethernet and LVDS modes unless noted.

                  +----------+
                  |          |
                  |   INIT   |
                  |          |
                  +----+-----+
                       |
              link detected
                       |
                       v
                  +----------+
                  |          |
                  | DISCOVER |<--------+
                  |          |         |
                  +----+-----+         |
                       |               |
             GM elected (BMC)          |
                       |               |
                       v               |
                  +----------+         |
                  |          |    GM lost
                  | SYNCING  |    (3x miss)
                  |          +---------+
                  +----+-----+
                       |
             offset < 1 us
                       |
                       v
                  +----------+
                  |          |
                  |  READY   |<-----+
                  |          |      |
                  +----+-----+      |
                       |            |
             streams active    stream
                       |       change
                       v            |
                  +----------+      |
                  |          |      |
                  | RUNNING  +------+
                  |          |
                  +----+-----+
                       |
              link lost / shutdown
                       |
                       v
                  +----------+
                  |          |
                  |   DOWN   |
                  |          |
                  +----------+

     Figure 2: Node Lifecycle State Machine

   INIT:  The node has powered on and is
      initializing hardware.  No packets are sent
      or received.  In Ethernet mode, the node
      waits for link-up on its Ethernet interface.
      In LVDS mode, the slave waits for the master
      to begin frame transmissions.

      Exit condition: physical link is detected.
      Transition to: DISCOVER.

   DISCOVER:  The node begins transmitting BEACON
      packets and listening for BEACON packets
      from other nodes.  It participates in the
      Best Master Clock election (Section 7.2).

      In LVDS mode, a slave enters DISCOVER when
      it first receives a valid TDM frame from the
      master.  The master enters DISCOVER
      immediately upon link-up.

      Exit condition: a grandmaster is elected
      (either self or another node).
      Transition to: SYNCING.

      Timeout: if no grandmaster is elected within
      5000 milliseconds, the node with the lowest
      UID MUST assume the grandmaster role and
      transition to SYNCING.

   SYNCING:  The node is synchronizing its clock
      to the grandmaster.  If this node is the
      grandmaster, SYNCING is instantaneous.  The
      node transmits and receives PTP messages
      (Section 7) and runs the clock servo
      (Section 7.8).

      Exit condition: the clock offset is below
      1 microsecond for at least 3 consecutive
      measurement cycles.
      Transition to: READY.

      Error condition: if the grandmaster becomes
      unreachable (3 consecutive Sync misses,
      375 ms), the node transitions back to
      DISCOVER for BMC re-election.

   READY:  The node's clock is synchronized.  The
      node transmits STREAM_ANNOUNCE for each
      stream it offers.  It may transmit SUBSCRIBE
      for streams it wishes to receive.

      Exit condition: at least one stream is
      actively sending or receiving audio data.
      Transition to: RUNNING.

   RUNNING:  Normal operation.  Audio, tunnel,
      metadata, and latency measurement packets
      flow.  The node continues to maintain PTP
      synchronization and transmit beacons.

      On stream configuration change (channel
      count update, new subscription, or
      unsubscription), the node transitions
      briefly to READY, applies the change, and
      returns to RUNNING.

      Error condition: if the grandmaster is lost,
      transition to DISCOVER.

   DOWN:  The node has lost its physical link or
      is shutting down.  All streams are torn down.
      The node ceases all transmission.

      In LVDS mode, a slave enters DOWN if it has
      not received a valid TDM frame for 10
      consecutive frame periods.  The master enters
      DOWN on explicit shutdown command.

      From DOWN, the node returns to INIT if the
      link is re-established.

3.6.  Timing Model

   AudioBus employs a layered timing model with
   the following time references:

   3.6.1.  PTP Time Base

   All timestamps in AudioBus are expressed in
   nanoseconds relative to the PTP epoch
   (1 January 1970 00:00:00 TAI).  The PTP time
   base is maintained by the grandmaster and
   distributed to all nodes via the Sync/Follow_Up
   and Delay_Req/Delay_Resp exchanges
   (Section 7).

   The target accuracy is less than 1 microsecond
   between any two nodes.  This is sufficient for
   sample-accurate playout at sample rates up to
   96 kHz, where one sample period is
   approximately 10.4 microseconds.

   3.6.2.  Media Clock

   The media clock generates audio sample
   interrupts at the nominal sample rate (e.g.,
   48000 Hz).  The media clock is derived from the
   PTP time base using a phase-locked loop (PLL)
   or numerically controlled oscillator (NCO) as
   described in Section 7.9.

   The media clock MUST have a frequency accuracy
   of +/- 1 ppm relative to the PTP reference.
   The peak-to-peak jitter of the media clock
   MUST NOT exceed 1 nanosecond at the DAC output.

   3.6.3.  Presentation Latency

   The presentation latency is the fixed offset
   added to the current PTP time to compute the
   presentation timestamp for each audio packet:

      PTS = T_now + L_pres

   where T_now is the current PTP time at the
   talker and L_pres is the presentation latency.

   The default presentation latency is 2,000,000
   nanoseconds (2 milliseconds).  The minimum
   presentation latency MUST be sufficient to
   absorb the maximum expected network delay plus
   playout buffer fill time:

      L_pres >= D_net_max + D_buf

   where D_net_max is the worst-case one-way
   network delay and D_buf is the playout buffer
   depth (Section 10.7).

   Implementations SHOULD allow the presentation
   latency to be configured per stream.

   3.6.4.  Timing Diagram

   The following diagram shows the timing
   relationships for a single audio packet in
   Ethernet mode:

   Talker                             Listener
     |                                    |
     | [sample capture]                   |
     |     T_cap                          |
     |                                    |
     | [packetize + stamp]                |
     |     PTS = T_cap + L_pres           |
     |                                    |
     | -------- AUDIO packet ---------> |
     |     (network delay D_net)          |
     |                                    |
     |                    [buffer sample] |
     |                       T_arrive     |
     |                                    |
     |                    [wait for PTS]  |
     |                                    |
     |                    [playout]       |
     |                       T_play = PTS |
     |                                    |

        Figure 3: Audio Packet Timing


4.  Common Packet Format

4.1.  Common Header

   Every AudioBus packet in both transport modes
   begins with a 12-octet common header.  The
   header provides version identification, packet
   type dispatch, source identification, sequence
   numbering, and payload delineation.

    0                   1                   2
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    Version    |   Pkt Type    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            Flags              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                               |
   |         Source UID            |
   |                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Sequence Number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Payload Length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   In 32-bit-aligned bit-ruler format:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    Version    |   Pkt Type    |            Flags              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Source UID                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Sequence Number         |       Payload Length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 4: AudioBus Common Header

   The following paragraphs describe each field.

   Version (octet 0):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 1 to 255.
      Default: 1.

      This field identifies the AudioBus protocol
      version.  This document defines version 1.
      A receiver MUST silently discard any packet
      whose Version field contains a value it does
      not support (Section 14.2).  A receiver MUST
      NOT attempt to parse the payload of a packet
      with an unsupported version.

      The value 0 is reserved and MUST NOT be
      transmitted.  A receiver that encounters
      version 0 MUST discard the packet and
      SHOULD increment a "malformed packet" counter.

   Pkt Type (octet 1):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0x00 to 0xFF, subject to the
         registry in Section 4.3.
      Default: none; this field is always explicitly
         set by the transmitter.

      This field identifies the type of payload
      that follows the common header.  The receiver
      uses this field to dispatch the packet to
      the appropriate processing function.

      A receiver MUST silently discard any packet
      whose Pkt Type value is not recognized
      (Section 14.1).  A receiver MUST NOT
      attempt to interpret the payload of an
      unrecognized packet type.

   Flags (octets 2-3):

      Type: 16-bit bitfield.
      Size: 2 octets.
      Default: 0x0000.

      The Flags field is a 16-bit bitfield with
      the following layout (bit 0 is the most
      significant bit of octet 2):

      Bit 0 (0x8000): VLAN.

         Set to 1 if the Ethernet frame carrying
         this packet includes an IEEE 802.1Q VLAN
         tag.  Set to 0 otherwise.  In LVDS mode,
         this bit MUST be 0.

         If this bit is set but no VLAN tag is
         present in the encapsulating frame, the
         receiver MUST process the packet normally
         but SHOULD log a warning.

      Bits 1-3 (0x7000): Reserved.

         These bits are reserved for future use.
         A transmitter MUST set these bits to 0.
         A receiver MUST ignore these bits.

      Bits 4-9 (0x0FC0): DSCP.

         A 6-bit Differentiated Services Code
         Point value indicating the desired QoS
         treatment (Section 5.5).  In LVDS mode,
         this field is ignored; transmitters
         SHOULD set it to 0.

         Valid range: 0 to 63.  If the value is
         outside this range (which cannot occur
         in a 6-bit field), the receiver MUST
         treat it as 0.

      Bits 10-15 (0x003F): Reserved.

         Reserved for future use.  A transmitter
         MUST set these bits to 0.  A receiver
         MUST ignore these bits.

   Source UID (octets 4-7):

      Type: unsigned 32-bit integer.
      Size: 4 octets.
      Valid range: 0x00000001 to 0xFFFFFFFF.
      Default: derived from MAC address
         (Section 8.3).

      This field contains the Node UID of the
      node that originated this packet.  The
      Source UID is used by receivers to identify
      the sender, to correlate streams with their
      talkers, and to detect duplicate nodes on
      the network.

      The value 0x00000000 is reserved and MUST
      NOT appear in the Source UID field.  A
      receiver that encounters Source UID 0 MUST
      discard the packet and SHOULD log a warning.

      Forwarding nodes (e.g., LVDS slaves
      retransmitting frames) MUST NOT modify the
      Source UID field.  The Source UID always
      identifies the originator, not the
      forwarder.

   Sequence Number (octets 8-9):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 65535.
      Default: 0 for the first packet; incremented
         by 1 for each subsequent packet from the
         same source.

      This field is a per-source monotonically
      increasing counter.  It increments by one
      for each packet transmitted by the source,
      regardless of packet type.  The counter
      wraps from 65535 to 0.

      The sequence number allows receivers to
      detect packet loss (gaps in the sequence),
      packet duplication (repeated sequence
      numbers), and packet reordering (out-of-
      order sequence numbers).

      For AUDIO packets, loss detection is
      critical.  Listeners MUST track expected
      sequence numbers and report gaps as loss
      events (Section 10.6).

      For non-AUDIO packets, sequence number
      tracking is OPTIONAL but RECOMMENDED for
      diagnostic purposes.

   Payload Length (octets 10-11):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 1476 (Ethernet mode) or
         0 to the remaining frame capacity minus
         4 octets for the CRC (LVDS mode).
      Default: determined by the packet type.

      This field specifies the number of octets
      in the payload that immediately follows
      the 12-octet common header.  It does not
      include the common header itself.

      A Payload Length of 0 is valid and indicates
      a header-only packet (used by some control
      messages).

      A receiver MUST verify that Payload Length
      is consistent with the encapsulating frame
      size.  In Ethernet mode, the total frame
      payload (common header + payload) MUST NOT
      exceed 1488 octets (12 + 1476).  If the
      Payload Length field indicates more data
      than is actually present in the frame, the
      receiver MUST discard the packet.

      In LVDS mode, the Payload Length MUST NOT
      exceed the capacity of the current TDM
      frame's auxiliary data region.

4.2.  ABNF for Common Header

   The following ABNF grammar [RFC5234] formally
   defines the common header and associated types.

   ; --- Core field types ---

   uint8        = OCTET
   uint16       = 2OCTET
   uint32       = 4OCTET
   uint64       = 8OCTET

   ; --- Common header ---

   common-hdr   = version pkt-type flags
                  source-uid seq-num payload-len

   version      = uint8
                  ; value 1 for this specification

   pkt-type     = uint8
                  ; see packet-type-value

   flags        = uint16
                  ; bitfield, see Section 4.1

   source-uid   = uint32
                  ; non-zero

   seq-num      = uint16

   payload-len  = uint16
                  ; 0..1476 in Ethernet mode

   ; --- Packet type values ---

   packet-type-value =
       %x01 /  ; AUDIO
       %x02 /  ; BEACON
       %x03 /  ; SUBSCRIBE
       %x04 /  ; UNSUBSCRIBE
       %x05 /  ; STREAM_ANNOUNCE
       %x06 /  ; STREAM_DELETE
       %x10 /  ; PTP_SYNC
       %x11 /  ; PTP_FOLLOW_UP
       %x12 /  ; PTP_DELAY_REQ
       %x13 /  ; PTP_DELAY_RESP
       %x20 /  ; TUNNEL
       %x30 /  ; METADATA
       %x40 /  ; PING
       %x41    ; PONG

   ; --- Flag bits ---
   ;
   ; Bit  0 (MSB of flags): VLAN present
   ; Bits 1-3: reserved (zero)
   ; Bits 4-9: DSCP (6 bits)
   ; Bits 10-15: reserved (zero)

   ; --- Full AudioBus packet ---

   audiobus-pkt = common-hdr payload

   payload      = *OCTET
                  ; length determined by
                  ; payload-len field

      Figure 5: ABNF Grammar for Common Header

4.3.  Packet Type Registry

   The following table enumerates all packet types
   defined by this specification.

   +---------+------------------+-----------+
   | Value   | Name             | Reference |
   +---------+------------------+-----------+
   | 0x00    | Reserved         | --        |
   | 0x01    | AUDIO            | Sec 10.1  |
   | 0x02    | BEACON           | Sec 8.1   |
   | 0x03    | SUBSCRIBE        | Sec 9.3   |
   | 0x04    | UNSUBSCRIBE      | Sec 9.3   |
   | 0x05    | STREAM_ANNOUNCE  | Sec 9.1   |
   | 0x06    | STREAM_DELETE    | Sec 9.5   |
   | 0x07-0F | Reserved         | --        |
   | 0x10    | PTP_SYNC         | Sec 7.3   |
   | 0x11    | PTP_FOLLOW_UP    | Sec 7.4   |
   | 0x12    | PTP_DELAY_REQ    | Sec 7.5   |
   | 0x13    | PTP_DELAY_RESP   | Sec 7.6   |
   | 0x14-1F | Reserved (PTP)   | --        |
   | 0x20    | TUNNEL           | Sec 11.1  |
   | 0x21-2F | Reserved (Tun.)  | --        |
   | 0x30    | METADATA         | Sec 12.1  |
   | 0x31-3F | Reserved (Meta)  | --        |
   | 0x40    | PING             | Sec 13.1  |
   | 0x41    | PONG             | Sec 13.2  |
   | 0x42-4F | Reserved (Diag)  | --        |
   | 0x50-7F | Private Use      | Sec 15.3  |
   | 0x80-FF | Reserved         | --        |
   +---------+------------------+-----------+

      Table 1: AudioBus Packet Type Registry

   Values in the Reserved ranges MUST NOT be
   transmitted by conformant implementations.
   Receivers MUST silently discard packets with
   reserved type values.

   Values in the Private Use range (0x50-0x7F)
   MAY be used by vendors for proprietary
   extensions.  Receivers that do not recognize a
   private-use type MUST silently discard the
   packet.  Private-use packet types MUST NOT be
   registered with IANA.

4.4.  Byte Ordering

   All multi-octet integer fields in AudioBus
   packets are encoded in network byte order
   (big-endian), with the most significant octet
   transmitted first, per [RFC791].

   Audio sample data is also encoded in big-endian
   byte order (most significant octet first) within
   each sample word (Section 10.2).

   Implementations on little-endian processors
   MUST perform byte swapping when reading or
   writing multi-octet fields.

4.5.  Maximum Packet Size

   The maximum AudioBus payload size is
   constrained by the encapsulating transport:

   Ethernet mode:

      The maximum Ethernet frame payload (after
      the EtherType) is 1500 octets.  Subtracting
      the 12-octet common header and an optional
      12-octet 802.1Q double-tag, the maximum
      AudioBus payload is:

         1500 - 12 = 1488 octets (no VLAN)
         1500 - 12 - 4 = 1484 octets (single tag)

      To ensure interoperability, implementations
      MUST NOT transmit AudioBus packets with a
      total size (header + payload) exceeding
      1488 octets.  Jumbo frames are NOT supported
      by this specification.

   LVDS mode:

      The maximum payload is determined by the
      TDM frame size minus the fixed overhead
      (frame header, sideband, guard, CRC).  See
      Section 6.3 for frame capacity calculations.

   Implementations MUST NOT fragment AudioBus
   packets across multiple Ethernet frames or
   LVDS TDM frames.  Each packet MUST fit entirely
   within a single encapsulating frame.

4.6.  Packet Integrity

   In Ethernet mode, packet integrity is provided
   by the IEEE 802.3 Frame Check Sequence (FCS),
   which is a CRC-32 computed and verified by the
   Ethernet hardware.  No additional integrity
   check is applied by the AudioBus protocol layer.

   In LVDS mode, each TDM frame carries a CRC-32
   computed over all octets from the frame header
   through the last data octet before the CRC
   field (Section 6.9).  The CRC polynomial is
   0x04C11DB7, identical to IEEE 802.3 FCS.

   A receiver that detects a CRC failure MUST
   discard the entire frame and SHOULD increment
   a "CRC error" counter.  The receiver MUST NOT
   attempt to extract audio or control data from
   a frame with a CRC failure.


5.  Ethernet Transport

5.1.  Frame Encapsulation

   In Ethernet mode, each AudioBus packet is
   encapsulated in a single IEEE 802.3 Ethernet
   frame.  The frame layout is as follows:

   +-------------------------------------------+
   | Preamble + SFD          |  8 octets       |
   +-------------------------------------------+
   | Destination MAC Address |  6 octets       |
   +-------------------------------------------+
   | Source MAC Address      |  6 octets       |
   +-------------------------------------------+
   | [802.1Q VLAN Tag]       |  4 octets (opt) |
   +-------------------------------------------+
   | EtherType (0x88B6)      |  2 octets       |
   +-------------------------------------------+
   | AudioBus Common Header  | 12 octets       |
   +-------------------------------------------+
   | AudioBus Payload        | variable        |
   +-------------------------------------------+
   | Padding (if needed)     | 0-N octets      |
   +-------------------------------------------+
   | Frame Check Sequence    |  4 octets       |
   +-------------------------------------------+

        Figure 6: Ethernet Frame Layout

   Preamble and SFD:  Generated and consumed by
      the Ethernet PHY.  Not part of the AudioBus
      specification.

   Destination MAC Address:  A multicast MAC
      address as defined in Section 5.3.  Unicast
      destination addresses MUST NOT be used for
      AudioBus packets; all communication is
      multicast.

   Source MAC Address:  The MAC address of the
      transmitting interface.  This MUST be the
      globally unique (OUI-based) MAC address
      assigned to the interface by its
      manufacturer.  Locally administered source
      MAC addresses MUST NOT be used.

   802.1Q VLAN Tag:  Present only if VLAN tagging
      is enabled (Section 5.4).  When present,
      the VLAN flag (bit 0) in the AudioBus
      Flags field MUST be set.

   EtherType:  The 2-octet value 0x88B6
      (Section 5.2).

   AudioBus Common Header:  The 12-octet header
      defined in Section 4.1.

   AudioBus Payload:  The packet type-specific
      payload.  Length is given by the Payload
      Length field in the common header.

   Padding:  If the total frame data (from
      Destination MAC through Payload) is fewer
      than 46 octets, the Ethernet hardware or
      driver appends zero-valued padding octets
      to reach the minimum frame size.  AudioBus
      receivers MUST tolerate padding; the Payload
      Length field in the common header indicates
      the actual payload size.

   Frame Check Sequence:  The CRC-32 computed
      by the Ethernet hardware.  See
      Section 4.6.

   An Ethernet frame MUST carry exactly one
   AudioBus packet.  Implementations MUST NOT
   concatenate multiple AudioBus packets in a
   single frame.  Implementations MUST NOT split
   a single AudioBus packet across multiple
   frames.

   The following ABNF defines the Ethernet frame
   structure from the perspective of the AudioBus
   protocol (excluding preamble and FCS, which
   are handled by hardware):

   eth-frame    = dst-mac src-mac [vlan-tag]
                  ethertype audiobus-pkt

   dst-mac      = 6OCTET
   src-mac      = 6OCTET
   vlan-tag     = %x81.00 tci
   tci          = uint16
   ethertype    = %x88.B6

      Figure 7: ABNF for Ethernet Encapsulation

5.2.  EtherType

   AudioBus uses EtherType 0x88B6, which is the
   IEEE 802 Local Experimental EtherType 1.

   This EtherType is reserved by IEEE for local
   experimental use and does not require
   registration for private networks.  A dedicated
   EtherType assignment will be requested from the
   IEEE Registration Authority if this protocol
   proceeds to standardization.

   Implementations MUST transmit EtherType 0x88B6
   in the EtherType field of every AudioBus
   Ethernet frame.

   Implementations MUST NOT process frames with
   any other EtherType as AudioBus packets.

   On shared networks, other protocols MAY use
   the same experimental EtherType.  Receivers
   MUST verify the Version and Pkt Type fields in
   the common header before processing any frame
   received on EtherType 0x88B6.  If the Version
   is not recognized, the frame MUST be discarded.

5.3.  Multicast Addressing

   AudioBus uses locally administered multicast
   MAC addresses.  All AudioBus multicast
   addresses share the 3-octet prefix 01:60:AB.

   Three address classes are defined:

   5.3.1.  Discovery and Control Group

      Address: 01:60:AB:FF:FF:00

      This group carries BEACON, SUBSCRIBE,
      UNSUBSCRIBE, STREAM_ANNOUNCE, STREAM_DELETE,
      TUNNEL, METADATA, PING, and PONG packets.

      All nodes MUST join this group upon
      initialization of the Ethernet interface
      and MUST NOT leave it while the interface
      is active.

   5.3.2.  PTP Synchronization Group

      Address: 01:60:AB:FF:FF:01

      This group carries PTP_SYNC, PTP_FOLLOW_UP,
      PTP_DELAY_REQ, and PTP_DELAY_RESP packets.

      All nodes MUST join this group upon
      initialization and MUST NOT leave it while
      the interface is active.

      Separating PTP traffic from discovery/
      control traffic allows switches that
      support per-group filtering to prioritize
      synchronization packets independently.

   5.3.3.  Audio Stream Groups

      Address: 01:60:AB:HH:LL:00

      HH is the high octet and LL is the low
      octet of the 16-bit Stream ID.  Each active
      stream has a unique multicast address
      derived from its Stream ID.

      A talker transmits AUDIO packets for a
      given stream to that stream's multicast
      address.

      A listener MUST join the multicast group
      for each stream to which it subscribes
      and MUST leave the group when it
      unsubscribes.

      The address 01:60:AB:00:00:00 is reserved
      (Stream ID 0x0000) and MUST NOT be used
      as an audio stream group address.

      Example: Stream ID 0x0042 uses multicast
      address 01:60:AB:00:42:00.

   5.3.4.  Multicast Group Summary

   +-------------------+-------------------------+
   | Address           | Usage                   |
   +-------------------+-------------------------+
   | 01:60:AB:FF:FF:00 | Discovery/Control       |
   | 01:60:AB:FF:FF:01 | PTP Synchronization     |
   | 01:60:AB:HH:LL:00 | Audio (per stream)      |
   | 01:60:AB:00:00:00 | Reserved                |
   +-------------------+-------------------------+

       Table 2: Multicast Address Assignments

   Implementations MUST configure the Ethernet
   interface to pass frames addressed to the
   joined multicast groups.  Implementations
   SHOULD use hardware multicast filtering where
   available, rather than promiscuous mode, to
   minimize CPU load.

5.4.  VLAN Tagging

   Implementations MAY use IEEE 802.1Q VLAN
   tagging to isolate AudioBus traffic from other
   traffic on the same physical network.

   When VLAN tagging is used:

   -  The VLAN ID MUST be configurable.  There is
      no default VLAN ID; if tagging is enabled,
      the ID MUST be explicitly set.

   -  All AudioBus nodes on the same network MUST
      use the same VLAN ID.

   -  The Priority Code Point (PCP) field SHOULD
      be set to 6 (Class 6, equivalent to
      Internetwork Control) for all AudioBus
      frames.

   -  The Drop Eligible Indicator (DEI) MUST be
      set to 0.

   -  The VLAN flag (bit 0 of the Flags field in
      the common header) MUST be set to 1.

   When VLAN tagging is not used:

   -  No 802.1Q tag is inserted in the frame.

   -  The VLAN flag (bit 0 of the Flags field)
      MUST be 0.

   The VLAN tag, when present, is inserted between
   the Source MAC Address and the EtherType field,
   per IEEE 802.1Q.  The Tag Protocol Identifier
   (TPID) is 0x8100.

   Implementations MUST function correctly on
   networks that do not support VLAN tagging.

5.5.  Quality of Service

   Implementations SHOULD set Differentiated
   Services Code Point (DSCP) values in the Flags
   field (bits 4-9) according to the following
   table:

   +------------------------+------+----------+
   | Packet Type(s)         | DSCP | PHB      |
   +------------------------+------+----------+
   | PTP_SYNC               |   48 | CS6      |
   | PTP_FOLLOW_UP          |   48 | CS6      |
   | PTP_DELAY_REQ          |   48 | CS6      |
   | PTP_DELAY_RESP         |   48 | CS6      |
   | AUDIO                  |   46 | EF       |
   | TUNNEL                 |   34 | AF41     |
   | BEACON                 |    0 | BE       |
   | SUBSCRIBE              |    0 | BE       |
   | UNSUBSCRIBE            |    0 | BE       |
   | STREAM_ANNOUNCE        |    0 | BE       |
   | STREAM_DELETE          |    0 | BE       |
   | METADATA               |    0 | BE       |
   | PING                   |    0 | BE       |
   | PONG                   |    0 | BE       |
   +------------------------+------+----------+

       Table 3: DSCP Assignments by Packet Type

   The DSCP value is purely advisory within the
   AudioBus header.  Switches that do not inspect
   the AudioBus header will not honor these values
   unless the 802.1Q PCP field is also set
   appropriately (Section 5.4).

   Implementations SHOULD use both the DSCP field
   in the AudioBus header and the PCP field in
   the VLAN tag (if present) to maximize the
   probability that QoS-aware switches will
   prioritize time-critical traffic.

   Implementations MUST function correctly on
   networks that provide no QoS differentiation.
   The playout buffer (Section 10.7) provides the
   necessary tolerance for packet delay variation
   on best-effort networks.

5.6.  Coexistence with IP Traffic

   AudioBus uses EtherType 0x88B6, which is
   distinct from all IANA-assigned EtherTypes
   including IPv4 (0x0800), IPv6 (0x86DD), and
   ARP (0x0806).  AudioBus frames are therefore
   invisible to IP protocol stacks that filter
   by EtherType.

   An implementation MUST NOT interfere with IP
   protocol stacks on the same interface.  In
   particular:

   -  AudioBus MUST NOT modify the ARP table.

   -  AudioBus MUST NOT generate IGMP messages
      (it uses Ethernet-level multicast group
      management, not IP multicast).

   -  AudioBus MUST NOT consume UDP or TCP ports.

   -  AudioBus MUST NOT alter IP routing tables.

   AudioBus and IP traffic MAY share the same
   physical port and switch infrastructure.
   On shared networks, implementations SHOULD
   use VLAN tagging (Section 5.4) to isolate
   AudioBus traffic when total bandwidth exceeds
   50% of link capacity.

5.7.  Bandwidth Budget

   Implementations MUST self-police bandwidth by
   limiting aggregate audio packet rate to the
   declared stream parameters.

   The bandwidth consumed by a single audio stream
   is computed as follows:

      N_samp  = interval_us * Fs / 1000000

      P_audio = N_samp * C * (D / 8)

      P_total = OH_eth + OH_ab + OH_audio + P_audio

      BW_bps  = (P_total * 8) / (interval_us / 1e6)

   Where:

      interval_us  = packet interval in microseconds
      Fs           = sample rate in Hz
      C            = channel count
      D            = bit depth in bits
      N_samp       = samples per channel per packet
      P_audio      = audio payload in octets
      OH_eth       = Ethernet overhead: 14 octets
                     (or 18 with VLAN tag)
      OH_ab        = AudioBus common header:
                     12 octets
      OH_audio     = Audio packet header: 20 octets
      P_total      = total frame payload in octets
      BW_bps       = bandwidth in bits per second

   Example: 8 channels, 32-bit, 48 kHz, 1 ms
   interval:

      N_samp  = 1000 * 48000 / 1000000 = 48
      P_audio = 48 * 8 * 4 = 1536 octets

      NOTE: This exceeds the Ethernet MTU.  The
      implementation MUST either reduce the packet
      interval or use fewer channels per stream.

   Example: 8 channels, 32-bit, 48 kHz, 250 us:

      N_samp  = 250 * 48000 / 1000000 = 12
      P_audio = 12 * 8 * 4 = 384 octets
      P_total = 14 + 12 + 20 + 384 = 430 octets
      BW_bps  = (430 * 8) / 0.000250
             = 13,760,000 bps = 13.76 Mbps

   Implementations MUST verify that the total
   bandwidth of all active streams does not
   exceed 80% of the link capacity.  If adding
   a new stream would exceed this limit, the
   implementation MUST refuse the stream and
   SHOULD report an error to the application
   layer.

   The 80% limit reserves bandwidth for
   non-audio traffic (beacons, PTP, tunnel,
   metadata, ping/pong) and for IP traffic
   sharing the same link.

5.8.  Switch Requirements

   AudioBus operates with any IEEE 802.3-
   compliant Layer 2 switch.  The following
   switch features are RECOMMENDED but not
   required:

   -  IGMP snooping disabled or configured to
      pass unknown multicast groups.  AudioBus
      does not use IGMP; switches that drop
      unregistered multicast groups will block
      AudioBus traffic.  Implementations SHOULD
      document this requirement for network
      administrators.

   -  Store-and-forward mode (as opposed to
      cut-through) for deterministic latency.

   -  Support for IEEE 802.1Q VLAN tagging.

   -  Support for Priority Code Point (PCP)
      scheduling with at least two priority
      queues.

   -  Port-based rate limiting to prevent
      non-AudioBus traffic from starving
      AudioBus streams.

   Managed switches that support static multicast
   group registration SHOULD be configured to
   forward AudioBus multicast groups only to
   ports where AudioBus nodes are connected.

   AudioBus does NOT require:

   -  AVB/TSN switch features (802.1AS, 802.1Qav,
      802.1Qat, 802.1CB).

   -  IGMP snooping.

   -  Spanning Tree Protocol (AudioBus does not
      use redundant paths).

   -  Any Layer 3 (IP) routing capability.


6.  LVDS Serial Transport

6.1.  Physical Layer

   The LVDS transport uses Low-Voltage
   Differential Signaling conforming to TIA/EIA-
   644 [TIA644] over a single twisted pair.

   6.1.1.  Electrical Characteristics

   +---------------------------+-------------+
   | Parameter                 | Value       |
   +---------------------------+-------------+
   | Differential output       |             |
   |   voltage swing           | 250-450 mV  |
   | Common-mode output        |             |
   |   voltage                 | 1.125-1.375V|
   | Differential impedance    | 100 ohms    |
   |   (nominal)               |   +/- 10%   |
   | Output rise/fall time     | < 1.5 ns    |
   | Receiver input threshold  | +/- 100 mV  |
   +---------------------------+-------------+

       Table 4: LVDS Electrical Parameters

   6.1.2.  Cabling

   Cable: Category 5e unshielded twisted pair
      (UTP) or better, 100-ohm differential
      impedance.  Shielded twisted pair (STP) MAY
      be used in high-EMI environments.

   Connector: The connector type is not mandated
      by this specification.  Implementations
      SHOULD use standard RJ45 connectors for
      ease of deployment with commodity cabling.

   Maximum segment length: 15 meters at the
      maximum line rate (49.152 MHz symbol rate
      for 48 kHz family).  Longer cables MAY
      work at reduced reliability; implementations
      SHOULD monitor CRC error rates and warn the
      user if errors exceed one per 100,000 frames.

   6.1.3.  Termination

   Each end of the LVDS link MUST be terminated
   with a 100-ohm (+/- 5%) differential resistor.
   The resistor MUST be placed within 10 mm of
   the receiver input pins on the circuit board.

   Failure to terminate correctly results in
   signal reflections that degrade the bit error
   rate, particularly at segment lengths
   approaching 15 meters.

   6.1.4.  Transceiver

   Each node MUST use a transceiver that provides
   both an LVDS driver and an LVDS receiver on the
   same differential pair, with a driver-enable
   (DE) control signal.

   When the node is receiving (not driving the
   bus), the driver MUST be disabled (DE
   deasserted) to present high impedance to the
   line.

   The driver-enable signal MUST respond within
   10 nanoseconds of assertion or deassertion.

   Typical parts: SN65LVDT41 (TI), MAX9113 (ADI),
   or equivalent.

   6.1.5.  Line Rate

   The line rate depends on the audio sample rate
   family and the TDM frame size:

   +----------+--------+---------------------+
   | Fs (Hz)  | Frame  | Symbol Rate (Hz)    |
   |          | (oct)  |                     |
   +----------+--------+---------------------+
   | 48000    |  1024  | 48000*1024*10/8     |
   |          |        | = 61,440,000        |
   | 96000    |   512  | 96000*512*10/8      |
   |          |        | = 61,440,000        |
   | 44100    |  1024  | 44100*1024*10/8     |
   |          |        | = 56,448,000        |
   | 88200    |   512  | 88200*512*10/8      |
   |          |        | = 56,448,000        |
   +----------+--------+---------------------+

       Table 5: Line Rate by Sample Rate

   The 10/8 factor accounts for 8b10b encoding
   overhead.  All rates are within the capability
   of standard LVDS transceivers.

6.2.  8b10b Line Coding

   All data on the LVDS link is encoded using the
   8b10b line code defined in IEEE 802.3
   [IEEE802.3] Clause 36.

   8b10b encoding maps each 8-bit data byte to a
   10-bit symbol, providing DC balance and
   guaranteed transition density for clock
   recovery.

   6.2.1.  Data Characters

   Data characters (D-characters) encode payload
   bytes.  Each D-character is denoted Dx.y where
   x is the value of the low 5 bits and y is the
   value of the high 3 bits.  The full encoding
   table is defined in IEEE 802.3 Clause 36 and
   is not reproduced here.

   6.2.2.  Control Characters (K-characters)

   AudioBus uses the following K-characters for
   framing and link management:

   +---------+-------+----------------------------+
   | Symbol  | Value | Usage                      |
   +---------+-------+----------------------------+
   | K28.5   | 0xBC  | Comma: byte alignment      |
   |         |       | and frame synchronization. |
   |         |       | Also used as idle fill.    |
   +---------+-------+----------------------------+
   | K28.1   | 0x3C  | Start of Frame (SOF):      |
   |         |       | marks the beginning of a   |
   |         |       | TDM frame payload.         |
   +---------+-------+----------------------------+
   | K27.7   | 0xFB  | Direction Turnaround:      |
   |         |       | signals the boundary       |
   |         |       | between downstream and     |
   |         |       | upstream phases.           |
   +---------+-------+----------------------------+
   | K28.3   | 0x7C  | Idle: transmitted during   |
   |         |       | guard time and when the    |
   |         |       | bus has no data to send.   |
   +---------+-------+----------------------------+
   | K29.7   | 0xFD  | End of Frame (EOF):        |
   |         |       | marks the end of the TDM   |
   |         |       | frame payload.             |
   +---------+-------+----------------------------+
   | K23.7   | 0xF7  | Discovery Beacon: used     |
   |         |       | during LVDS link init to   |
   |         |       | detect connected nodes.    |
   +---------+-------+----------------------------+

       Table 6: K-Character Assignments

   6.2.3.  Running Disparity

   The 8b10b encoder MUST maintain running
   disparity (RD) as defined in IEEE 802.3
   Clause 36.  The initial running disparity at
   link initialization is negative (RD-).

   A receiver that detects a running disparity
   error MUST NOT discard the frame immediately
   but MUST count the error.  If disparity errors
   exceed 10 per second, the receiver SHOULD
   report a link quality degradation warning to
   the application.

   6.2.4.  Comma Detection and Alignment

   The receiver MUST use the K28.5 comma character
   for byte alignment.  The unique bit pattern of
   K28.5 (0011111 or 1100000 in the 10-bit domain)
   cannot appear within any pair of adjacent data
   or control symbols, guaranteeing unambiguous
   alignment.

   Upon initial link-up, the receiver MUST acquire
   comma alignment within 10 symbol periods.

   If comma alignment is lost (no K28.5 detected
   for 100 consecutive symbols), the receiver MUST
   declare a loss-of-sync condition, discard all
   data until alignment is reacquired, and
   increment a "sync loss" counter.

6.3.  TDM Frame Structure

   In LVDS mode, one TDM frame is transmitted per
   audio sample period.  The frame carries audio
   samples, tunnel data, and sideband information
   for all nodes in the daisy chain.

   Frame sizes are fixed for each sample rate
   family:

   -  1024 octets for 48000 Hz and 44100 Hz.
   -   512 octets for 96000 Hz and 88200 Hz.

   The frame structure is as follows:

   +------+--------+----------------------------+
   | Byte | Length | Field                      |
   +------+--------+----------------------------+
   |    0 |      1 | K28.5 (Comma)              |
   |    1 |      1 | K28.1 (Start of Frame)     |
   |    2 |      8 | Frame Header (Sec 6.4)     |
   |   10 |      1 | Downstream Sideband        |
   |   11 |   var  | Downstream Audio Slots     |
   |      |   var  | Downstream Aux Data        |
   |      |      1 | K28.5 (Guard preamble)     |
   |      |      1 | K27.7 (Turnaround)         |
   |      |   var  | K28.3 (Guard idle fill)    |
   |      |      1 | Upstream Sideband          |
   |      |   var  | Upstream Audio Slots       |
   |      |   var  | Upstream Aux Data          |
   | F-5  |      1 | K29.7 (End of Frame)       |
   | F-4  |      4 | CRC-32                     |
   +------+--------+----------------------------+

       Table 7: TDM Frame Layout
       (F = frame size in octets)

         Figure 8: TDM Frame Structure

   6.3.1.  Downstream Phase

   The downstream phase begins immediately after
   the frame header and carries data from the
   master to all slaves.  It contains:

   -  Downstream Sideband: 1 octet of sideband
      control data (used for slot map distribution,
      status polling, and similar management
      functions as described in Section 11.6).

   -  Downstream Audio Slots: audio sample data
      for channels originating at the master,
      packed contiguously according to the slot
      map (Section 6.5).

   -  Downstream Aux Data: tunnel and metadata
      payloads destined for slaves, formatted as
      AudioBus packets with the common header.

   6.3.2.  Guard

   The guard separates the downstream and upstream
   phases.  It consists of:

   -  K28.5: one comma symbol to re-establish
      byte alignment at the receiver.

   -  K27.7: one direction turnaround symbol
      signaling the transition.

   -  K28.3 (repeated): idle fill symbols for
      CDR re-lock.  The number of idle symbols
      is:

         N_idle = ceil(T_guard * F_sym / 10) - 2

      where T_guard is the guard time in seconds
      (minimum 650 ns, corresponding to 32 symbol
      periods at 61.44 MHz), and F_sym is the
      symbol rate in Hz.  The "-2" accounts for
      the K28.5 and K27.7 symbols.

      At the minimum guard time of 650 ns:

         N_idle = ceil(650e-9 * 61440000 / 10) - 2
                = ceil(4.0) - 2
                = 2

      Total guard overhead: 4 symbols = 4 octets
      (after 8b10b decoding).

   6.3.3.  Upstream Phase

   The upstream phase carries data from slaves to
   the master.  Its structure mirrors the
   downstream phase:

   -  Upstream Sideband: 1 octet.
   -  Upstream Audio Slots: audio data from slave
      nodes.
   -  Upstream Aux Data: tunnel and metadata
      payloads from slaves.

   6.3.4.  Frame Capacity Calculation

   The available capacity for audio and aux data
   within a single TDM frame is:

      C_total = F - OH_fixed

   where F is the frame size (1024 or 512 octets)
   and OH_fixed is the sum of all fixed-size
   fields:

      OH_fixed = 1 (K28.5)
               + 1 (K28.1)
               + 8 (frame header)
               + 1 (downstream sideband)
               + 4 (guard: K28.5+K27.7+2*K28.3)
               + 1 (upstream sideband)
               + 1 (K29.7)
               + 4 (CRC-32)
               = 21 octets

   Therefore:

      C_total(1024) = 1024 - 21 = 1003 octets
      C_total(512)  =  512 - 21 =  491 octets

   This capacity is split between downstream and
   upstream audio and aux data, as determined by
   the slot map.

   At 32-bit depth, each channel requires 4 octets
   per sample.  The maximum channel count for
   symmetric (equal up/down) allocation:

      Ch_max(1024) = floor(1003 / 2 / 4) = 125
      Ch_max(512)  = floor( 491 / 2 / 4) =  61

   These exceed the protocol maximum of 64
   channels per direction, so the 64-channel
   limit is enforced by the protocol, not the
   frame capacity.

   ABNF for TDM frame:

   tdm-frame   = comma sof frame-hdr
                  dn-sideband dn-audio dn-aux
                  guard
                  up-sideband up-audio up-aux
                  eof crc32

   comma        = %xBC        ; K28.5
   sof          = %x3C        ; K28.1
   frame-hdr    = 8OCTET      ; Section 6.4
   dn-sideband  = OCTET
   dn-audio     = *OCTET      ; per slot map
   dn-aux       = *OCTET      ; per slot map
   guard        = %xBC %xFB *(%x7C)
                  ; K28.5, K27.7, K28.3...
   up-sideband  = OCTET
   up-audio     = *OCTET      ; per slot map
   up-aux       = *OCTET      ; per slot map
   eof          = %xFD        ; K29.7
   crc32        = 4OCTET

      Figure 9: ABNF for TDM Frame

6.4.  Frame Header

   The 8-octet LVDS frame header follows the
   Start-of-Frame (K28.1) symbol and precedes all
   audio and auxiliary data.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Frame Counter           |  Frame Type   |    Flags      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Node Count   |DN Audio Slots |UP Audio Slots | SlotMap CRC   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

           Figure 10: LVDS Frame Header

   The following paragraphs describe each field.

   Frame Counter (octets 0-1):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 65535.
      Default: 0 at power-on; increments by 1
         for each frame.

      A monotonically increasing counter that
      wraps from 65535 to 0.  The master
      increments this counter for each TDM frame
      it initiates.

      Slaves MUST NOT modify the Frame Counter.

      Slaves use the Frame Counter to detect
      missed frames.  A gap in the counter
      indicates one or more frames were lost.
      On detection of a gap, the slave MUST
      substitute silence for the missing audio
      samples and increment a "frame loss"
      counter.

   Frame Type (octet 2):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0x00 to 0x03.
      Default: 0x00 (Normal).

      Identifies the purpose of this frame:

      0x00 - Normal: contains audio and/or
         auxiliary data per the active slot map.

      0x01 - Discovery: transmitted by the master
         during link initialization to detect
         connected slaves.  Slaves receiving a
         Discovery frame MUST respond with their
         Node UID in the upstream sideband.

      0x02 - Configuration: carries a new slot
         map from the master to all slaves.
         Slaves MUST store the new slot map and
         apply it beginning with the next Normal
         frame.

      0x03 - Status: requests status information
         from all slaves.  Each slave inserts its
         status byte in the upstream sideband at
         its assigned slot.

      Values 0x04 through 0xFF are reserved.  A
      slave that receives a frame with an
      unrecognized Frame Type MUST forward the
      frame unchanged and MUST NOT insert any
      upstream data.

   Flags (octet 3):

      Type: 8-bit bitfield.
      Size: 1 octet.
      Default: 0x00.

      Bits 0-1 (0xC0): Sample Rate Index.

         Encodes the active sample rate:
         0 = 48000 Hz, 1 = 96000 Hz,
         2 = 44100 Hz, 3 = 88200 Hz.

         A slave MUST verify that the Sample Rate
         Index matches its own configuration.
         If it does not match, the slave MUST NOT
         insert audio data and MUST set an error
         flag in the upstream sideband.

      Bits 2-3 (0x30): Bit Depth Index.

         Encodes the active bit depth:
         0 = 16 bits, 1 = 24 bits, 2 = 32 bits.
         Value 3 is reserved and MUST NOT be
         transmitted.

         A slave that receives Bit Depth Index 3
         MUST treat it as a configuration error
         and MUST NOT insert audio data.

      Bit 4 (0x08): Sideband Valid.

         Set to 1 if the downstream sideband
         octet contains valid management data.
         Set to 0 if the sideband is unused
         (fill value 0x00).

         Slaves MUST ignore the sideband octet
         if this bit is 0.

      Bits 5-7 (0x07): Reserved.

         MUST be 0 on transmit.  Slaves MUST
         ignore these bits.

   Node Count (octet 4):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 1 to 16.
      Default: 1 (master only).

      The number of active nodes in the chain,
      including the master.  The master sets this
      field based on the most recent discovery.

      A slave whose node ID exceeds
      (Node Count - 1) MUST NOT insert any data
      into the frame.

      If Node Count is 0, the slave MUST discard
      the frame and log an error.

   DN Audio Slots (octet 5):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0 to 255.
      Default: 0.

      The number of audio channel slots in the
      downstream phase.  Each slot occupies
      (bit_depth / 8) octets.  The total
      downstream audio region is:

         DN_bytes = DN_Audio_Slots * (D / 8)

      where D is the bit depth in bits.

      If this value is inconsistent with the slot
      map (i.e., the computed size does not match
      the slot map entries), the slave MUST ignore
      all downstream audio and set an error flag.

   UP Audio Slots (octet 6):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0 to 255.
      Default: 0.

      The number of audio channel slots in the
      upstream phase.  Semantics mirror
      DN Audio Slots.

   SlotMap CRC (octet 7):

      Type: unsigned 8-bit integer (CRC-8).
      Size: 1 octet.
      Valid range: 0x00 to 0xFF.
      Default: 0x00 (no slot map configured).

      The CRC-8 computed over the active slot map
      using polynomial 0x07:

         G(x) = x^8 + x^2 + x + 1

      The CRC is initialized to 0x00, computed
      over each octet of the slot map table in
      the order it was distributed during the
      most recent Configuration frame.

      Slaves MUST compare this CRC to their
      locally stored slot map CRC.  If the values
      differ, the slave MUST request
      reconfiguration by setting the appropriate
      error flag in the upstream sideband and
      MUST NOT insert audio data until the CRC
      matches.

      A SlotMap CRC of 0x00 when Node Count > 1
      indicates that no slot map has been
      distributed yet.  Slaves MUST NOT insert
      audio data in this state.

   ABNF for frame header:

   frame-hdr     = frame-counter frame-type
                   frame-flags node-count
                   dn-audio-slots up-audio-slots
                   slotmap-crc

   frame-counter  = uint16
   frame-type     = uint8
   frame-flags    = uint8
   node-count     = uint8
   dn-audio-slots = uint8
   up-audio-slots = uint8
   slotmap-crc    = uint8

      Figure 11: ABNF for LVDS Frame Header

6.5.  Slot Map

   The slot map is a table that assigns byte
   offsets within the TDM frame to audio channels
   and tunnel streams for each node in the chain.
   It is computed by the bus master during the
   configuration phase and distributed to all
   slaves via Configuration frames (Frame Type
   0x02).

   6.5.1.  Slot Map Entry Format

   Each entry in the slot map is 6 octets:

    0                   1                   2
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        Byte Offset            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Byte Width   |  Channel ID   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Node ID     |  Direction    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

      Figure 12: Slot Map Entry

   Byte Offset (octets 0-1):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 10 to (F - 5), where F is the
         frame size.

      The byte offset within the TDM frame where
      this slot's data begins.  The offset is
      relative to the start of the frame (byte 0
      is the K28.5 comma).

      Offsets 0 through 9 are occupied by the
      comma, SOF, and frame header, and MUST NOT
      be used for audio slots.  Offsets from
      (F - 4) through (F - 1) are occupied by
      the CRC-32, and MUST NOT be used.

   Byte Width (octet 2):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 2, 3, or 4.
      Default: determined by the bit depth.

      The number of octets occupied by one audio
      sample in this slot: 2 for 16-bit, 3 for
      24-bit, 4 for 32-bit audio.

      If the Byte Width does not match the
      bit depth indicated in the frame header
      Flags field, the slot is invalid.  The
      slave MUST NOT read or write data for
      invalid slots and MUST report the error.

   Channel ID (octet 3):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0 to 127.
      Default: 0.

      The logical channel identifier.  Channel
      IDs 0 through 63 correspond to audio
      channels.  Channel IDs 64 through 127 are
      reserved for tunnel data slots.

      The Channel ID is unique within the scope
      of (Node ID, Direction).

   Node ID (octet 4):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0 to 15.
      Default: 0 (master).

      The node that owns this slot.  In the
      downstream direction, the master (Node
      ID 0) writes to slots it owns; slaves
      read from them.  In the upstream direction,
      the slave writes to slots it owns; the
      master reads from them.

   Direction (octet 5):

      Type: unsigned 8-bit integer.
      Size: 1 octet.
      Valid range: 0 or 1.
      Default: 0.

      0 = downstream (master to slaves).
      1 = upstream (slaves to master).

      A value other than 0 or 1 is invalid.  A
      slave receiving an invalid Direction MUST
      ignore the slot entry and report an error.

   6.5.2.  Slot Map Distribution

   The master distributes the slot map using
   Configuration frames (Frame Type 0x02).  The
   slot map entries are placed in the downstream
   audio and aux data region, serialized
   contiguously.

   Because the slot map may exceed the available
   space in a single frame, the master MAY
   fragment it across multiple consecutive
   Configuration frames.  The master MUST set
   the Sideband Valid flag (bit 4) and encode
   the following in the downstream sideband octet:

   -  Bits 0-3: fragment index (0 = first).
   -  Bits 4-7: total fragment count.

   A slave MUST buffer all fragments and assemble
   the complete slot map before applying it.

   The master MUST transmit the complete slot map
   at least twice to ensure all slaves receive
   it (in case of transient errors on the first
   transmission).

   6.5.3.  Slot Map Computation Algorithm

   The master computes the slot map as follows:

   1.  Sort nodes by Node ID (ascending).

   2.  For each node, collect the number of
       downstream and upstream audio channels it
       requires (obtained during discovery).

   3.  Set offset = 11 (first byte after
       downstream sideband).

   4.  For each node in order, allocate
       downstream audio slots:

       For each channel c from 0 to (N_dn - 1):
          Create entry: offset, byte_width,
             c, node_id, 0 (downstream).
          offset += byte_width.

   5.  Allocate downstream aux data region
       (if needed):

       Record dn_aux_offset = offset.
       offset += dn_aux_size.

   6.  Insert guard:

       Record guard_offset = offset.
       offset += guard_size (minimum 4 octets).

   7.  Allocate upstream sideband:

       Record up_sideband_offset = offset.
       offset += 1.

   8.  For each node in order, allocate upstream
       audio slots:

       For each channel c from 0 to (N_up - 1):
          Create entry: offset, byte_width,
             c, node_id, 1 (upstream).
          offset += byte_width.

   9.  Allocate upstream aux data region:

       Record up_aux_offset = offset.
       offset += up_aux_size.

   10. Verify: offset + 1 (EOF) + 4 (CRC) <= F.

       If not, reduce channel counts or aux data
       allocation and repeat from step 3.

   11. Compute CRC-8 over the serialized slot map
       table.

   ABNF for slot map:

   slot-map      = *slot-entry

   slot-entry    = byte-offset byte-width
                   channel-id node-id direction

   byte-offset   = uint16
   byte-width    = uint8    ; 2, 3, or 4
   channel-id    = uint8    ; 0..127
   node-id       = uint8    ; 0..15
   direction     = uint8    ; 0 or 1

      Figure 13: ABNF for Slot Map

6.6.  Half-Duplex Operation

   The LVDS link is half-duplex on a single
   twisted pair.  Within each TDM frame, the
   downstream phase precedes the upstream phase.

   6.6.1.  Direction Change Sequence

   The transition from downstream to upstream is
   signaled by the following sequence:

   1.  The master (or current transmitter in the
       downstream direction) transmits K28.5
       (comma).

   2.  Immediately following, it transmits K27.7
       (direction turnaround).

   3.  The transmitter MUST disable its LVDS
       driver within 10 nanoseconds of completing
       the K27.7 symbol transmission.

   4.  All transmitters remain idle for the
       guard time.  During this period, the
       bus is undriven (high impedance).

   5.  The first upstream transmitter (the slave
       at the end of the chain) enables its
       driver and begins transmitting K28.3 idle
       fill for at least 2 symbols, followed by
       upstream data.

   6.6.2.  Guard Time

   The minimum guard time is 650 nanoseconds,
   corresponding to 32 symbol periods at the
   61.44 MHz symbol rate.  This time allows the
   clock and data recovery (CDR) circuit at each
   receiver to re-acquire lock on the new
   transmitter's signal.

   The guard time formula is:

      T_guard = N_guard * T_sym

   where N_guard is the number of guard symbols
   (minimum 32, including K28.5, K27.7, and
   K28.3 fills) and T_sym is the symbol period:

      T_sym = 1 / F_sym

   At 61.44 MHz:

      T_sym   = 16.276 ns
      T_guard = 32 * 16.276 ns = 520.8 ns

   Rounded up to the minimum of 650 ns, the
   guard is 40 symbol periods.

   Implementations SHOULD monitor CRC error rates
   after direction changes.  If the error rate
   exceeds 1 per 10,000 frames, the
   implementation SHOULD increase the guard time
   by 8 symbol periods and re-evaluate.

   6.6.3.  Bus Turnaround Timing

   Transmitter                 Receiver
      |                           |
      |  [K28.5] [K27.7]         |
      |                           |
      |  driver OFF               |
      |      (<10 ns)             |
      |                           |
      |  === guard time ===       |
      |  (min 650 ns / 40 sym)   |
      |                           |
      |         new TX driver ON  |
      |         [K28.3] [K28.3]  |
      |         [upstream data]  |
      |                           |

      Figure 14: Bus Turnaround Timing

6.7.  Daisy-Chain Forwarding

   Nodes in an LVDS chain connect as follows:

     Master <-> Node 1 <-> Node 2 <-> ... <-> N

   Each intermediate node performs store-and-
   forward: it receives the entire downstream
   phase, extracts its assigned channels, inserts
   upstream data in the appropriate slots, and
   retransmits the frame to the next node.

   6.7.1.  Forwarding Latency

   Each hop adds one frame period of latency in
   each direction:

      L_hop = 1 / Fs

   At 48 kHz, L_hop = 20.833 microseconds.

   The total one-way latency for H hops is:

      L_oneway = H * L_hop = H / Fs

   The total round-trip latency is:

      L_rtt = 2 * H / Fs

   Examples:

   +------+--------+----------+-----------+
   | Hops | Fs     | One-Way  | Round-Trip|
   |      | (Hz)   | (us)     | (us)      |
   +------+--------+----------+-----------+
   |    1 | 48000  |    20.83 |     41.67 |
   |    4 | 48000  |    83.33 |    166.67 |
   |    8 | 48000  |   166.67 |    333.33 |
   |   15 | 48000  |   312.50 |    625.00 |
   |    1 | 96000  |    10.42 |     20.83 |
   |    8 | 96000  |    83.33 |    166.67 |
   +------+--------+----------+-----------+

      Table 8: Daisy-Chain Latency

   6.7.2.  Forwarding Rules

   An intermediate slave MUST:

   -  Retransmit the downstream phase verbatim to
      the next node, including all audio slots,
      aux data, and the guard sequence.

   -  Extract its own audio channels from the
      downstream audio region (as identified by
      the slot map) during retransmission.

   -  Insert its upstream audio data into the
      correct slots (as identified by the slot
      map) during the upstream phase.

   -  NOT modify any fields in the frame header.

   -  NOT modify any audio slots belonging to
      other nodes.

   -  Retransmit the upstream phase from
      downstream nodes verbatim.

   The last node in the chain (Node N) does not
   retransmit downstream.  It receives the
   downstream phase, extracts its channels, then
   initiates the upstream phase after the guard
   time.

   6.7.3.  Chain Break Detection

   If a slave fails to receive a valid frame
   within 10 consecutive frame periods
   (10 / Fs seconds), it MUST:

   -  Declare a "chain break" condition.
   -  Cease all audio output (mute).
   -  Transition to the DOWN state
      (Section 3.5).

   The master detects a downstream chain break by
   not receiving upstream data from one or more
   expected slaves within 10 frame periods.  The
   master MUST then re-run discovery to determine
   which nodes remain reachable.

6.8.  LVDS Node State Machine

   LVDS nodes follow the general lifecycle
   described in Section 3.5 with the following
   LVDS-specific substates:

   Master:

     IDLE -> DISCOVERY -> CONFIG -> RUNNING

   Slave:

     IDLE -> WAIT_FRAME -> SYNCED -> RUNNING

                   +----------+
                   |   IDLE   |
                   +----+-----+
                        |
               line rate locked
                        |
            +-----------+-----------+
            |                       |
         [Master]               [Slave]
            |                       |
            v                       v
      +----------+           +------------+
      | DISCOVERY|           | WAIT_FRAME |
      +----+-----+           +-----+------+
           |                       |
      all nodes found       valid frame rcvd
           |                       |
           v                       v
      +----------+           +----------+
      | CONFIG   |           | SYNCED   |
      +----+-----+           +-----+----+
           |                       |
      slot map sent         slot map valid
           |                       |
           v                       v
      +----------+           +----------+
      | RUNNING  |           | RUNNING  |
      +----+-----+           +-----+----+
           |                       |
      chain break            frame loss
      or shutdown            or shutdown
           |                       |
           v                       v
      +----------+           +----------+
      |   IDLE   |           |   IDLE   |
      +----------+           +----------+

   Figure 15: LVDS Node State Machine

   IDLE:  The node has powered on but has not yet
      locked to the line rate.  The master
      transmits K28.5 commas continuously.  The
      slave listens for commas.

   DISCOVERY (master only):  The master transmits
      Discovery frames (Frame Type 0x01) and
      listens for slave responses in the upstream
      sideband.  Discovery continues for at least
      100 ms or until no new slaves are detected
      for 3 consecutive frames.

   WAIT_FRAME (slave only):  The slave has
      achieved comma alignment and is waiting for
      the first valid frame (correct K28.1 SOF
      and valid CRC).

   CONFIG (master only):  The master computes the
      slot map (Section 6.5.3) and distributes it
      via Configuration frames.

   SYNCED (slave only):  The slave has received
      and validated the slot map (SlotMap CRC
      matches).

   RUNNING:  Normal operation.  Audio and auxiliary
      data flow per the slot map.

6.9.  Error Detection

   6.9.1.  CRC-32

   Each TDM frame carries a CRC-32 in its last
   4 octets.  The CRC is computed over all octets
   from the frame header (byte 2) through the
   last data octet before the CRC field.

   CRC polynomial: 0x04C11DB7

      G(x) = x^32 + x^26 + x^23 + x^22 + x^16
           + x^12 + x^11 + x^10 + x^8  + x^7
           + x^5  + x^4  + x^2  + x    + 1

   Initial value: 0xFFFFFFFF.
   Final XOR: 0xFFFFFFFF.
   Bit order: MSB first.

   This is identical to the CRC used by IEEE
   802.3 for the Frame Check Sequence.

   A receiver that detects a CRC mismatch MUST
   discard the entire frame, substitute silence
   for all audio channels, and increment a
   "CRC error" counter.

   6.9.2.  8b10b Code Violations

   A code violation occurs when a received 10-bit
   symbol does not correspond to any valid 8b10b
   data or control character.

   On detection of a code violation, the receiver
   MUST:

   -  Discard the current frame.
   -  Increment a "code violation" counter.
   -  Attempt to re-align using the next K28.5
      comma.

   6.9.3.  Running Disparity Errors

   A running disparity error occurs when the
   received symbol has the correct encoding but
   the wrong disparity (positive instead of
   negative or vice versa).

   Running disparity errors are less severe than
   code violations.  The receiver SHOULD count
   them but SHOULD NOT discard the frame unless
   the CRC also fails.

   6.9.4.  Frame Counter Gaps

   A gap in the Frame Counter (Section 6.4)
   indicates one or more frames were lost in
   transit.  The slave MUST:

   -  Substitute silence for all audio channels
      during the missing frame period(s).
   -  Increment a "frame loss" counter.
   -  Report the gap to the application layer.


7.  Clock Synchronization

7.1.  PTP Profile

   AudioBus defines a constrained Precision Time
   Protocol (PTP) profile based on IEEE 1588-2019
   [IEEE1588].  This profile operates within the
   AudioBus packet encapsulation (using the
   AudioBus common header and PTP-specific packet
   types) and does not interoperate with standard
   PTP profiles or gPTP (IEEE 802.1AS).

   7.1.1.  Profile Parameters

   +-------------------------------+--------------+
   | Parameter                     | Value        |
   +-------------------------------+--------------+
   | Transport                     | AudioBus     |
   |                               | over IEEE    |
   |                               | 802.3 (L2)   |
   | Delay mechanism               | End-to-End   |
   | Operation mode                | Two-step     |
   | Clock domain                  | 0            |
   | Sync interval                 | 125 ms       |
   | Follow_Up interval            | (immediate)  |
   | Delay_Req interval            | 500 ms       |
   | Announce interval             | (via BEACON) |
   | Priority1                     | N/A          |
   | Priority2                     | 1-255        |
   |                               | (default 128)|
   | Clock class                   | 6, 13,       |
   |                               | or 248       |
   | Hardware timestamping         | REQUIRED     |
   | Timestamp resolution          | <= 10 ns     |
   | Target accuracy               | < 1 us       |
   | Maximum path asymmetry        | < 100 ns     |
   | Multicast address             | See Sec 5.3  |
   +-------------------------------+--------------+

       Table 9: PTP Profile Parameters

   7.1.2.  Differences from Default PTP Profile

   The AudioBus PTP profile differs from the
   IEEE 1588 default profile in the following
   ways:

   -  Transport: AudioBus packets over Ethernet
      L2 (not UDP/IPv4 or UDP/IPv6).

   -  Encapsulation: AudioBus common header
      (Section 4.1) instead of PTP header.

   -  Announce messages: not used.  Grandmaster
      election parameters are carried in BEACON
      packets (Section 8.1).

   -  Domain: fixed at 0.  Multi-domain
      operation is not supported.

   -  Sync interval: fixed at 125 ms (log
      interval -3 in PTP terms is 125 ms when
      base is 1 second; here the value is
      specified directly in milliseconds).

   -  One-step mode: not supported.  Two-step
      mode is REQUIRED.

   -  Peer-to-peer delay: not supported.  Only
      end-to-end delay measurement is used.

   7.1.3.  PTP in LVDS Mode

   In LVDS mode, clock synchronization is
   inherent: the bus master's clock directly
   drives the frame timing.  Slaves recover the
   clock from the incoming data stream using their
   CDR circuitry.

   PTP messages are NOT transmitted on the LVDS
   link.  The LVDS frame timing IS the clock
   reference.  See Section 7.11 for details.

7.2.  Grandmaster Election

   The grandmaster (GM) is elected using the
   Best Master Clock (BMC) algorithm.  The BMC
   comparison is performed by every node
   independently, using information from received
   BEACON packets (Section 8.1).

   7.2.1.  Comparison Criteria

   The BMC algorithm compares candidates using
   the following criteria, in strict priority
   order:

   1.  Clock Class (lower value wins):

       -  6: The node's clock is locked to an
          external reference (e.g., GPS receiver,
          atomic clock, word clock input from a
          trusted source).

       -  13: Application-specific clock source,
          better than free-running but not
          traceable to a primary reference.

       -  248: Free-running local oscillator
          (default for nodes without external
          reference).

       If two candidates have different Clock
       Class values, the one with the lower
       value wins.

   2.  Priority (lower value wins):

       An 8-bit unsigned integer, range 1 to 255.
       Default: 128.

       This field allows system integrators to
       influence grandmaster election without
       requiring an external clock reference.  A
       node with Priority 1 is strongly preferred
       over one with Priority 128.

       The value 0 is reserved and MUST NOT be
       used.

       If two candidates have equal Clock Class
       but different Priority values, the one
       with the lower Priority wins.

   3.  Node UID (lower value wins):

       If Clock Class and Priority are both
       equal, the node with the numerically
       lowest Node UID wins.  This is a
       deterministic tiebreaker that guarantees
       exactly one grandmaster is elected.

   7.2.2.  Election Procedure

   1.  Each node maintains a table of all known
       nodes and their (Clock Class, Priority,
       Node UID) tuples, obtained from received
       BEACON packets.

   2.  Each node independently evaluates the BMC
       comparison for all entries in its table,
       including its own entry.

   3.  The winning node is the grandmaster.

   4.  If a node determines that it is the
       grandmaster, it MUST begin transmitting
       PTP_SYNC packets at the Sync interval
       (125 ms) within one Sync interval of the
       determination.

   5.  If a node determines that another node is
       the grandmaster, it MUST synchronize its
       clock to that grandmaster using the
       Sync/Follow_Up and Delay_Req/Delay_Resp
       exchanges described in Sections 7.3
       through 7.6.

   6.  A node MUST NOT transmit PTP_SYNC or
       PTP_FOLLOW_UP packets unless it is the
       current grandmaster.

   7.2.3.  Grandmaster Loss

   A node MUST declare grandmaster loss if it has
   not received a PTP_SYNC packet from the current
   grandmaster for 3 consecutive Sync intervals
   (375 ms).

   Upon grandmaster loss:

   1.  The node removes the former grandmaster
       from its node table (if no BEACON has been
       received within the node expiry timeout).

   2.  The node re-runs the BMC algorithm over
       all remaining entries.

   3.  If the node itself is the new winner, it
       assumes the grandmaster role and begins
       PTP_SYNC transmission.

   4.  If another node is the new winner, the
       node begins synchronizing to the new
       grandmaster.

   5.  The transition MUST complete within one
       Sync interval (125 ms) of the BMC re-
       evaluation.

   During the transition period (up to 500 ms
   from the last valid Sync to the first Sync
   from the new grandmaster), nodes MUST continue
   audio playout using their free-running local
   clock.  The clock servo (Section 7.8) MUST
   re-acquire lock within 2 seconds of the first
   Sync from the new grandmaster.

   7.2.4.  Grandmaster Preemption

   If a node with a superior (Clock Class,
   Priority, UID) tuple appears on the network
   (e.g., a node with an external GPS reference
   boots up), the BMC algorithm will elect it as
   the new grandmaster.

   The transition proceeds as follows:

   1.  All nodes receive the new node's BEACON
       and re-evaluate BMC.

   2.  The new grandmaster begins PTP_SYNC
       transmission.

   3.  The former grandmaster detects the
       superior node and ceases PTP_SYNC
       transmission within one Sync interval.

   4.  All nodes begin synchronizing to the new
       grandmaster.

   During preemption, there is a brief period
   (up to one Sync interval = 125 ms) where two
   grandmasters may be transmitting PTP_SYNC.
   Nodes MUST accept PTP_SYNC only from the node
   they have elected as grandmaster and MUST
   silently discard PTP_SYNC from other sources.

7.3.  Sync Message (Type 0x10)

   The PTP_SYNC message is transmitted by the
   grandmaster at every Sync interval (125 ms).
   It is the first step of the two-step clock
   synchronization exchange.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       PTP Sequence            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |              Origin Timestamp (64 bits, ns)                   |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

           Figure 16: PTP Sync Payload

   Payload size: 12 octets.

   PTP Sequence (octets 0-1):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 65535.
      Default: 0 for the first Sync; increments
         by 1 for each subsequent Sync.

      A monotonically increasing counter that
      identifies this Sync/Follow_Up pair.  The
      grandmaster increments this counter by one
      for each Sync message it transmits.  The
      counter wraps from 65535 to 0.

      The PTP Sequence is independent of the
      Sequence Number in the common header.  The
      common header Sequence Number counts all
      packets from the source; the PTP Sequence
      counts only PTP exchanges.

      A slave uses the PTP Sequence to match a
      Follow_Up message (Section 7.4) to its
      corresponding Sync message.  A Follow_Up
      whose PTP Sequence does not match the most
      recently received Sync MUST be discarded.

   Reserved (octets 2-3):

      Type: 16-bit field.
      Size: 2 octets.
      Valid range: 0x0000.
      Default: 0x0000.

      Reserved for future use.  A transmitter
      MUST set this field to 0x0000.  A receiver
      MUST ignore this field.

   Origin Timestamp (octets 4-11):

      Type: signed 64-bit integer.
      Size: 8 octets.
      Valid range: any non-negative value
         representing nanoseconds since the PTP
         epoch.
      Default: the grandmaster's best estimate
         of the current PTP time at the moment
         of transmission.

      In two-step mode, this timestamp is a
      coarse estimate.  The precise hardware-
      captured departure timestamp is carried
      in the subsequent Follow_Up message
      (Section 7.4).  The Origin Timestamp
      SHOULD be as close as possible to the
      actual departure time to assist receivers
      in coarse time alignment before the
      Follow_Up arrives.

      If the Origin Timestamp contains a
      negative value, the receiver MUST discard
      the packet.

   Transmission rules:

   -  The grandmaster MUST transmit PTP_SYNC at
      intervals of 125 ms +/- 5 ms.

   -  PTP_SYNC MUST be sent to the PTP
      synchronization multicast group
      (01:60:AB:FF:FF:01).

   -  The hardware MUST capture the precise
      departure timestamp (t1) as the frame
      passes the MAC/PHY boundary.

   -  A Follow_Up message carrying t1 MUST be
      transmitted within 10 ms of the Sync
      departure.

   Reception rules:

   -  A slave MUST record the hardware-captured
      arrival timestamp (t2) when the Sync frame
      is received at the MAC/PHY boundary.

   -  The slave MUST store the (PTP Sequence, t2)
      pair for correlation with the subsequent
      Follow_Up.

   -  If a new Sync arrives before the Follow_Up
      for the previous Sync, the slave MUST
      discard the incomplete pair and begin
      tracking the new Sync.

   ABNF for Sync payload:

   sync-payload  = ptp-seq reserved-16
                   origin-ts

   ptp-seq       = uint16
   reserved-16   = %x00.00
   origin-ts     = uint64    ; nanoseconds

      Figure 17: ABNF for Sync Payload

7.4.  Follow_Up Message (Type 0x11)

   The PTP_FOLLOW_UP message is transmitted by
   the grandmaster immediately after the
   corresponding Sync message.  It carries the
   precise hardware-captured departure timestamp
   of the Sync.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       PTP Sequence            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |         Precise Origin Timestamp (64 bits, ns)                |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        Figure 18: PTP Follow_Up Payload

   Payload size: 12 octets.

   PTP Sequence (octets 0-1):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 65535.

      This field MUST contain the same value as
      the PTP Sequence field in the corresponding
      Sync message.  A slave uses this field to
      match the Follow_Up to its Sync.

      If a slave receives a Follow_Up whose PTP
      Sequence does not match the PTP Sequence
      of the most recently received Sync from
      the same source, the slave MUST discard
      the Follow_Up.

   Reserved (octets 2-3):

      Type: 16-bit field.
      Size: 2 octets.
      Valid range: 0x0000.
      Default: 0x0000.

      Reserved for future use.  Transmitter MUST
      set to 0x0000.  Receiver MUST ignore.

   Precise Origin Timestamp (octets 4-11):

      Type: signed 64-bit integer.
      Size: 8 octets.
      Valid range: any non-negative value
         representing nanoseconds since the PTP
         epoch.

      This is the hardware-captured timestamp of
      the exact instant the Sync frame's first
      octet crossed the MAC/PHY boundary of the
      grandmaster's Ethernet interface.  This
      timestamp is denoted "t1" in the offset
      computation (Section 7.7).

      The Precise Origin Timestamp MUST have a
      resolution of 10 nanoseconds or better
      (Section 7.10).

      If the Precise Origin Timestamp contains
      a negative value, the receiver MUST discard
      the packet.

      If the Precise Origin Timestamp differs
      from the Origin Timestamp in the
      corresponding Sync by more than 10
      milliseconds, the receiver SHOULD log a
      warning (this may indicate a software
      timestamping fallback rather than hardware
      timestamping).

   Transmission rules:

   -  The grandmaster MUST transmit PTP_FOLLOW_UP
      within 10 ms of the corresponding Sync.

   -  PTP_FOLLOW_UP MUST be sent to the PTP
      synchronization multicast group
      (01:60:AB:FF:FF:01).

   -  The PTP Sequence MUST match the preceding
      Sync.

   Reception rules:

   -  A slave MUST match the Follow_Up to the
      most recent unmatched Sync using the PTP
      Sequence field.

   -  Upon successful matching, the slave records
      t1 = Precise Origin Timestamp and t2 =
      the locally captured arrival timestamp of
      the Sync.  The slave now has the (t1, t2)
      pair needed for offset computation
      (Section 7.7).

   -  If the slave cannot match the Follow_Up
      (e.g., the Sync was missed), the slave
      MUST discard the Follow_Up and wait for
      the next Sync/Follow_Up pair.

   ABNF for Follow_Up payload:

   followup-payload = ptp-seq reserved-16
                      precise-origin-ts

   precise-origin-ts = uint64  ; nanoseconds

      Figure 19: ABNF for Follow_Up Payload

7.5.  Delay_Req Message (Type 0x12)

   The PTP_DELAY_REQ message is transmitted by a
   slave to the grandmaster to measure the
   one-way network delay.  Together with the
   Delay_Resp from the grandmaster, it provides
   the (t3, t4) timestamp pair needed for offset
   and delay computation (Section 7.7).

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       PTP Sequence            |           Reserved            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |              Origin Timestamp (64 bits, ns)                   |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        Figure 20: PTP Delay_Req Payload

   Payload size: 12 octets.

   The format is identical to the Sync message
   (Figure 16).

   PTP Sequence (octets 0-1):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 65535.

      A per-slave counter, incremented by one
      for each Delay_Req the slave transmits.
      Wraps from 65535 to 0.

      This counter is independent of the
      grandmaster's PTP Sequence counter.  The
      grandmaster uses this value in the
      Delay_Resp to allow the slave to match
      the response.

   Reserved (octets 2-3):

      Type: 16-bit field.
      Size: 2 octets.
      Default: 0x0000.

      Reserved for future use.  Transmitter
      MUST set to 0x0000.  Receiver MUST ignore.

   Origin Timestamp (octets 4-11):

      Type: signed 64-bit integer.
      Size: 8 octets.

      The slave's best estimate of the current
      PTP time at the moment of transmission.
      This value is informational; the precise
      departure timestamp (t3) is captured
      locally by the slave's hardware
      timestamping unit and is NOT transmitted
      on the wire.

      The grandmaster does not use this field
      for clock computation.  It MAY use it for
      diagnostic purposes (e.g., to estimate
      the slave's current offset before the
      delay measurement completes).

   Transmission rules:

   -  A slave MUST transmit PTP_DELAY_REQ at
      intervals of 500 ms +/- 50 ms.

   -  Slaves SHOULD randomize the initial
      Delay_Req transmission time to avoid
      all slaves transmitting simultaneously.
      The randomization window is 0 to 500 ms
      after the first successful
      Sync/Follow_Up reception.

   -  PTP_DELAY_REQ MUST be sent to the PTP
      synchronization multicast group
      (01:60:AB:FF:FF:01).

   -  The slave's hardware MUST capture the
      precise departure timestamp (t3) as the
      frame passes the MAC/PHY boundary.

   -  The slave MUST store the (PTP Sequence,
      t3) pair locally for correlation with the
      subsequent Delay_Resp.

   -  A slave MUST NOT transmit PTP_DELAY_REQ
      before it has received at least one valid
      Sync/Follow_Up pair from the grandmaster.

   ABNF for Delay_Req payload:

   delay-req-payload = ptp-seq reserved-16
                       origin-ts

      Figure 21: ABNF for Delay_Req Payload

7.6.  Delay_Resp Message (Type 0x13)

   The PTP_DELAY_RESP message is transmitted by
   the grandmaster in response to a Delay_Req
   from a slave.  It carries the hardware-
   captured arrival timestamp of the Delay_Req
   at the grandmaster (t4) and identifies the
   requesting slave.

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

        Figure 22: PTP Delay_Resp Payload

   Payload size: 16 octets.

   PTP Sequence (octets 0-1):

      Type: unsigned 16-bit integer.
      Size: 2 octets.
      Valid range: 0 to 65535.

      This field MUST contain the same PTP
      Sequence value as the Delay_Req that
      triggered this response.  The requesting
      slave uses this field to match the
      response to its request.

      If a slave receives a Delay_Resp whose
      PTP Sequence does not match any outstanding
      Delay_Req from that slave, the slave MUST
      discard the Delay_Resp.

   Reserved (octets 2-3):

      Type: 16-bit field.
      Size: 2 octets.
      Default: 0x0000.

      Reserved for future use.  Transmitter
      MUST set to 0x0000.  Receiver MUST ignore.

   Receive Timestamp (octets 4-11):

      Type: signed 64-bit integer.
      Size: 8 octets.

      The hardware-captured timestamp of the
      exact instant the Delay_Req frame's first
      octet crossed the MAC/PHY boundary of the
      grandmaster's Ethernet interface.  This
      timestamp is denoted "t4" in the offset
      computation (Section 7.7).

      The Receive Timestamp MUST have a
      resolution of 10 nanoseconds or better
      (Section 7.10).

      If the Receive Timestamp contains a
      negative value, the slave MUST discard
      the packet.

   Requester UID (octets 12-15):

      Type: unsigned 32-bit integer.
      Size: 4 octets.
      Valid range: 0x00000001 to 0xFFFFFFFF.

      The Node UID of the slave that transmitted
      the Delay_Req.  The grandmaster copies this
      value from the Source UID field of the
      received Delay_Req's common header.

      A slave MUST verify that the Requester UID
      matches its own Node UID before using the
      Receive Timestamp.  If the UID does not
      match, the slave MUST silently discard the
      Delay_Resp (it is addressed to a different
      slave).

      Since Delay_Resp is sent to the PTP
      multicast group, all slaves receive every
      Delay_Resp.  The Requester UID field
      provides the necessary demultiplexing.

   Transmission rules:

   -  The grandmaster MUST transmit a
      PTP_DELAY_RESP within 10 ms of receiving
      the corresponding Delay_Req.

   -  The grandmaster MUST capture the arrival
      timestamp (t4) using hardware timestamping
      when the Delay_Req frame arrives at the
      MAC/PHY boundary.

   -  PTP_DELAY_RESP MUST be sent to the PTP
      synchronization multicast group
      (01:60:AB:FF:FF:01).

   -  The PTP Sequence MUST match the Delay_Req.

   -  The Requester UID MUST match the
      Delay_Req's Source UID.

   ABNF for Delay_Resp payload:

   delay-resp-payload = ptp-seq reserved-16
                        receive-ts requester-uid

   receive-ts     = uint64  ; nanoseconds
   requester-uid  = uint32

      Figure 23: ABNF for Delay_Resp Payload

7.7.  Offset and Delay Computation

   After a complete measurement cycle (one
   Sync/Follow_Up exchange and one Delay_Req/
   Delay_Resp exchange), a slave has four
   hardware-captured timestamps:

   t1:  Precise departure time of the Sync at
        the grandmaster (from Follow_Up).

   t2:  Arrival time of the Sync at the slave
        (captured locally).

   t3:  Departure time of the Delay_Req at the
        slave (captured locally).

   t4:  Arrival time of the Delay_Req at the
        grandmaster (from Delay_Resp).

   7.7.1.  Offset Computation

   The clock offset (the difference between the
   slave's clock and the grandmaster's clock) is:

      offset = ((t2 - t1) - (t4 - t3)) / 2

   A positive offset means the slave's clock is
   ahead of the grandmaster.  A negative offset
   means the slave's clock is behind.

   7.7.2.  Delay Computation

   The mean one-way network propagation delay is:

      delay = ((t2 - t1) + (t4 - t3)) / 2

   This assumes symmetric path delay (the
   propagation time from grandmaster to slave
   equals the propagation time from slave to
   grandmaster).  The AudioBus PTP profile
   requires that path asymmetry be less than
   100 nanoseconds (Table 9).

   7.7.3.  Timestamp Validation

   Before computing offset and delay, the slave
   MUST validate the timestamps:

   -  t1 MUST be non-negative.
   -  t2 MUST be non-negative.
   -  t3 MUST be non-negative.
   -  t4 MUST be non-negative.
   -  t2 MUST be greater than t1 (assuming no
      initial offset; if t2 < t1, the offset is
      very large and the computation is still
      valid).
   -  The computed delay MUST be positive.  If
      delay <= 0, the measurement is invalid
      and MUST be discarded.
   -  The computed delay SHOULD be less than
      10 milliseconds.  If delay >= 10 ms, the
      measurement SHOULD be flagged as suspect
      but SHOULD NOT be discarded unless it
      exceeds 100 ms.

   7.7.4.  Fixed-Point Arithmetic

   All timestamp computations MUST be performed
   using 64-bit signed integer arithmetic in
   nanoseconds.  No floating-point arithmetic
   is required.

   The division by 2 in the offset and delay
   formulas is an arithmetic right shift by 1
   bit.  For signed integers, this preserves
   the sign.

   Intermediate values (t2 - t1) and (t4 - t3)
   may exceed the range of a 32-bit integer.
   Implementations MUST use 64-bit arithmetic
   throughout.

7.8.  Clock Servo

   The clock servo disciplines the slave's local
   clock to track the grandmaster's clock using
   the offset measurements from Section 7.7.

   7.8.1.  Servo Architecture

   The RECOMMENDED servo architecture is a
   proportional-integral (PI) controller:

      correction = Kp * offset + Ki * integral

   where:

      Kp       = proportional gain
      Ki       = integral gain
      offset   = current measured offset (ns)
      integral = running sum of offsets

   The correction is applied to the local clock
   by adjusting the frequency of the local
   oscillator (NCO or PLL).

   7.8.2.  Recommended Gain Values

   +--------------------+----------+
   | Parameter          | Value    |
   +--------------------+----------+
   | Kp (proportional)  | 1/16     |
   | Ki (integral)      | 1/256    |
   | Max correction     | +/- 100  |
   |   per step (ns)    |   ns     |
   | Initial step mode  | hard set |
   +--------------------+----------+

      Table 10: Servo Parameters

   These values are RECOMMENDED.  Implementations
   MAY use different values provided the servo
   converges to within 1 microsecond within 2
   seconds of the first valid Sync/Follow_Up
   pair.

   7.8.3.  Initial Synchronization

   On first lock (the first valid offset
   measurement), the slave SHOULD set its local
   clock directly to the grandmaster's time
   (hard step) rather than using the PI
   controller.  This avoids a long convergence
   time when the initial offset is large (e.g.,
   seconds or more).

   After the hard step, the servo switches to PI
   control for fine-grained tracking.

   7.8.4.  Filtering

   Implementations SHOULD filter raw offset and
   delay measurements before feeding them to the
   servo.  The RECOMMENDED filter is an
   exponential moving average (EMA):

      filtered = (1 - alpha) * filtered_prev
                 + alpha * raw

   where alpha = 1/16 (equivalent to a 16-sample
   window).

   Implementations SHOULD also discard outlier
   measurements.  A measurement is an outlier if
   the raw offset differs from the filtered
   offset by more than 10 microseconds.  Outlier
   rejection prevents transient network events
   (e.g., switch congestion) from disturbing the
   clock.

   7.8.5.  Holdover

   If PTP messages from the grandmaster cease
   (e.g., due to network failure), the slave MUST
   continue generating the media clock using its
   free-running local oscillator.  This is called
   holdover mode.

   In holdover mode:

   -  The servo MUST freeze its integral term at
      the last known value.

   -  The local oscillator continues at the last
      corrected frequency.

   -  Audio playout continues uninterrupted.

   -  The clock accuracy degrades at a rate
      determined by the oscillator stability
      (typically 20-50 ppm for a standard
      crystal).

   Holdover mode persists until either:

   -  A new grandmaster is elected and PTP
      resumes (normal recovery).

   -  The holdover duration exceeds 10 seconds,
      at which point the node SHOULD declare a
      "clock unsynchronized" warning to the
      application.

7.9.  Media Clock Recovery

   Each node MUST derive its audio sample clock
   from the PTP time base.  This ensures that
   all nodes in the network generate audio
   samples at precisely the same rate,
   eliminating drift and the need for sample
   rate conversion.

   7.9.1.  Clock Recovery Methods

   Two methods are acceptable:

   Phase-Locked Loop (PLL):

      A hardware PLL locks its voltage-controlled
      oscillator (VCO) to the PTP time base.
      The PLL input is typically a pulse-per-
      second or pulse-per-sample signal derived
      from the local PTP clock.

      The PLL loop bandwidth SHOULD be between
      1 Hz and 10 Hz to reject high-frequency
      jitter while tracking low-frequency drift.

   Numerically Controlled Oscillator (NCO):

      A software or hardware NCO generates
      sample clock edges based on a phase
      accumulator driven by the PTP time base.
      At each PTP measurement update, the NCO
      frequency is adjusted to correct any
      accumulated phase error.

      The NCO resolution MUST be at least
      32 bits for the phase accumulator.

   7.9.2.  Jitter Requirements

   The peak-to-peak jitter of the recovered
   media clock, measured at the digital-to-
   analog converter (DAC) input, MUST NOT
   exceed:

   -  1 nanosecond peak-to-peak for 24-bit and
      32-bit audio.

   -  5 nanoseconds peak-to-peak for 16-bit
      audio.

   These requirements ensure that clock jitter
   does not degrade the signal-to-noise ratio
   below the resolution of the audio data.

   The relationship between clock jitter and
   SNR degradation for a full-scale sinusoidal
   signal at frequency f is:

      SNR_jitter = -20 * log10(2 * pi * f * Tj)

   where Tj is the RMS jitter.  For 1 ns peak-
   to-peak (approximately 0.3 ns RMS) at
   20 kHz:

      SNR = -20 * log10(2 * 3.14159 * 20000
                        * 0.3e-9)
          = -20 * log10(3.77e-5)
          = 88.5 dB

   This exceeds the 24-bit dynamic range of
   approximately 144 dB by a comfortable margin,
   confirming that 1 ns jitter is adequate.

7.10. Hardware Timestamping

   Conformant implementations MUST capture PTP
   event message timestamps at the MAC/PHY
   boundary using hardware timestamping.

   7.10.1.  Requirements

   -  Timestamps MUST be captured at the point
      where the frame's Start-of-Frame Delimiter
      (SFD) crosses the MII/GMII/RGMII boundary
      between the MAC and the PHY.

   -  Timestamp resolution MUST be 10 nanoseconds
      or better.  A resolution of 1 nanosecond
      is RECOMMENDED.

   -  The timestamping mechanism MUST NOT
      introduce more than 100 nanoseconds of
      non-deterministic error (i.e., the
      timestamping jitter must be less than
      100 ns peak-to-peak).

   -  Timestamps MUST be captured for both
      transmitted and received PTP event messages
      (PTP_SYNC and PTP_DELAY_REQ).

   -  PTP_FOLLOW_UP and PTP_DELAY_RESP are
      general messages, not event messages.
      They do not require hardware timestamping.

   7.10.2.  Software Timestamping Fallback

   Software timestamping (capturing timestamps
   in the device driver or application layer)
   is NOT RECOMMENDED because it introduces
   non-deterministic delays due to interrupt
   latency, context switching, and cache effects.

   However, an implementation that cannot provide
   hardware timestamping MAY use software
   timestamping as a fallback, with the following
   constraints:

   -  The node MUST set its Clock Class to 248
      (free-running) to ensure it is not elected
      grandmaster.

   -  The node's Priority MUST be 255 (lowest).

   -  The achieved accuracy will be on the order
      of 10-100 microseconds, which is
      sufficient for some use cases (e.g.,
      sound reinforcement with large venue
      delays) but not for sample-accurate
      playout at 96 kHz.

   -  The node MUST advertise "software
      timestamping" in its BEACON metadata so
      that other nodes and management tools can
      identify nodes with degraded clock
      quality.

7.11. LVDS Clock Synchronization

   In LVDS mode, clock synchronization is
   achieved through the inherent timing of the
   TDM frame structure rather than through
   PTP message exchange.

   7.11.1.  Master Clock

   The bus master generates the line clock from
   its local oscillator.  This clock directly
   determines the frame rate and thus the audio
   sample rate.

   The master's oscillator MUST have a frequency
   accuracy of +/- 50 ppm or better.  For
   professional audio applications, +/- 1 ppm
   is RECOMMENDED (achievable with a TCXO or
   OCXO).

   If the master is also connected to an
   Ethernet segment and is synchronized to an
   Ethernet-mode grandmaster, the master's LVDS
   clock MUST be derived from the PTP time base
   using the media clock recovery mechanism
   described in Section 7.9.

   7.11.2.  Slave Clock Recovery

   Each slave recovers the clock from the
   incoming LVDS data stream using its CDR
   circuitry.  The 8b10b coding guarantees
   sufficient transitions for reliable clock
   recovery.

   The recovered clock is used directly as the
   audio sample clock.  No PTP exchange is
   needed on the LVDS link because all slaves
   are inherently synchronized to the master's
   clock through the physical layer.

   7.11.3.  Clock Accuracy

   The clock accuracy between the master and
   any slave on the LVDS chain is limited by:

   -  CDR jitter: typically < 0.5 ns peak-to-
      peak for modern LVDS transceivers.

   -  Accumulated jitter over H hops: grows as
      approximately sqrt(H) * Tj_per_hop.

   For a 15-node chain with 0.5 ns per-hop
   jitter:

      Tj_total = sqrt(15) * 0.5 ns
               = 1.94 ns peak-to-peak

   This is well within the 5 ns requirement for
   16-bit audio and close to the 1 ns
   requirement for 24/32-bit audio.  For chains
   longer than 8 nodes with 32-bit audio,
   implementations SHOULD use a higher-quality
   CDR (< 0.25 ns per hop) or reduce the chain
   length.

   7.11.4.  Sample Rate Accuracy

   The LVDS frame rate is locked to the master's
   oscillator.  If the master uses a crystal with
   +/- 50 ppm accuracy, the sample rate will be:

      Fs_actual = Fs_nominal * (1 +/- 50e-6)

   At 48000 Hz with +50 ppm:

      Fs_actual = 48000 * 1.00005 = 48002.4 Hz

   Over 24 hours, this would accumulate a
   4.32-second drift relative to an external
   reference.  For standalone LVDS chains, this
   drift is typically not audible.  For chains
   bridged to an Ethernet segment, the master
   MUST lock to the PTP time base to eliminate
   drift entirely.

8.  Node Discovery

8.1.  Beacon Format (Type 0x02)

   Every node transmits periodic beacon packets to
   advertise its presence, capabilities, and clock
   state.  The beacon payload follows the common
   header (Section 4.1).

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          Node UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Name Len    |    HW Type    |Talker Streams |Listener Slots |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |PTP Priority   |PTP Clock Class|           Flags               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Max Channels  |  Tunnel Caps  |     Uptime (seconds)          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Node Name (UTF-8)                       ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 8: Beacon Message

   ABNF for beacon payload:

   beacon-payload = node-uid name-len hw-type
                    talker-streams listener-slots
                    ptp-priority ptp-clock-class
                    flags max-channels tunnel-caps
                    uptime node-name

   node-uid       = 4OCTET
   name-len       = OCTET  ; 0..31
   hw-type        = OCTET  ; Table 6
   talker-streams = OCTET  ; 0..255
   listener-slots = OCTET  ; 0..255
   ptp-priority   = OCTET  ; 1..255, default 128
   ptp-clock-class= OCTET  ; 6, 13, or 248
   flags          = 2OCTET ; big-endian
   max-channels   = OCTET  ; 1..64
   tunnel-caps    = OCTET  ; bitmask
   uptime         = 2OCTET ; big-endian, seconds
   node-name      = *31OCTET ; UTF-8

   Field definitions:

   Node UID:  32 bits.  Unique identifier derived per
      Section 8.3.  This field MUST match the Source
      UID in the common header.

   Name Len:  8 bits.  Length of the Node Name field
      in octets.  Range: 0 to 31.  A value of 0
      indicates no name.

   HW Type:  8 bits.  Hardware classification per
      Section 8.4 and Table 6.  Default: 0 (Generic).

   Talker Streams:  8 bits.  Number of streams this
      node currently publishes.  Range: 0 to 255.
      A listener-only node sets this to 0.

   Listener Slots:  8 bits.  Number of additional
      stream subscriptions this node can accept.
      Range: 0 to 255.  A talker-only node sets
      this to 0.

   PTP Priority:  8 bits.  This node's priority for
      grandmaster election.  Lower values win.
      Range: 1 to 255.  Default: 128.  A value of 1
      forces grandmaster.  A value of 255 prevents
      grandmaster election.

   PTP Clock Class:  8 bits.  Quality of this node's
      clock source.

      6:    Primary reference (GPS/atomic).
      13:   Application-specific (e.g., word clock).
      248:  Free-running oscillator (default).

   Flags:  16 bits.  Big-endian.

      Bit 0:   Grandmaster flag.  1 = this node
               is the current grandmaster.
      Bit 1:   LVDS capable.  1 = this node
               supports LVDS transport.
      Bit 2:   Ethernet capable.  1 = this node
               supports Ethernet transport.
      Bit 3:   Bridge node.  1 = this node bridges
               between Ethernet and LVDS.
      Bits 4-15:  Reserved.  MUST be zero on
               transmit.  Receivers MUST ignore.

   Max Channels:  8 bits.  Maximum number of audio
      channels this node can source or sink in any
      single stream.  Range: 1 to 64.

   Tunnel Caps:  8 bits.  Bitmask of supported
      tunnel types.

      Bit 0:  SPI tunnel supported.
      Bit 1:  I2C tunnel supported.
      Bit 2:  GPIO tunnel supported.
      Bit 3:  MIDI tunnel supported.
      Bit 4:  Sideband channel supported.
      Bits 5-7:  Reserved.  MUST be zero.

   Uptime:  16 bits.  Big-endian.  Node uptime in
      seconds since last reset.  Saturates at 65535.

   Node Name:  Variable length, up to 31 octets.
      UTF-8 encoded.  MUST NOT be null-terminated.
      Implementations SHOULD include a model name
      and a suffix derived from the last 2 octets
      of the MAC address (e.g., "Speaker-A3F2").

   Beacon packets are sent to the discovery
   multicast address 01:60:AB:FF:FF:00 (Ethernet)
   or embedded in DISCOVERY-type frames (LVDS).

8.2.  Beacon Timing

   All nodes MUST transmit beacons at a nominal
   interval of 1000 milliseconds.

   Beacon timing constraints:

   -  Nominal interval:  1000 ms.
   -  Jitter tolerance:  +/- 100 ms.
   -  Minimum interval:  900 ms.
   -  Maximum interval:  1100 ms.

   Initial transmission randomization:

      On startup, a node MUST delay its first beacon
      by a random interval uniformly distributed in
      the range [0, 1000) milliseconds.  This
      prevents beacon synchronization when multiple
      nodes boot simultaneously.

   The randomization seed SHOULD be derived from the
   low bits of the MAC address or a hardware random
   number generator.

   A node MUST transmit its first beacon within
   1000 milliseconds of completing initialization.

   LVDS-mode beacon timing:

      In LVDS mode, beacons are embedded in
      DISCOVERY-type TDM frames.  The master sends
      discovery frames at a nominal interval of
      1000 ms.  Slaves respond within the same frame
      when polled.  The timing constraints above
      apply to the master's discovery frame rate.

   IGMP snooping interaction:

      On Ethernet switches that implement IGMP
      snooping, AudioBus multicast traffic may be
      pruned from ports without listeners.  Because
      AudioBus uses Layer 2 multicast without IGMP,
      switches MUST be configured to flood the
      AudioBus multicast groups to all ports, or
      IGMP snooping MUST be disabled on the AudioBus
      VLAN.

      Alternatively, implementations MAY send IGMP
      Membership Report (IGMPv2 or IGMPv3) messages
      for the discovery multicast group
      (01:60:AB:FF:FF:00) mapped to the IPv4
      multicast address 239.96.171.0 to ensure
      switch forwarding.

8.3.  Node Identification

   The Node UID is a 32-bit unsigned integer that
   uniquely identifies a node within an AudioBus
   network.

   Derivation from Ethernet MAC address:

      UID = (MAC[2] << 24) | (MAC[3] << 16)
          | (MAC[4] << 8)  |  MAC[5]

   Where MAC[0] through MAC[5] are the 6 octets of
   the node's Ethernet MAC address, with MAC[0]
   being the first (most significant) octet.

   This uses the lower 4 octets of the MAC address,
   which are the unique portion assigned by the
   manufacturer.  The upper 2 octets (OUI prefix)
   are discarded.

   For LVDS-only nodes without an Ethernet MAC:

      The UID SHOULD be derived from a hardware
      unique identifier (e.g., the MCU's fused
      serial number).  If no hardware identifier
      is available, the UID MUST be assigned at
      manufacturing time and stored in non-volatile
      memory.

   UID 0x00000000 is reserved for broadcast.
   UID 0xFFFFFFFF is reserved for unassigned nodes.

   A node MUST NOT change its UID during operation.
   Two nodes MUST NOT have the same UID on the same
   network.  If a UID collision is detected (a
   beacon from a different node with the same UID),
   the node with the higher PTP priority value
   (lower priority) MUST generate a new UID by
   XOR-ing its current UID with 0x80000000 and
   re-announce.

   Node Name:

      The node name is a human-readable UTF-8 string
      of 0 to 31 octets.  Implementations SHOULD
      set a default name of the form:

         <model>-<hex suffix>

      Example: "AudioBridge-A3F2"

      The name is informational only and MUST NOT be
      used for addressing or identification.

8.4.  Hardware Types

   The HW Type field classifies the primary function
   of a node for user-interface presentation and
   automatic routing.

   +-------+------+-----------------------------+
   | Value | Name | Description                 |
   +-------+------+-----------------------------+
   |     0 | GEN  | Generic (unclassified)      |
   |     1 | SPK  | Speaker / output device     |
   |     2 | MIC  | Microphone / input device   |
   |     3 | IO   | Speaker + Microphone        |
   |     4 | ABR  | Analog bridge (ADC/DAC)     |
   |     5 | DBR  | Digital bridge (AES/SPDIF)  |
   |     6 | MIX  | Mixer / DSP processor       |
   |     7 | REC  | Recording device            |
   |     8 | AMP  | Power amplifier             |
   |     9 | EFX  | Effects processor           |
   |    10 | CTL  | Control surface             |
   | 11-63 |      | Unassigned (Expert Review)  |
   |64-254 |      | Unassigned (Spec Required)  |
   |   255 | VND  | Vendor-specific             |
   +-------+------+-----------------------------+

            Table 6: Hardware Type Values

   Implementations MUST set a meaningful hardware
   type.  A node whose function does not match any
   defined type MUST use 0 (Generic).

   Receivers MUST NOT reject nodes based on their
   hardware type.

8.5.  Node Expiry

   A node MUST be considered offline if no valid
   beacon has been received from that node within
   the expiry timeout.

   Expiry timeout:  3000 milliseconds (3 missed
   beacon intervals).

   Upon expiry, the implementation MUST perform
   the following actions in order:

   1.  Mark the node as offline in the local node
       table.
   2.  Notify the application via the event callback
       with event type NODE_LOST.
   3.  If the expired node was the grandmaster,
       immediately re-run the BMC algorithm
       (Section 7.2).
   4.  Release all subscriptions to streams
       originated by the expired node.
   5.  If audio from the expired node was being
       played, fade to silence over 10 milliseconds
       (480 samples at 48 kHz) to avoid clicks.

   A node that has expired MAY rejoin the network
   by transmitting beacons again.  Receivers MUST
   treat a beacon from a previously expired node
   as a new discovery and re-initialize all state
   for that node.

8.6.  LVDS Discovery Sequence

   In LVDS mode, discovery is master-initiated and
   sequential.  The master discovers nodes one at a
   time along the daisy chain.

   The discovery process uses DISCOVERY-type TDM
   frames (frame_type = 0x01) with sub-type codes
   in the payload:

   +------+--------------+---------------------------+
   | Code | Name         | Direction                 |
   +------+--------------+---------------------------+
   | 0x01 | BEACON       | Master to chain           |
   | 0x02 | RESPONSE     | Slave to master           |
   | 0x03 | ASSIGN       | Master to specific slave  |
   | 0x04 | CONFIG       | Master to all slaves      |
   | 0x05 | CONFIG_ACK   | Slave to master           |
   +------+--------------+---------------------------+

            Table 7: LVDS Discovery Sub-Types

   Sequence:

   1.  Master sends BEACON with target_node_id = 1.
   2.  The nearest undiscovered node (CDR locked
       on upstream port, no address assigned) receives
       the beacon and responds with its node
       descriptor (RESPONSE sub-type).
   3.  Master sends ASSIGN with the node's assigned
       address.
   4.  The newly addressed node enables its
       downstream port.
   5.  Master increments target_node_id and repeats
       from step 1.
   6.  If no RESPONSE is received within 50 ms,
       discovery is complete.
   7.  Master computes the slot map and distributes
       it via CONFIG sub-type.
   8.  Each slave acknowledges with CONFIG_ACK.
   9.  On all ACKs received, master transitions to
       RUNNING state.

   Hot-plug detection:

      Each node monitors the CDR LOCK signal on
      both ports.  Loss of lock on the downstream
      port indicates that the downstream node was
      disconnected.  The node reports this to the
      master via a STATUS frame.

      When a new node is connected, CDR lock is
      acquired on the upstream port of the existing
      end-node.  The master detects the topology
      change and re-runs discovery from the last
      known node.


9.  Stream Management

9.1.  Stream Announcement (Type 0x05)

   A talker advertises each published stream by
   periodically transmitting a stream announcement
   packet.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |   Channels    |  Bit Depth    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sample Rate                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    Packet Interval (us)       |   Encoding    |  Name Len     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Stream Flags                |         Reserved              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Stream Name (UTF-8)                     ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   Channel Labels (TLV)                     ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 9: Stream Announcement

   ABNF for stream announcement payload:

   announce-payload = stream-id channels bit-depth
                      sample-rate pkt-interval
                      encoding name-len
                      stream-flags reserved
                      stream-name channel-labels

   stream-id      = 2OCTET  ; big-endian, 1..65534
   channels       = OCTET   ; 1..64
   bit-depth      = OCTET   ; 16, 24, or 32
   sample-rate    = 4OCTET  ; big-endian Hz
   pkt-interval   = 2OCTET  ; big-endian, us
   encoding       = OCTET   ; 0 = Linear PCM
   name-len       = OCTET   ; 0..63
   stream-flags   = 2OCTET  ; big-endian
   reserved       = 2OCTET  ; MUST be zero
   stream-name    = *63OCTET
   channel-labels = *channel-label
   channel-label  = label-len label-text
   label-len      = OCTET   ; 0..31
   label-text     = *31OCTET ; UTF-8

   Field definitions:

   Stream ID:  16 bits.  Unique per talker.  Range
      1 to 65534.  Value 0x0000 is reserved (node
      metadata).  Value 0xFFFF is reserved.  The
      talker assigns stream IDs starting from 1.

   Channels:  8 bits.  Number of audio channels in
      this stream.  Range: 1 to 64.

   Bit Depth:  8 bits.  Sample bit depth.  Valid
      values: 16, 24, 32.  Other values are reserved.
      Receivers MUST reject announcements with
      unsupported bit depths.

   Sample Rate:  32 bits.  Big-endian.  Sample rate
      in hertz.  Valid values: 44100, 48000, 88200,
      96000.  Other values are reserved.

   Packet Interval:  16 bits.  Big-endian.  Time in
      microseconds between consecutive audio packets
      for this stream.  Valid values: 125, 250, 500,
      1000, 2000, 4000.

   Encoding:  8 bits.

      0:   Linear PCM (signed two's complement,
           big-endian).  This is the only encoding
           defined by this specification.
      1-255:  Reserved for future use.

   Name Len:  8 bits.  Length of the stream name in
      octets.  Range: 0 to 63.

   Stream Flags:  16 bits.  Big-endian.

      Bit 0:   Active.  1 = stream is currently
               transmitting audio data.  0 = stream
               is announced but not yet active.
      Bit 1:   Persistent.  1 = stream survives
               talker reboot (stored in NVM).
      Bits 2-15:  Reserved.  MUST be zero.

   A talker MUST transmit a stream announcement for
   each published stream at least once per beacon
   interval (1000 ms).  A talker SHOULD transmit
   the announcement immediately when the stream is
   first created.

   The announcement MUST be sent to the discovery
   multicast address (01:60:AB:FF:FF:00).

9.2.  Channel Labels

   Channel labels follow the stream name as a
   sequence of length-prefixed UTF-8 strings.
   There is exactly one label entry per channel,
   in channel order (channel 0 first).

   Format of each entry:

      [label-len: 1 octet]  Length of label text.
      [label-text: label-len octets]  UTF-8 text.

   A label-len of 0 indicates an unnamed channel.

   Standard label conventions:

   +----------+---------------------------------+
   | Label    | Meaning                         |
   +----------+---------------------------------+
   | "L"      | Left                            |
   | "R"      | Right                           |
   | "C"      | Center                          |
   | "LFE"    | Low Frequency Effects           |
   | "LS"     | Left Surround                   |
   | "RS"     | Right Surround                  |
   | "M"      | Mono                            |
   | "1".."64"| Numbered channel                |
   +----------+---------------------------------+

            Table 8: Standard Channel Labels

   Total label data MUST NOT exceed the remaining
   payload capacity after the stream name.

9.3.  Stream Subscription (Type 0x03)

   A listener requests audio from a talker by
   sending a subscription packet.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |        Subscription Seq       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Talker UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Ch Mask Len   |   Channel Bitmask (variable)               ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 10: Stream Subscription

   ABNF for subscription payload:

   subscribe-payload = stream-id sub-seq
                       talker-uid ch-mask-len
                       [channel-bitmask]

   stream-id       = 2OCTET  ; big-endian
   sub-seq         = 2OCTET  ; big-endian
   talker-uid      = 4OCTET  ; big-endian
   ch-mask-len     = OCTET   ; 0..8
   channel-bitmask = 1*8OCTET

   Field definitions:

   Stream ID:  16 bits.  The stream to subscribe to.

   Subscription Seq:  16 bits.  Monotonically
      increasing per listener.  Allows the talker
      to deduplicate retransmitted subscriptions.

   Talker UID:  32 bits.  UID of the node publishing
      the stream.

   Ch Mask Len:  8 bits.  Length of the channel
      bitmask in octets.  Range: 0 to 8.  A value
      of 0 means subscribe to all channels.

   Channel Bitmask:  Variable, 1 to 8 octets.  Bit
      N = 1 subscribes to channel N.  LSB of the
      first octet is channel 0.  Unused high bits
      MUST be zero.

   Talkers transmit audio to the stream multicast
   group regardless of subscription state.  This
   enables stateless multicast; the subscription
   serves only as a signal for the listener to join
   the multicast group and begin buffering.

   A listener MUST transmit a subscription to the
   discovery multicast address.  The talker does not
   acknowledge; the listener infers success by
   receiving audio packets.

   Unsubscription (Type 0x04) uses the same format.
   The bitmask is ignored for unsubscription.

   A listener MUST leave the stream multicast group
   after sending an unsubscription.

9.4.  Dynamic Channel Count

   A talker MAY change the channel count of an
   active stream.  The procedure is:

   1.  Talker sends a new stream announcement with
       the updated Channels field.

   2.  Talker adjusts audio packets to include the
       new channel count, starting with the next
       packet interval boundary.

   3.  On channel addition, listeners MUST initialize
       new channels to silence (zero samples).

   4.  On channel removal, listeners MUST cease
       reading removed channels immediately.

   The following fields MUST NOT change during the
   lifetime of a stream:

   -  Stream ID
   -  Sample Rate
   -  Bit Depth
   -  Encoding

   To change any of these, the talker MUST delete
   the stream (Section 9.5) and create a new one.

   The Packet Interval MAY be changed.  Listeners
   MUST adapt to the new interval without manual
   intervention.

9.5.  Stream Teardown (Type 0x06)

   A talker removes a stream by sending a
   STREAM_DELETE packet.

   Payload: 2 octets.

      stream-delete-payload = stream-id
      stream-id = 2OCTET  ; big-endian

   The talker MUST:

   1.  Stop transmitting audio packets for the
       stream.
   2.  Send a STREAM_DELETE packet.
   3.  Retransmit the STREAM_DELETE packet two
       additional times at 100 ms intervals to
       ensure delivery (total: 3 transmissions).
   4.  Leave the stream multicast group.
   5.  Release the Stream ID for reuse.

   Listeners MUST:

   1.  Stop playout within one beacon interval
       (1000 ms).
   2.  Fade audio to silence over 10 ms (480
       samples at 48 kHz).
   3.  Leave the stream multicast group.
   4.  Release all resources (buffers, state)
       associated with the stream.
   5.  Notify the application via event callback.

9.6.  Stream Lifecycle State Machine

   A stream transitions through the following
   states:

      +----------+    announce    +-----------+
      |          |  -----------> |           |
      |  IDLE    |               | ANNOUNCED |
      |          |  <----------- |           |
      +----------+    delete     +-----------+
                                    |     ^
                              first |     | stop
                             audio  |     | audio
                              pkt   |     |
                                    v     |
                                +-----------+
                                |           |
                                |  ACTIVE   |
                                |           |
                                +-----------+
                                    |
                              delete|
                                    v
                                +-----------+
                                |           |
                                | TEARDOWN  |
                                |           |
                                +-----------+
                                    |
                               3x   |
                              sent  |
                                    v
                                +-----------+
                                |           |
                                |  DELETED  |
                                |           |
                                +-----------+

         Figure 11: Stream Lifecycle State Machine

   IDLE:  Stream ID is not in use.

   ANNOUNCED:  Stream announcement has been sent
      but no audio data is flowing yet.

   ACTIVE:  Audio packets are being transmitted.

   TEARDOWN:  STREAM_DELETE has been sent.  The
      talker retransmits up to 2 more times.

   DELETED:  All resources released.  Stream ID
      may be reused after 5 seconds.


10.  Audio Data Transport

10.1.  Audio Packet Format (Type 0x01)

   Audio data is carried in AUDIO packets, each
   containing one or more sample frames for a
   single stream.

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

            Figure 12: Audio Packet

   ABNF for audio payload:

   audio-payload  = stream-id channels bit-depth
                    sample-rate samples-per-ch
                    reserved pts audio-data

   stream-id      = 2OCTET  ; big-endian
   channels       = OCTET   ; 1..64
   bit-depth      = OCTET   ; 16, 24, or 32
   sample-rate    = 4OCTET  ; big-endian, Hz
   samples-per-ch = 2OCTET  ; big-endian, 1..192
   reserved       = 2OCTET  ; MUST be zero
   pts            = 8OCTET  ; big-endian, signed ns
   audio-data     = *OCTET  ; see Section 10.3

   The audio header is 20 octets.  The total audio
   payload is:

      20 + (channels * samples-per-ch * (bit-depth/8))

   Field definitions:

   Stream ID:  16 bits.  Identifies the stream.
      MUST match a previously announced stream.

   Channels:  8 bits.  Number of interleaved
      channels.  MUST match the current announcement.

   Bit Depth:  8 bits.  16, 24, or 32.

   Sample Rate:  32 bits.  MUST match the stream
      announcement.  Included for self-describing
      packets to allow receivers to decode without
      prior announcement state.

   Samples per Channel:  16 bits.  Number of sample
      frames in this packet.  Determined by the
      packet interval and sample rate:

         N = sample_rate * packet_interval_us
             / 1,000,000

   Reserved:  16 bits.  MUST be zero.

   Presentation Timestamp:  64 bits.  Signed
      integer in nanoseconds.  See Section 10.4.

10.2.  Sample Encoding

   All audio samples are encoded as signed two's
   complement integers in big-endian (network) byte
   order, with the most significant octet first.

   +----------+--------+-----------+------------------+
   | Depth    | Octets | Min Value | Max Value        |
   +----------+--------+-----------+------------------+
   | 16-bit   |      2 |   -32768  |        32767     |
   | 24-bit   |      3 | -8388608  |      8388607     |
   | 32-bit   |      4 |-2^31      |      2^31 - 1    |
   +----------+--------+-----------+------------------+

            Table 9: Sample Encoding Ranges

   Encoding examples (big-endian byte order):

   16-bit samples:

      Silence:      0x00 0x00
      Full-scale +:  0x7F 0xFF  (= +32767)
      Full-scale -:  0x80 0x00  (= -32768)
      Half-scale +:  0x40 0x00  (= +16384)
      -1 (LSB):      0xFF 0xFF  (= -1)

   24-bit samples:

      Silence:      0x00 0x00 0x00
      Full-scale +:  0x7F 0xFF 0xFF  (= +8388607)
      Full-scale -:  0x80 0x00 0x00  (= -8388608)
      -6 dBFS:       0x5A 0x82 0x79  (= +5931641)

   32-bit samples:

      Silence:      0x00 0x00 0x00 0x00
      Full-scale +:  0x7F 0xFF 0xFF 0xFF
      Full-scale -:  0x80 0x00 0x00 0x00
      1 kHz sine peak at 48 kHz:
                     0x7F 0xFF 0xFF 0xFF

   Bit-depth conversion:

      When a receiver's internal processing depth
      differs from the wire format, the following
      conversions apply:

      16 to 32:  out = (int32_t)in16 << 16
      24 to 32:  out = (int32_t)in24 << 8
      32 to 24:  out = in32 >> 8 (truncation)
      32 to 16:  out = in32 >> 16 (truncation)

      Implementations SHOULD apply dithering when
      truncating from higher to lower bit depth.

10.3.  Sample Interleaving

   Samples are interleaved by channel within each
   frame, then frames are concatenated:

      S(0,0) S(0,1) ... S(0,C-1)
      S(1,0) S(1,1) ... S(1,C-1)
      ...
      S(N-1,0) S(N-1,1) ... S(N-1,C-1)

   Where S(f,c) is sample frame f, channel c.  C
   is the channel count.  N is samples per channel.

   Total audio data size in octets:

      audio_bytes = C * N * (bit_depth / 8)

   Example: 2 channels, 48 samples, 24-bit:

      audio_bytes = 2 * 48 * 3 = 288 octets

   The channel ordering is defined by the channel
   labels in the stream announcement.  Channel 0 is
   always the first channel in the interleave.

10.4.  Presentation Timestamps

   The Presentation Timestamp (PTS) is a signed
   64-bit integer in nanoseconds, referenced to the
   PTP time base established in Section 7.

   Computation at the talker:

      PTS = T_capture + presentation_latency

   Where:

      T_capture is the PTP time at which the first
      sample in the packet was captured (or would
      have been captured for synthesized audio).

      presentation_latency is a configurable offset
      that determines how far in the future playout
      occurs.  Default: 2,000,000 ns (2 ms).

   Presentation_latency MUST be at least:

      pres_latency >= max_network_delay
                    + max_jitter
                    + receiver_processing_time

   Where max_network_delay is the worst-case
   one-way delay, max_jitter is the observed
   peak-to-peak jitter, and receiver_processing_time
   is the time needed to decode and route audio
   to the output.

   Playout algorithm at the listener:

   1.  On receipt of an audio packet, extract PTS.
   2.  Compute delta = PTS - current_PTP_time.
   3.  If delta > 0, buffer the packet.
   4.  If delta <= 0, the packet is late.
       Increment late-packet counter.
       If delta > -1,000,000 ns (-1 ms), play
       immediately (soft late).  Otherwise, discard.
   5.  When the local PTP clock reaches PTS,
       release samples to the audio output.

   All listeners receiving the same stream MUST
   begin playout at the same PTP-referenced instant,
   ensuring sample-accurate synchronization.

   PTS wrap-around:

      The 64-bit nanosecond counter wraps after
      approximately 292 years.  Implementations
      need not handle wrap-around.

10.5.  Packet Interval

   The packet interval determines how many samples
   are grouped into each audio packet.

   +----------+-------+-------+-------+-------+-----+
   | Interval | 44.1k | 48k   | 88.2k | 96k   |Use  |
   | (us)     | (smp) | (smp) | (smp) | (smp) |     |
   +----------+-------+-------+-------+-------+-----+
   |      125 |     6 |     6 |    11 |    12 |ULL  |
   |      250 |    11 |    12 |    22 |    24 |LL   |
   |      500 |    22 |    24 |    44 |    48 |Bal  |
   |     1000 |    44 |    48 |    88 |    96 |Def  |
   |     2000 |    88 |    96 |   176 |   192 |HE   |
   |     4000 |   176 |   192 |   352 |   384 |ME   |
   +----------+-------+-------+-------+-------+-----+

   ULL=Ultra-Low Latency, LL=Low Latency,
   Bal=Balanced, Def=Default, HE=High Efficiency,
   ME=Maximum Efficiency.

            Table 10: Packet Intervals and Counts

   Implementations MUST support 1000 us.  Support
   for 125, 250, 500, 2000, and 4000 us is
   RECOMMENDED.

   The samples-per-channel value for non-integer
   divisions is computed as:

      N = floor(sample_rate * interval_us
                / 1,000,000)

   For 44100 Hz at 125 us:

      N = floor(44100 * 125 / 1000000)
        = floor(5.5125) = 5

   However, to maintain long-term sample count
   accuracy, the talker MUST alternate between
   N and N+1 samples per packet such that the
   average sample rate is exact.

10.6.  Sequence Numbering and Gap Detection

   The common header Sequence Number (Section 4.1)
   is incremented by one for each audio packet
   transmitted on a given stream.  The counter is
   per-stream, per-source.  It wraps from 65535
   to 0.

   Expected next sequence number:

      expected = (last_received + 1) mod 65536

   Gap detection:

      If the received sequence number does not
      equal expected, a gap has occurred.

      gap_size = (received - expected) mod 65536

      If gap_size > 32768, interpret as a reorder
      (the packet is old) and discard.

      If gap_size <= 32768, interpret as loss.

   On loss:

   1.  Increment the lost-packet counter by
       gap_size.
   2.  Apply packet loss concealment
       (Section 10.8).
   3.  Update expected to received + 1.

   Loss ratio:

      loss_ratio = lost / (received + lost)

   Implementations MUST report loss_ratio to the
   application at least once per second.

10.7.  Playout Buffer Sizing

   The playout buffer absorbs network jitter and
   ensures audio continuity.  The minimum buffer
   depth in sample frames is:

      B_min = ceil(Fs * (pres_latency
              - D_min) / 1,000,000,000)

   Where:

      Fs = sample rate in Hz.
      pres_latency = presentation latency in ns.
      D_min = minimum observed one-way network
              delay in ns.

   The recommended buffer depth is:

      B_rec = ceil(Fs * (pres_latency
              - D_min + 3 * J_rms)
              / 1,000,000,000)

   Where J_rms is the RMS jitter in ns computed
   per Section 13.4.

   The buffer MUST be implemented as a circular
   (ring) buffer.  The buffer depth MUST be a
   power of two for efficient modular arithmetic.

   Example:

      Fs = 48000 Hz
      pres_latency = 2,000,000 ns
      D_min = 100,000 ns
      J_rms = 50,000 ns

      B_min = ceil(48000 * (2000000 - 100000)
              / 1000000000)
            = ceil(48000 * 0.0019)
            = ceil(91.2) = 92 samples

      B_rec = ceil(48000 * (2000000 - 100000
              + 150000) / 1000000000)
            = ceil(48000 * 0.00205)
            = ceil(98.4) = 99 samples

      Round up to next power of two: 128 samples.

   Implementations SHOULD dynamically adjust buffer
   depth based on observed jitter, with a minimum
   floor of 2 * packet_interval worth of samples.

10.8.  Packet Loss Concealment

   When one or more audio packets are lost (detected
   via sequence gap), the listener MUST conceal the
   gap using one of the following strategies:

   Level 0 - Silence insertion (REQUIRED):

      Insert zero-valued samples for each lost
      frame.  This is the simplest and always-
      available strategy.

   Level 1 - Repetition (RECOMMENDED):

      Repeat the last valid sample frame for each
      lost frame.  Apply a linear fade toward zero
      over the duration of the gap, with a maximum
      fade time of 10 ms (480 samples at 48 kHz).

   Level 2 - Interpolation (OPTIONAL):

      If the packet following the gap arrives within
      the playout deadline, linearly interpolate
      between the last valid sample and the first
      sample of the next packet.

   Level 3 - Waveform substitution (OPTIONAL):

      Use pitch-synchronous waveform repetition
      (PSOLA or similar) to maintain tonal quality
      during gaps of up to 40 ms.

   The concealment strategy is implementation-
   defined.  All implementations MUST support at
   least Level 0.

   After 100 ms of consecutive loss (approximately
   100 packets at 1000 us interval), the
   implementation MUST fade to silence and notify
   the application.

10.9.  MTU Considerations

   The maximum audio packet size on Ethernet is
   constrained by the path MTU.  The default
   Ethernet MTU is 1500 octets.

   Maximum audio payload per packet:

      max_audio = MTU - eth_hdr - abus_hdr
                  - audio_hdr
                = 1500 - 14 - 12 - 20
                = 1454 octets

   Maximum samples per channel for a given
   configuration:

      max_N = floor(max_audio
              / (channels * (bit_depth / 8)))

   Example: 64 channels, 32-bit, MTU 1500:

      max_N = floor(1454 / (64 * 4))
            = floor(1454 / 256) = 5

   This constrains the minimum packet interval
   for high channel counts:

   +------+-------+-------+-----------+-----------+
   | Ch   | Depth | N/pkt | Audio(B)  | Min Int   |
   +------+-------+-------+-----------+-----------+
   |    2 |    32 |    48 |       384 | 1000 us   |
   |    2 |    24 |    48 |       288 | 1000 us   |
   |    8 |    32 |    48 |      1536 | 1000 us*  |
   |    8 |    24 |    48 |      1152 | 1000 us   |
   |   32 |    32 |    12 |      1536 | 250 us*   |
   |   64 |    32 |     5 |      1280 | 125 us*   |
   |   64 |    24 |     7 |      1344 | 125 us    |
   +------+-------+-------+-----------+-----------+

   * May require jumbo frames for higher N values.

            Table 11: Audio Packet Sizes

   Implementations SHOULD support jumbo frames
   (9000 octets) when available.

10.10. Bandwidth Calculation

   The aggregate bandwidth for a single stream:

      BW_bps = (C * D * Fs) + (Fs / N) * OH * 8

   Where:

      C  = channels
      D  = bit depth (bits)
      Fs = sample rate (Hz)
      N  = samples per channel per packet
      OH = per-packet overhead in octets
         = ETH_HDR(14) + ABUS_HDR(12)
           + AUDIO_HDR(20) = 46 octets

   Worked example 1: 2ch, 24-bit, 48kHz, 1000us

      BW = (2 * 24 * 48000)
         + (48000 / 48) * 46 * 8
         = 2,304,000 + 1000 * 368
         = 2,304,000 + 368,000
         = 2,672,000 bps = 2.672 Mbps

   Worked example 2: 64ch, 32-bit, 48kHz, 1000us

      BW = (64 * 32 * 48000)
         + (48000 / 48) * 46 * 8
         = 98,304,000 + 368,000
         = 98,672,000 bps = 98.672 Mbps

   Worked example 3: 64ch, 24-bit, 96kHz, 1000us

      BW = (64 * 24 * 96000)
         + (96000 / 96) * 46 * 8
         = 147,456,000 + 368,000
         = 147,824,000 bps = 147.824 Mbps

      This exceeds 100 Mbps Ethernet.  Requires
      Gigabit Ethernet or LVDS transport.

   The LVDS transport bandwidth per direction:

      BW_lvds = frame_bytes * Fs * 8
              = 1024 * 48000 * 8
              = 393,216,000 bps = 393.2 Mbps

   This is the raw payload rate before 8b10b
   encoding.  The line rate with 8b10b overhead
   (25%) is:

      line_rate = BW_lvds * 10 / 8
               = 491,520,000 bps = 491.5 Mbps


11.  Control Tunneling

11.1.  Tunnel Packet Format (Type 0x20)

   Control data for GPIO, MIDI, SPI, and I2C is
   carried in TUNNEL packets.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Target UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Tunnel Type  |  Tunnel Seq   |      Tunnel Len               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                      Tunnel Payload                        ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 13: Tunnel Packet

   ABNF for tunnel payload:

   tunnel-payload = target-uid tunnel-type
                    tunnel-seq tunnel-len
                    tunnel-data

   target-uid   = 4OCTET  ; 0x00000000 = broadcast
   tunnel-type  = OCTET   ; Table 12
   tunnel-seq   = OCTET   ; per-type sequence
   tunnel-len   = 2OCTET  ; big-endian, octets
   tunnel-data  = *OCTET  ; type-specific

   Field definitions:

   Target UID:  32 bits.  Destination node.
      0x00000000 = broadcast to all nodes.
      A node MUST discard tunnel packets not
      addressed to it or to broadcast.

   Tunnel Type:  8 bits.

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

            Table 12: Tunnel Type Values

   Tunnel Seq:  8 bits.  Per-type sequence counter.
      Increments per packet per tunnel type per
      target.  Wraps from 255 to 0.  Receivers use
      this to detect loss and reordering.

   Tunnel Len:  16 bits.  Big-endian.  Length of the
      tunnel-data field in octets.

11.2.  GPIO Tunnel (Type 2)

   The GPIO tunnel carries idempotent pin state.

   Payload format (2 octets):

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        Pin States (16 bits)   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         Figure 14: GPIO Tunnel Payload

   Bit N corresponds to GPIO pin N on the target
   node.  1 = HIGH, 0 = LOW.  Bit 0 is the LSB.

   Timing requirements:

      The sender SHOULD transmit GPIO state at a
      minimum rate of 100 Hz (every 10 ms) when
      the state is changing.  During steady state,
      the sender MUST retransmit at least once per
      second to ensure convergence after packet
      loss.

   Idempotency:

      GPIO packets are idempotent.  The receiver
      MUST apply the complete 16-bit state on each
      reception, overwriting any previous state.
      There is no delta encoding.

   Error behavior:

      On tunnel packet loss, the receiver retains
      the last known GPIO state.  The sender's
      periodic retransmission ensures eventual
      consistency.

   GPIO transition latency:

      Worst case: 1 packet interval + network delay.
      At 1000 us interval on Ethernet with 100 us
      one-way delay: ~1100 us (1.1 ms).
      At 48 kHz LVDS (frame period ~21 us):
      ~42 us (one round-trip frame).

11.3.  MIDI Tunnel (Type 3)

   The MIDI tunnel carries raw MIDI byte streams.

   Payload format (1 to 253 octets):

    0
    0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+
   | MIDI Status   |
   +-+-+-+-+-+-+-+-+
   | Data 1        |  (OPTIONAL)
   +-+-+-+-+-+-+-+-+
   | Data 2        |  (OPTIONAL)
   +-+-+-+-+-+-+-+-+
   | ...           |  (additional bytes)
   +-+-+-+-+-+-+-+-+

         Figure 15: MIDI Tunnel Payload

   The payload contains raw MIDI bytes exactly
   as they would appear on a MIDI 1.0 serial link.
   Running status is permitted.

   Message framing:

      Each tunnel packet SHOULD contain exactly one
      complete MIDI message (1 to 3 octets for
      channel messages).

      Multiple short messages MAY be concatenated
      in a single tunnel packet if they fit within
      the tunnel bandwidth allocation.

   System Exclusive (SysEx) fragmentation:

      SysEx messages (0xF0 ... 0xF7) may exceed the
      tunnel packet capacity.  They MUST be
      fragmented as follows:

      -  First fragment: begins with 0xF0, contains
         as many data bytes as fit.
      -  Middle fragments: contain continuation data
         bytes (0x00-0x7F only, no status bytes).
      -  Last fragment: ends with 0xF7.

      The tunnel sequence number ensures correct
      reassembly.  If a fragment is lost, the
      receiver MUST discard the entire SysEx and
      wait for the next 0xF0.

      Maximum SysEx size: 65535 octets (limited by
      Tunnel Len field).

   Throughput calculation:

      At 1000 us packet interval (Ethernet):
         Max 3 octets per packet at minimum.
         1000 packets/sec * 3 bytes = 3000 bytes/sec.
         Standard MIDI 1.0 = 31250 baud / 10 bits
         = 3125 bytes/sec.
         AudioBus MIDI throughput is comparable to
         standard MIDI at 1 ms interval.

      At higher bandwidth allocations:
         With 253 octets per packet at 1 ms:
         253,000 bytes/sec = ~80x standard MIDI.

      LVDS mode (3 bytes per frame at 48 kHz):
         3 * 48000 = 144,000 bytes/sec
         = ~46x standard MIDI.

   Timing accuracy:

      MIDI events are timestamped implicitly by
      their transmission time.  For sample-accurate
      MIDI timing, implementations SHOULD use the
      sideband channel (Section 11.6) for MIDI
      clock ticks and embed MIDI events in the
      nearest audio packet boundary.

11.4.  SPI Tunnel (Type 0)

   The SPI tunnel enables remote SPI transactions
   on a target node's SPI bus.

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   CS Pin      |   SPI Mode    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Flags       |   TX Len      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           TX Data          ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         Figure 16: SPI Tunnel Payload

   CS Pin:  8 bits.  Chip-select index on the target
      node.  Range: 0 to 15.  Values 16-255 are
      reserved.

   SPI Mode:  8 bits.  SPI clock polarity and phase.

      0: CPOL=0, CPHA=0 (Mode 0).
      1: CPOL=0, CPHA=1 (Mode 1).
      2: CPOL=1, CPHA=0 (Mode 2).
      3: CPOL=1, CPHA=1 (Mode 3).
      4-255: Reserved.

   Flags:  8 bits.

      Bit 0:  Response requested.  1 = the target
              MUST send a reverse SPI tunnel packet
              containing the MISO data.
      Bit 1:  CS hold.  1 = do not deassert CS
              after this transaction (for multi-
              packet transfers).
      Bits 2-7:  Reserved.  MUST be zero.

   TX Len:  8 bits.  Length of TX Data in octets.
      Range: 1 to 249.

   TX Data:  Variable.  MOSI data to transmit.

   Response mechanism:

      When bit 0 of Flags is set, the target node
      executes the SPI transaction and returns the
      MISO data in a reverse TUNNEL packet addressed
      to the requester.  The reverse packet has the
      same format with the TX Data replaced by RX
      Data and the Response flag cleared.

      The tunnel sequence number in the response
      MUST match the request.

      Timeout: the requester MUST wait up to 100 ms
      for a response.  If no response arrives, the
      transaction is considered failed.

11.5.  I2C Tunnel (Type 1)

   The I2C tunnel enables remote I2C transactions.

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  I2C Address  |    Flags      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  TX Len       |   RX Len      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           I2C TX Data      ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

         Figure 17: I2C Tunnel Payload

   I2C Address:  8 bits.  7-bit I2C address in
      bits 6:0.  Bit 7 MUST be zero.  Range of
      address: 0x08 to 0x77 (valid 7-bit range,
      excluding reserved addresses).

   Flags:  8 bits.

      Bit 0:  Direction.  1 = Read, 0 = Write.
      Bit 1:  10-bit address.  1 = the address
              field is the low 7 bits; the high
              3 bits follow in the first TX byte.
      Bit 2:  Repeated start.  1 = use repeated
              start between write and read phases.
      Bits 3-7:  Reserved.  MUST be zero.

   TX Len:  8 bits.  Length of TX data.  For a
      write, this is the data to write.  For a
      read, this is the register address to send
      before reading.  Range: 0 to 248.

   RX Len:  8 bits.  Number of bytes to read.
      Range: 0 to 248.  For writes, MUST be zero.

   I2C TX Data:  Variable.  Data to transmit on
      the I2C bus.

   Read response:

      For read transactions (Flags bit 0 = 1),
      the target node performs the I2C read and
      returns the data in a reverse TUNNEL packet.
      The response contains:

      -  I2C Address:  echoed from request.
      -  Flags:  bit 0 = 0 (write, carrying data).
      -  TX Len:  length of read data.
      -  RX Len:  0.
      -  TX Data:  the bytes read from the I2C bus.

      The tunnel sequence number MUST match.
      Timeout: 100 ms.

   Error reporting:

      If the I2C transaction fails (NACK, timeout,
      bus error), the target sends a response with
      TX Len = 1 and TX Data = error code:

      0x01:  NACK on address.
      0x02:  NACK on data.
      0x03:  Bus error.
      0x04:  Timeout.

11.6.  Sideband Channel (Type 4)

   The sideband provides a single-octet,
   per-direction, per-frame data path with
   absolute minimum latency.

   Payload: 1 octet.

    0
    0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+
   |  Sideband Data|
   +-+-+-+-+-+-+-+-+

         Figure 18: Sideband Payload

   The sideband byte is embedded in every audio
   packet (Ethernet mode) or every TDM frame
   (LVDS mode).  It has zero additional latency
   beyond the audio transport itself.

   Typical uses:

   -  Bit 0:  MIDI clock tick (24 ppqn).
   -  Bit 1:  Transport start/stop.
   -  Bit 2:  Trigger/gate signal.
   -  Bits 3-7:  Application-defined.

   In LVDS mode, the sideband byte occupies a
   dedicated 1-byte slot in both the downstream
   and upstream regions of the TDM frame
   (Section 6.3).

   In Ethernet mode, the sideband byte is carried
   in TUNNEL packets with type SIDEBAND.


12.  Metadata

12.1.  Metadata Packet Format (Type 0x30)

   Metadata carries descriptive key-value pairs
   about nodes and streams.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Stream ID            |  Num Entries  | Meta Flags    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        Meta Sequence          |        Total Entries          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   Metadata Entries                          ...|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 19: Metadata Packet

   ABNF for metadata payload:

   metadata-payload = stream-id num-entries
                      meta-flags meta-seq
                      total-entries
                      *metadata-entry

   stream-id      = 2OCTET  ; 0x0000 = node-level
   num-entries    = OCTET   ; entries in this packet
   meta-flags     = OCTET
   meta-seq       = 2OCTET  ; big-endian
   total-entries  = 2OCTET  ; big-endian
   metadata-entry = key-length value-length
                    key-data value-data
   key-length     = OCTET   ; 1..255
   value-length   = 2OCTET  ; big-endian, 0..65535
   key-data       = 1*255OCTET ; UTF-8
   value-data     = *65535OCTET ; UTF-8 or binary

   Field definitions:

   Stream ID:  16 bits.  0x0000 = node-level
      metadata.  Non-zero = stream-level metadata
      for the specified stream.

   Num Entries:  8 bits.  Number of metadata entries
      in this packet.

   Meta Flags:  8 bits.

      Bit 0:  Complete.  1 = this packet contains
              all metadata for the stream/node.
              0 = metadata is fragmented across
              multiple packets.
      Bit 1:  Refresh.  1 = this metadata supersedes
              all previously received metadata for
              this stream/node.
      Bits 2-7:  Reserved.  MUST be zero.

   Meta Sequence:  16 bits.  Sequence number for
      ordering fragmented metadata.

   Total Entries:  16 bits.  Total number of entries
      across all fragments.

12.2.  Metadata Entry Encoding

   Each entry is encoded as:

      [key_length:   1 octet]
      [value_length: 2 octets, big-endian]
      [key:          key_length octets, UTF-8]
      [value:        value_length octets]

   Maximum key length: 255 octets.
   Maximum value length: 65535 octets.

   Keys MUST be valid UTF-8.  Keys are
   case-sensitive.

   Values are UTF-8 by default.  Binary values
   are permitted when the key definition specifies
   binary encoding.

   Duplicate keys within the same stream/node
   context are not permitted.  If a receiver
   encounters a duplicate key, the later value
   MUST replace the earlier one.

12.3.  Standard Metadata Keys

   +------------------+------+--------------------------+
   | Key              | Type | Description              |
   +------------------+------+--------------------------+
   | "vendor"         | UTF8 | Manufacturer name        |
   | "model"          | UTF8 | Device model identifier  |
   | "firmware"       | UTF8 | Firmware version string  |
   | "serial"         | UTF8 | Serial number            |
   | "location"       | UTF8 | Physical location        |
   | "purpose"        | UTF8 | Intended use             |
   | "icon"           | UTF8 | URL or data: URI         |
   | "channels.in"    | UTF8 | Input channel count      |
   | "channels.out"   | UTF8 | Output channel count     |
   | "latency.min"    | UTF8 | Minimum latency (us)     |
   | "latency.max"    | UTF8 | Maximum latency (us)     |
   | "sample.rates"   | UTF8 | Supported rates (CSV)    |
   | "bit.depths"     | UTF8 | Supported depths (CSV)   |
   +------------------+------+--------------------------+

            Table 13: Standard Metadata Keys

12.4.  Vendor Key Naming Rules

   Vendor-specific metadata keys MUST use reverse
   domain notation:

      <reversed-domain>.<key-name>

   Examples:

      "tv.datanoise.firmware.build"
      "tv.datanoise.hw.revision"
      "com.example.custom-feature"

   Vendor keys MUST NOT collide with standard keys.
   Standard keys are always unprefixed single words
   or dotted lowercase identifiers without a domain
   component.

12.5.  Metadata Caching and Refresh

   Receivers SHOULD cache metadata indexed by
   (Source UID, Stream ID, Key).

   Cache invalidation:

      When a METADATA packet with the Refresh flag
      (bit 1) is received, the receiver MUST discard
      all previously cached metadata for that
      (Source UID, Stream ID) pair and replace it
      with the new entries.

   Refresh timing:

      Nodes SHOULD retransmit complete metadata
      at least once every 10 seconds.

      On any metadata change, the node MUST
      immediately transmit an updated METADATA
      packet with the Refresh flag set.


13.  Latency Measurement

13.1.  Ping Message (Type 0x40)

   A node measures round-trip time to another node
   by sending a Ping and waiting for a Pong.

   Payload: 16 octets.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Target UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |           Sender Timestamp (64 bits, ns)                      |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Ping Sequence                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 20: Ping Message

   Target UID:  32 bits.  The node to ping.
      0x00000000 is not permitted (cannot broadcast
      ping).

   Sender Timestamp:  64 bits.  The sender's PTP
      time at transmission, in nanoseconds.

   Ping Sequence:  32 bits.  Monotonically
      increasing per sender.  Used to match Pong
      responses.

13.2.  Pong Message (Type 0x41)

   Payload: 24 octets.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Pinger UID                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |         Echoed Sender Timestamp (64 bits, ns)                 |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Echoed Ping Sequence                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |         Responder Timestamp (64 bits, ns)                     |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            Figure 21: Pong Message

   Pinger UID:  32 bits.  Copied from the Ping's
      Source UID (common header).

   Echoed Sender Timestamp:  64 bits.  Copied from
      the Ping.

   Echoed Ping Sequence:  32 bits.  Copied from
      the Ping.

   Responder Timestamp:  64 bits.  The responder's
      PTP time at Pong transmission.

   The receiver of a Ping MUST transmit the Pong
   within 1 millisecond.  If the node cannot meet
   this deadline, it MUST discard the Ping.

13.3.  Round-Trip Computation

   The ping sender computes RTT as:

      T1 = Sender Timestamp (from Ping)
      T2 = Responder Timestamp (from Pong)
      T3 = PTP time when Pong is received

      RTT = T3 - T1

   Estimated one-way delay (symmetric assumption):

      D_oneway = RTT / 2

   Processing delay at responder:

      D_proc = T2 - T1 - D_oneway  (approximate)

   Smoothed RTT (exponential moving average):

      SRTT = SRTT + (RTT - SRTT) / 8

   Implementations SHOULD send Pings at 1 Hz
   (one per second) per target node.

13.4.  Jitter Estimation

   Interarrival jitter is computed per the
   algorithm in [RFC3550] Section 6.4.1.

   For consecutive audio packets i and j:

      D(i,j) = (R_j - R_i) - (S_j - S_i)

   Where:

      R_j = PTP time of arrival of packet j
      R_i = PTP time of arrival of packet i
      S_j = Presentation timestamp of packet j
      S_i = Presentation timestamp of packet i

   The jitter estimator is updated as:

      J(i) = J(i-1) + (|D(i,j)| - J(i-1)) / 16

   This is equivalent to:

      J(i) = (15/16) * J(i-1) + (1/16) * |D(i,j)|

   The factor 1/16 provides a time constant of
   approximately 16 packet intervals.

   Additional statistics that implementations
   SHOULD track:

   -  J_peak: maximum |D(i,j)| over the last
      256 measurements.
   -  J_min: minimum |D(i,j)| over the last
      256 measurements.
   -  J_rms: root-mean-square jitter over the
      last 256 measurements:

         J_rms = sqrt(sum(D(i,j)^2) / 256)

   -  Loss count: total packets lost.
   -  Loss ratio: lost / (received + lost).
   -  Late count: packets arriving after PTS.

13.5.  Measurement Filtering

   Implementations SHOULD discard outlier RTT
   measurements that exceed 3 * SRTT.  These
   may be caused by transient network congestion
   and would corrupt the jitter estimate.

   After filtering, the SRTT SHOULD be computed
   using a minimum of 8 samples.

13.6.  Reporting Requirements

   Implementations MUST maintain per-stream
   statistics and make them available to the
   application:

   -  Current RTT (last measurement).
   -  Smoothed RTT.
   -  Interarrival jitter (J).
   -  Packet loss count and ratio.
   -  Late packet count.
   -  Buffer fill level (samples).

   These statistics MUST be updated at least once
   per second.


14.  Error Handling

   This section specifies the REQUIRED behavior
   for all error conditions.

14.1.  Unknown Packet Types

   A receiver MUST silently discard packets with
   unrecognized Pkt Type values.  The receiver
   MUST NOT close connections, reset state, send
   error responses, or log at a rate exceeding
   1 message per second.

   A counter of discarded packets MUST be
   maintained.

14.2.  Version Mismatch

   A receiver MUST silently discard packets with
   a Version field that does not match any version
   it supports.

   If more than 50% of received packets in any
   10-second window have an unsupported version,
   the implementation SHOULD notify the application
   of a possible version mismatch on the network.

14.3.  CRC Failure

   LVDS transport:

      Frames with CRC-32 mismatch MUST be discarded
      in their entirety.  No partial data from a
      corrupted frame may be used.

      Implementations MUST maintain a CRC error
      counter.

      If the CRC error rate exceeds 1 per 1000
      frames (0.1%) sustained over 10 seconds, the
      implementation MUST report a link quality
      warning to the application.

      If the CRC error rate exceeds 1 per 100
      frames (1%) sustained over 5 seconds, the
      implementation SHOULD attempt link
      reinitialization (increase guard time, retry
      CDR synchronization).

   Ethernet transport:

      Ethernet FCS checking is performed by hardware.
      Corrupted frames are discarded by the MAC
      before reaching the AudioBus stack.

14.4.  Sequence Gaps

   Listeners MUST track the expected sequence
   number for each stream.  On detection of a gap:

   1.  Compute gap size:
       gap = (received_seq - expected_seq) mod 65536
       If gap > 32768, the packet is stale; discard.

   2.  Increment lost-packet counter by gap.

   3.  For each missing packet, generate concealment
       audio per Section 10.8.

   4.  Update expected to (received_seq + 1)
       mod 65536.

   5.  Continue normal processing of the received
       packet.

   If consecutive gaps accumulate to more than
   100 ms of audio (e.g., 100 packets at 1 ms
   interval), the implementation MUST:

   -  Fade audio to silence.
   -  Notify the application.
   -  Reset the sequence tracker.
   -  Wait for the next packet before resuming.

14.5.  Late Packets

   Audio packets arriving after their presentation
   timestamp MUST be counted in the late-packet
   counter.

   Soft late (within 1 ms after PTS):

      The implementation MAY play the audio
      immediately if the playout buffer can absorb
      the timing error.

   Hard late (more than 1 ms after PTS):

      The audio data MUST be discarded.

   If more than 10% of packets in any 1-second
   window are late:

   1.  Report a latency warning to the application.

   2.  Implementations SHOULD increase the
       presentation latency by 500 us and notify
       the application.

   3.  If the late ratio exceeds 50%, the
       implementation MUST increase presentation
       latency to the maximum configured value or
       notify the application to take corrective
       action.

14.6.  Grandmaster Loss

   If no PTP Sync is received for 3 consecutive
   Sync intervals (3 * 125 ms = 375 ms):

   1.  Declare the grandmaster unreachable.

   2.  Enter clock holdover mode: continue using
       the last computed clock offset and drift
       rate.

   3.  Re-run the BMC algorithm (Section 7.2)
       using the beacons from remaining nodes.

   4.  If this node wins BMC, begin transmitting
       PTP Sync within 125 ms.

   5.  Continue audio playout without interruption.

   Holdover accuracy:

      The holdover drift rate depends on the local
      oscillator frequency stability:

      +-------------+------------------+
      | Oscillator  | Drift Rate       |
      +-------------+------------------+
      | 50 ppm      | 50 us/s          |
      | 25 ppm      | 25 us/s          |
      | 1 ppm       | 1 us/s           |
      | TCXO (0.5)  | 0.5 us/s         |
      +-------------+------------------+

            Table 14: Holdover Drift Rates

      At 50 ppm, sample clocks will drift by 1
      sample (20.83 us at 48 kHz) in approximately
      417 ms.

      Implementations SHOULD support holdover for
      at least 2 seconds without audible artifacts.

14.7.  Malformed Packets

   A packet is malformed if:

   -  The Payload Length field exceeds the actual
      remaining data in the packet.
   -  A required field contains a value outside
      its defined range.
   -  The packet is shorter than the minimum
      length for its type.

   Malformed packets MUST be discarded silently.
   A counter MUST be maintained.

14.8.  Oversized Packets

   A packet exceeding the transport MTU is invalid.

   Ethernet:  Packets exceeding the negotiated MTU
      (typically 1500 or 9000 octets) are dropped
      by the network.

   LVDS:  Frames exceeding the TDM frame size
      (1024 or 512 octets) MUST be discarded by
      the receiver.

   Implementations MUST NOT generate oversized
   packets.

14.9.  Resource Exhaustion

   If a node cannot allocate resources (memory,
   buffers, processing time) for a new stream or
   tunnel:

   1.  The node MUST NOT subscribe to additional
       streams.
   2.  The node SHOULD set Listener Slots to 0 in
       its beacon.
   3.  Existing streams MUST NOT be affected.

   If existing streams cannot be serviced due to
   resource pressure:

   1.  Drop the most recently subscribed stream
       first (LIFO).
   2.  Notify the application.
   3.  Update the beacon to reflect reduced
       capacity.

14.10. Multicast Storm Protection

   Implementations MUST implement the following
   self-policing mechanisms:

   1.  Rate limiting:  A node MUST NOT transmit
       more than 1000 non-audio packets per second.

   2.  Audio rate limiting:  A stream's audio
       packet rate MUST NOT exceed:

          max_pps = 1,000,000 / packet_interval_us

       with a tolerance of +5%.

   3.  Beacon suppression:  If a node receives
       beacons from more than 64 unique UIDs, it
       MUST stop processing beacons from new nodes
       and log a warning.

   4.  Multicast group limiting:  A node MUST NOT
       join more than 64 stream multicast groups
       simultaneously.

14.11. Clock Holdover Behavior

   During grandmaster loss (Section 14.6), nodes
   operate in holdover mode.

   The holdover algorithm:

   1.  Maintain the last computed frequency offset
       (in ppb) from the PTP servo loop.

   2.  Continue incrementing the local PTP estimate
       using the free-running local oscillator
       adjusted by the stored frequency offset.

   3.  If a new grandmaster is elected within the
       holdover period, smoothly transition to the
       new time base using a linear slew of no more
       than 1 ppm (1 us/s) to avoid audible glitches.

   4.  If holdover exceeds 10 seconds, declare clock
       free-running and notify the application.

14.12. Link Failure Recovery (LVDS)

   On detection of CDR lock loss on an LVDS port:

   1.  The node MUST cease transmission on the
       affected port within 1 frame period.

   2.  The node MUST notify the master via the
       remaining active port (upstream).

   3.  The master MUST re-run discovery for the
       affected chain segment.

   4.  If the master's only link fails, it enters
       standalone mode and continues local audio
       processing.

   5.  On CDR re-lock, the node MUST wait for 10
       consecutive valid frames before resuming
       normal operation.


15.  Extensibility and Versioning

15.1.  Version Negotiation

   The Version field in the common header enables
   future protocol revisions.

   Version 1:  This specification.

   Version 0:  Reserved (MUST NOT be used).

   A node supporting version N MAY also support
   version N-1 for backwards compatibility, but
   this is OPTIONAL.

   When a node receives a packet with a version
   higher than it supports, it MUST silently
   discard the packet.  The node MUST NOT attempt
   to interpret the payload.

   Version negotiation procedure:

   1.  On receiving a beacon with a different
       version, the node records the version.

   2.  If all nodes on the network support
       version N+1, a node MAY begin transmitting
       version N+1 packets.

   3.  A node MUST NOT transmit version N+1 packets
       unless it has received at least one beacon
       from every known node indicating support for
       version N+1.

15.2.  Reserved Fields

   All fields marked "Reserved" in this document
   MUST be set to zero on transmission and MUST
   be ignored on reception.

   This ensures forward compatibility.  Future
   versions may assign meaning to reserved fields.
   Implementations MUST NOT reject packets that
   have non-zero reserved fields.

15.3.  Private-Use Ranges

   Packet types 0x50 through 0x7F are designated
   for private use.  Implementations MAY use these
   for vendor-specific extensions.

   Private-use packets MUST carry a valid common
   header (Section 4.1).  The Source UID and
   Sequence Number fields MUST be set correctly.

   Tunnel types 5 through 15 are available for
   future standardization or private use.

   Private-use tunnel types (8-15) MAY be used
   without registration.  Tunnel types 5-7 are
   reserved for future Standards Action.

15.4.  Feature Negotiation

   Nodes advertise capabilities via the beacon
   Flags and Tunnel Caps fields (Section 8.1).

   A node MUST NOT send tunnel packets of a type
   not supported by the target (as indicated in
   the target's beacon Tunnel Caps field).

   A node MUST NOT subscribe to a stream with
   parameters it cannot support (e.g., unsupported
   sample rate or bit depth).


16.  Conformance

16.1.  Minimum Conformance Requirements

   A conformant AudioBus implementation MUST:

   1.  Implement the common header format
       (Section 4.1).

   2.  Implement at least one transport
       (Ethernet or LVDS).

   3.  Implement beacon transmission and reception
       (Section 8).

   4.  Implement node expiry (Section 8.5).

   5.  Implement stream announcement reception
       (Section 9.1).

   6.  Implement audio packet transmission or
       reception (Section 10.1).

   7.  Implement PTP clock synchronization as a
       slave (Section 7).

   8.  Support linear PCM encoding at 48 kHz,
       24-bit, with 1000 us packet interval.

   9.  Implement the error handling behaviors
       in Section 14.

   10. Implement the security considerations
       relevant to the transport in Section 17.

16.2.  Conformance Levels

   Level 1 - Listener (minimum):

      MUST receive and decode audio streams.
      MUST implement PTP slave.
      MUST process beacons.
      MUST send subscriptions.

   Level 2 - Talker:

      All of Level 1, plus:
      MUST publish stream announcements.
      MUST transmit audio packets.
      MUST support PTP grandmaster candidacy.

   Level 3 - Full:

      All of Level 2, plus:
      MUST support at least one tunnel type.
      MUST support metadata.
      MUST support latency measurement (Ping/Pong).
      MUST support dynamic channel count changes.

   Level 4 - Bridge:

      All of Level 3, plus:
      MUST support both Ethernet and LVDS.
      MUST forward streams between transports.
      MUST relay PTP across transport boundaries.

   OPTIONAL features (any level):

   -  GPIO tunnel
   -  MIDI tunnel
   -  SPI tunnel
   -  I2C tunnel
   -  Sideband channel
   -  Multiple simultaneous streams
   -  Jumbo frame support
   -  44.1 kHz sample rate family
   -  88.2/96 kHz sample rates
   -  16-bit and 32-bit sample depths

16.3.  Interoperability Requirements

   All conformant implementations MUST
   interoperate at the following baseline
   configuration:

   -  48 kHz sample rate
   -  24-bit sample depth
   -  1000 us packet interval
   -  Linear PCM encoding
   -  2 ms presentation latency

   Implementations MUST gracefully handle
   announcements for configurations they do not
   support by ignoring unsupported streams
   (not by crashing or entering an error state).


17.  Security Considerations

   This section analyzes security threats and
   provides mitigations in accordance with
   [RFC3552].

17.1.  Threat Model

   AudioBus is designed for trusted local-area
   networks.  The security boundary is the Layer 2
   broadcast domain.

   Specific attacks:

   T1. Rogue talker:  An unauthorized device
       publishes audio streams containing malicious
       content (noise, offensive material) or
       high-volume data intended to overload
       listeners.

   T2. Stream hijacking:  An attacker sends
       STREAM_ANNOUNCE for an existing Stream ID
       with a different Source UID, redirecting
       listeners to malicious audio.

   T3. Clock attack:  An attacker injects PTP Sync
       messages with a lower Clock Class and Priority,
       forcing itself to become grandmaster.  It then
       manipulates time to cause playout glitches,
       buffer overflows, or denial of service.

   T4. Tunnel injection:  An attacker sends TUNNEL
       packets to perform unauthorized I2C/SPI
       transactions on remote nodes, potentially
       reprogramming DACs, erasing firmware, or
       controlling GPIO pins connected to physical
       actuators.

   T5. Eavesdropping:  An attacker on the same
       Layer 2 segment passively captures audio
       content.

   T6. Denial of service:  An attacker floods the
       network with AudioBus packets, exhausting
       switch bandwidth and node processing capacity.

   T7. Replay attack:  An attacker captures and
       retransmits valid packets at a later time.

17.2.  Confidentiality

   AudioBus transmits all data in cleartext.  Audio
   content, control commands, and metadata are
   visible to any device on the same Layer 2
   broadcast domain.

   Deployments requiring confidentiality SHOULD:

   -  Use IEEE 802.1AE MACsec [IEEE802.1AE] for
      link-layer encryption.
   -  Use physically isolated networks.
   -  Use dedicated VLANs with access control.

   LVDS transport provides inherent physical-layer
   confidentiality: only devices physically
   connected to the twisted-pair chain can observe
   traffic.

17.3.  Integrity

   The LVDS transport includes CRC-32 for error
   detection.  The Ethernet transport relies on
   the Ethernet FCS.

   Neither mechanism provides cryptographic
   integrity.  An active attacker on the same
   network segment can forge or modify packets
   without detection.

   Mitigation:

   -  MACsec provides per-frame integrity using
      AES-GCM.
   -  Implementations SHOULD validate that Source
      UIDs in received packets match known node
      UIDs from the beacon table.
   -  Future AudioBus versions MAY define HMAC-based
      packet authentication.

17.4.  Availability

   Denial of service vectors and mitigations:

   Vector: Multicast flooding.
   Mitigation: Rate-limit AudioBus multicast groups
      at the switch.  Implementations MUST implement
      self-policing (Section 14.10).

   Vector: PTP Sync injection.
   Mitigation: Implementations SHOULD validate that
      PTP Sync Source UID matches the expected
      grandmaster.  Implementations SHOULD reject
      Sync from unknown UIDs.

   Vector: STREAM_DELETE injection.
   Mitigation: Implementations SHOULD verify that
      STREAM_DELETE Source UID matches the stream's
      talker UID.

   Vector: Beacon flooding (>64 nodes).
   Mitigation: Node table limiting
      (Section 14.10 item 3).

17.5.  Authentication

   AudioBus version 1 does not define an
   authentication mechanism.  All nodes on the
   network are implicitly trusted.

   Future versions MAY define:

   -  HMAC-SHA256 authentication appended to the
      common header (8 additional octets).
   -  Node certificate exchange during discovery.
   -  Authenticated PTP per IEEE 1588-2019 Annex P.
   -  Pre-shared key distribution via out-of-band
      mechanism.

17.6.  Privacy

   Node names, vendor information, and metadata
   are broadcast in cleartext.  This may reveal
   information about the audio installation.

   Implementations SHOULD allow administrators to
   disable metadata transmission.

   LVDS topologies have limited privacy exposure
   since traffic is confined to the physical chain.

17.7.  Specific Mitigations

   Deployments in environments with untrusted
   devices SHOULD implement the following measures,
   in order of effectiveness:

   M1. Physical isolation:

      Deploy AudioBus on a dedicated physical
      network or LVDS chain with no untrusted
      access points.

   M2. VLAN separation:

      Place all AudioBus nodes on a dedicated
      VLAN (Section 5.4).  Configure switch ports
      for the AudioBus VLAN only.

   M3. Port-based access control:

      Use IEEE 802.1X to authenticate devices
      before granting network access.

   M4. Link-layer encryption:

      Deploy MACsec [IEEE802.1AE] on all links
      carrying AudioBus traffic.

   M5. Grandmaster protection:

      Configure a UID allowlist of permitted
      grandmaster nodes.  Reject PTP Sync from
      UIDs not on the list.

   M6. Tunnel access control:

      Process TUNNEL packets only when the Target
      UID matches the local UID or is broadcast
      (0x00000000).  Optionally maintain a list of
      UIDs permitted to send tunnel packets to
      this node.

17.8.  Residual Risks

   Even with all mitigations applied:

   -  A compromised node on the same MACsec domain
      can still inject or modify AudioBus traffic.

   -  Physical access to an LVDS chain enables
      traffic interception and injection.

   -  Replay attacks are possible within the MACsec
      replay window.

   -  No mechanism prevents a legitimate node from
      malfunctioning and flooding the network.

   Implementations MUST NOT be used in safety-
   critical systems without additional integrity
   and redundancy measures.


18.  IANA Considerations

   This section requests the creation of six IANA
   registries and two IEEE Registration Authority
   assignments.

18.1.  EtherType Assignment

   This document requests assignment of an EtherType
   from the IEEE Registration Authority.

   Name:           AudioBus Protocol
   Reference:      Section 5.2 of this document
   Interim value:  0x88B6 (IEEE 802 Local
                   Experimental EtherType 1)
   Contact:        Authors (Section Authors'
                   Addresses)

   Until a dedicated EtherType is assigned,
   implementations MUST use 0x88B6.

18.2.  Multicast OUI Assignment

   This document requests assignment of an Ethernet
   multicast OUI from the IEEE Registration
   Authority.

   Name:           AudioBus Multicast
   Reference:      Section 5.3 of this document
   Interim prefix: 01:60:AB
   Contact:        Authors

18.3.  AudioBus Packet Type Registry

   IANA is requested to create the "AudioBus Packet
   Types" registry.

   Registration template:

      Value:        Single octet (0x00-0xFF)
      Name:         Short name (max 20 characters)
      Description:  Brief description
      Reference:    Specification reference

   Registration policies per [RFC8126]:

   +-------------+-----------------------+
   | Range       | Policy                |
   +-------------+-----------------------+
   | 0x00        | Reserved              |
   | 0x01-0x4F   | Standards Action      |
   | 0x50-0x7F   | Private Use           |
   | 0x80-0xEF   | Specification Req.    |
   | 0xF0-0xFF   | Reserved              |
   +-------------+-----------------------+

   Initial values:  Table 1 of this document.

   Designated expert instructions:

      The expert SHOULD verify that the proposed
      packet type does not duplicate functionality
      of an existing type, that the specification
      is sufficiently detailed for interoperable
      implementation, and that the type has a
      clear use case for audio networking.

18.4.  AudioBus Tunnel Type Registry

   IANA is requested to create the "AudioBus
   Tunnel Types" registry.

   Registration template:

      Value:        Single octet (0-15)
      Name:         Short name
      Description:  Brief description
      Reference:    Specification reference

   Registration policies:

   +--------+----------------------------+
   | Range  | Policy                     |
   +--------+----------------------------+
   | 0-4    | Standards Action           |
   | 5-7    | Standards Action           |
   | 8-15   | Expert Review              |
   +--------+----------------------------+

   Initial values:  Table 12 of this document.

   Designated expert instructions:

      The expert SHOULD verify that the tunnel type
      represents a distinct control protocol not
      already covered by existing types.

18.5.  AudioBus Hardware Type Registry

   IANA is requested to create the "AudioBus
   Hardware Types" registry.

   Registration template:

      Value:        Single octet (0-255)
      Abbreviation: 3-letter code
      Name:         Full name
      Description:  Brief description
      Reference:    Specification reference

   Registration policies:

   +--------+----------------------------+
   | Range  | Policy                     |
   +--------+----------------------------+
   | 0-10   | Standards Action           |
   | 11-63  | Expert Review              |
   | 64-254 | Specification Required     |
   | 255    | Private Use                |
   +--------+----------------------------+

   Initial values:  Table 6 of this document.

   Designated expert instructions:

      The expert SHOULD verify that the hardware
      type represents a distinct device category
      that aids user-interface presentation or
      automatic routing decisions.

18.6.  AudioBus Metadata Key Registry

   IANA is requested to create the "AudioBus
   Metadata Keys" registry.

   Registration template:

      Key:          UTF-8 string (max 64 chars)
      Type:         "UTF8" or "binary"
      Description:  Brief description
      Reference:    Specification reference

   Registration policy: First Come First Served
   per [RFC8126].

   Initial values:  Table 13 of this document.

   Keys using reverse-domain notation (vendor
   keys) do not require registration.

18.7.  AudioBus Sample Rate Registry

   IANA is requested to create the "AudioBus
   Sample Rates" registry.

   Registration template:

      Value:        32-bit unsigned integer (Hz)
      Description:  Brief description
      Reference:    Specification reference

   Registration policy: Standards Action.

   Initial values:

   +---------+-----------------------------+
   | Value   | Description                 |
   +---------+-----------------------------+
   |   44100 | 44.1 kHz family base rate   |
   |   48000 | 48 kHz family base rate     |
   |   88200 | 44.1 kHz family double rate |
   |   96000 | 48 kHz family double rate   |
   +---------+-----------------------------+

18.8.  AudioBus Encoding Registry

   IANA is requested to create the "AudioBus
   Audio Encodings" registry.

   Registration template:

      Value:        Single octet (0-255)
      Name:         Short name
      Description:  Encoding specification
      Reference:    Specification reference

   Registration policy: Standards Action.

   Initial values:

   +-------+-----------+----------------------+
   | Value | Name      | Description          |
   +-------+-----------+----------------------+
   |     0 | PCM       | Linear PCM, signed   |
   |       |           | two's complement,    |
   |       |           | big-endian           |
   | 1-255 | Reserved  |                      |
   +-------+-----------+----------------------+


Appendix A.  Channel Capacity Tables

A.1.  Ethernet Transport (100 Mbps)

   Maximum channels per stream, limited by MTU
   and bandwidth.  Assumes default MTU of 1500
   octets and the overhead structure:
   Ethernet header (14) + AudioBus header (12) +
   Audio header (20) = 46 octets overhead.

   +------+-------+-------+-------+---------+------+
   | Rate | Depth | Int.  | N/pkt | Audio B | MaxC |
   | (Hz) | (bit) | (us)  |       | /pkt    |      |
   +------+-------+-------+-------+---------+------+
   |48000 |    16 |  1000 |    48 |   C*96  |   15 |
   |48000 |    24 |  1000 |    48 |   C*144 |   10 |
   |48000 |    32 |  1000 |    48 |   C*192 |    7 |
   |48000 |    16 |   125 |     6 |   C*12  |  121 |
   |48000 |    24 |   125 |     6 |   C*18  |   80 |
   |48000 |    32 |   125 |     6 |   C*24  |   60 |
   |96000 |    16 |  1000 |    96 |   C*192 |    7 |
   |96000 |    24 |  1000 |    96 |   C*288 |    5 |
   |96000 |    32 |  1000 |    96 |   C*384 |    3 |
   |44100 |    24 |  1000 |    44 |   C*132 |   11 |
   +------+-------+-------+-------+---------+------+

   MaxC = floor(1454 / (N * (Depth/8)))
   Capped at 64 (protocol maximum).

   Note: The bandwidth limit may constrain MaxC
   below the MTU limit.  At 100 Mbps, the total
   aggregate of all streams must not exceed
   approximately 90 Mbps (allowing 10% for
   control traffic).

            Table A-1: Ethernet Capacity (MTU 1500)

A.2.  Ethernet Transport (1 Gbps, Jumbo Frames)

   With jumbo frames (MTU 9000) and Gigabit
   Ethernet:

   +------+-------+-------+-------+---------+------+
   | Rate | Depth | Int.  | N/pkt | Audio B | MaxC |
   | (Hz) | (bit) | (us)  |       | /pkt    |      |
   +------+-------+-------+-------+---------+------+
   |48000 |    32 |  1000 |    48 |   C*192 |   46 |
   |48000 |    24 |  1000 |    48 |   C*144 |   62 |
   |96000 |    32 |  1000 |    96 |   C*384 |   23 |
   |96000 |    24 |  1000 |    96 |   C*288 |   31 |
   +------+-------+-------+-------+---------+------+

   MaxC = floor(8954 / (N * (Depth/8)))
   Capped at 64.

            Table A-2: Ethernet Capacity (Jumbo)

A.3.  LVDS Transport (per direction)

   Available payload per direction after overhead:

      payload = (frame_bytes - OVERHEAD) / 2
                - GUARD/2 - SIDEBAND

   Where OVERHEAD = 16 bytes (sync + header +
   guard + CRC), GUARD = 2 bytes, SIDEBAND = 1.

   At 48/44.1 kHz (1024-byte frame):

      per_dir = (1024 - 16) / 2 - 1 - 1 = 502 B

   At 96/88.2 kHz (512-byte frame):

      per_dir = (512 - 16) / 2 - 1 - 1 = 246 B

   +------+-------+--------+---------+------+------+
   | Rate | Depth | Frame  | Per Dir | MaxC | Aux  |
   | (Hz) | (bit) | (B)    | (B)     |      | (B)  |
   +------+-------+--------+---------+------+------+
   |48000 |    32 |   1024 |     502 |   64 |  246 |
   |48000 |    24 |   1024 |     502 |   64 |  310 |
   |48000 |    16 |   1024 |     502 |   64 |  374 |
   |96000 |    32 |    512 |     246 |   61 |    2 |
   |96000 |    24 |    512 |     246 |   64 |   54 |
   |96000 |    16 |    512 |     246 |   64 |  118 |
   |44100 |    32 |   1024 |     502 |   64 |  246 |
   |44100 |    24 |   1024 |     502 |   64 |  310 |
   |88200 |    32 |    512 |     246 |   61 |    2 |
   |88200 |    24 |    512 |     246 |   64 |   54 |
   +------+-------+--------+---------+------+------+

   MaxC = min(64, floor(Per_Dir / (Depth/8)))
   Aux = Per_Dir - MaxC * (Depth/8)

            Table A-3: LVDS Capacity


Appendix B.  Recommended Hardware

B.1.  Ethernet Transport

   Microcontroller:

      Espressif ESP32-P4 (dual RISC-V 400 MHz,
      EMAC with IEEE 1588 hardware timestamping,
      512 KB SRAM, PSRAM support).

   Ethernet PHY:

      IP101GRI, RTL8201, or LAN8720 (100BASE-TX
      RMII).  Any MII/RMII PHY compatible with the
      ESP32-P4 EMAC is suitable.

   Switch:

      Any commodity 100 Mbps or Gigabit Ethernet
      switch.  Managed switches with IGMP snooping
      control and VLAN support are RECOMMENDED for
      installations with more than 8 nodes.

B.2.  LVDS Transport (Single-Chip, Recommended)

   Transceiver:

      TI SN65LVDT41 (single LVDS transceiver with
      integrated driver and receiver, driver enable
      control).  Approximate cost: $2 per port.

   Controller:

      ESP32-P4 with PARLIO peripheral in 1-bit mode
      at 98.304 MHz.  Uses 5 GPIO pins per port:
      data, clock, driver enable, receiver data,
      receiver clock.

   Clock:

      Si5351A programmable clock generator (master
      and slave).  Master: generates 49.152 MHz or
      45.1584 MHz from 25 MHz reference.  Slave:
      software PLL locked to master frame timing.

   Cable:  Cat5e or better, single pair.
   Termination:  100 ohm differential.

B.3.  LVDS Transport (Maximum Performance)

   Serializer:

      TI DS92LV1021A (10:1 LVDS serializer).
      10-bit parallel input at 49.152 MHz.

   Deserializer:

      TI DS92LV1212A (1:10 LVDS deserializer with
      CDR).  10-bit parallel output.  Hardware clock
      recovery provides self-clocking operation.

   Controller:

      ESP32-P4 with PARLIO peripheral in 10-bit or
      16-bit mode at 49.152 MHz.  Uses 24 GPIO pins
      per port.

   Clock:

      49.152 MHz crystal oscillator (master node).
      Slave nodes use the recovered clock from the
      DS92LV1212A CDR.

   Cable:  Cat5e or better, single pair.
   Termination:  100 ohm differential at each end.

   Approximate cost per port: $7 (serializer +
   deserializer pair).

B.4.  Audio Codec (Optional)

   DAC:  PCM5102A (I2S stereo, 32-bit, 384 kHz,
      ~$2.50).
   ADC:  PCM1808 (I2S stereo, 24-bit, 96 kHz,
      ~$3.00).
   Codec:  WM8960 (I2S stereo DAC+ADC, ~$2.00).

B.5.  Connectors

   +------------------+-----+----------------------------+
   | Type             |Cost | Notes                      |
   +------------------+-----+----------------------------+
   | RJ45             |$0.30| Use pins 1,2 (pair)        |
   | 3.5mm TRS        |$0.20| Tip=+, Ring=-, Sleeve=GND  |
   | JST-PH 3-pin     |$0.15| Compact, board-to-board    |
   | Ethercon (NE8MC)  |$3.00| Ruggedized RJ45, pro audio |
   | XLR-5 (Neutrik)  |$2.50| 5-pin, 2 for data, 1 GND  |
   +------------------+-----+----------------------------+

            Table B-1: Connector Options


Appendix C.  Example Message Exchanges

C.1.  Ethernet Startup Sequence

   Two nodes A (UID=0x00001234) and B
   (UID=0x00005678) on the same switch.

   T=0.000s  A boots.
             A joins 01:60:AB:FF:FF:00 (disc).
             A joins 01:60:AB:FF:FF:01 (PTP).
             A randomizes beacon delay: 347 ms.

   T=0.200s  B boots.
             B joins multicast groups.
             B randomizes beacon delay: 812 ms.

   T=0.347s  A sends BEACON:
               Version=1, PktType=0x02,
               Seq=0, SrcUID=0x00001234,
               PayloadLen=27,
               NodeUID=0x00001234,
               NameLen=6, HWType=4 (ABR),
               TalkerStreams=0,ListenerSlots=4,
               PTPPri=128, PTPClass=248,
               Flags=0x0004 (Ethernet capable),
               MaxCh=2, TunnelCaps=0x0C,
               Uptime=0, Name="Node-A".
             A assumes grandmaster (only node).
             A begins PTP Sync at 125 ms.

   T=0.347s  A sends PTP_SYNC:
               Seq=0, PTPSeq=0,
               OriginTS=347000000.

   T=0.350s  A sends PTP_FOLLOW_UP:
               PTPSeq=0,
               PreciseTS=347128456.

   T=1.012s  B sends BEACON:
               SrcUID=0x00005678,
               NameLen=6, HWType=1 (SPK),
               TalkerStreams=0,ListenerSlots=2,
               PTPPri=128, PTPClass=248,
               Flags=0x0004,
               MaxCh=2, TunnelCaps=0x04,
               Uptime=0, Name="Node-B".

   T=1.012s  A receives B's beacon.
             BMC: Both class=248, pri=128.
             Tiebreak: 0x1234 < 0x5678.
             A remains grandmaster.

   T=1.012s  B receives A's beacon (from T=0.347).
             BMC: 0x1234 < 0x5678.
             B accepts A as grandmaster.

   T=1.100s  B sends PTP_DELAY_REQ:
               PTPSeq=0.
             B records t3=1100000000 ns.

   T=1.100s  A receives Delay_Req.
             A records t4=1100080000 ns.
             A sends PTP_DELAY_RESP:
               PTPSeq=0, RxTS=1100080000,
               RequesterUID=0x00005678.

   T=1.100s  B receives Delay_Resp.
             B computes:
               t1 = 347128456 (Follow_Up)
               t2 = 347208000 (local HW rx)
               t3 = 1100000000
               t4 = 1100080000
               offset = ((t2-t1)-(t4-t3))/2
                      = (79544 - 80000) / 2
                      = -228 ns
               delay  = (79544 + 80000) / 2
                      = 79772 ns (~80 us)

   T=1.500s  A creates stream:
             id=0x0001, 2ch, 48kHz, 24-bit,
             1000us interval, name="Main Out".
             A sends STREAM_ANNOUNCE.
             A joins 01:60:AB:00:01:00.
             A begins sending AUDIO packets.

   T=1.500s  B receives STREAM_ANNOUNCE.
             B sends SUBSCRIBE:
               StreamID=0x0001,
               SubSeq=0,
               TalkerUID=0x00001234,
               ChMaskLen=0 (all channels).
             B joins 01:60:AB:00:01:00.

   T=1.501s  B receives first AUDIO packet:
               PTS = 1503000000 ns
               (2 ms presentation latency).
             B buffers audio.

   T=1.503s  PTS reached.  B begins playout.

            Figure C-1: Ethernet Startup Sequence

C.2.  LVDS Stream Creation

   Master M, slave S1 (node_id=1), slave S2
   (node_id=2).  Configuration: 48 kHz, 24-bit.

   T=0.000s  M boots, enters DISCOVERY state.
             M sends DISCOVERY frame:
               frame_type=0x01,
               sub_type=0x01 (BEACON),
               target_node_id=1.

   T=0.001s  S1 receives beacon on upstream port.
             S1 CDR locked. S1 responds:
               sub_type=0x02 (RESPONSE),
               node_descriptor: uid=0xA001,
               hw_type=3, max_dn=2, max_up=2,
               depth=24, tunnel_req=0x08 (MIDI),
               tunnel_bw=3.

   T=0.002s  M receives response.
             M sends ASSIGN: node_id=1.
             S1 stores node_id=1.
             S1 enables downstream port.

   T=0.003s  M sends BEACON: target_node_id=2.

   T=0.004s  S2 receives beacon via S1.
             S2 responds: uid=0xA002,
               hw_type=1, max_dn=2, max_up=0,
               depth=24, tunnel_req=0x00.

   T=0.005s  M assigns node_id=2.

   T=0.006s  M sends BEACON: target_node_id=3.
             No response within 50 ms.

   T=0.056s  Discovery complete. 2 slaves found.
             M computes slot map:
               DN: sideband(1) + S1_dn(6) +
                   S2_dn(6) + S1_midi(3) = 16 B
               Guard: 2 B
               UP: sideband(1) + S1_up(6) +
                   S1_midi(3) = 10 B
             M sends CONFIG frame with slot map.

   T=0.057s  S1 receives CONFIG, ACKs.
   T=0.058s  S2 receives CONFIG, ACKs.

   T=0.060s  M enters RUNNING state.
             First audio frame transmitted.

            Figure C-2: LVDS Discovery Sequence

C.3.  Subscription to Subset of Channels

   Node B subscribes to channels 0 and 1 (left
   and right) of a 16-channel stream from node A.

   B sends SUBSCRIBE:
     StreamID    = 0x0042
     SubSeq      = 5
     TalkerUID   = 0x00001234
     ChMaskLen   = 2  (2 octets = 16 bits)
     ChBitmask   = 0x03 0x00
                   (bits 0 and 1 set = ch 0, ch 1)

   B joins multicast 01:60:AB:00:42:00.
   B receives AUDIO packets containing all 16
   channels but only decodes channels 0 and 1.

C.4.  Grandmaster Failover

   Three nodes: A (UID=0x1000, GM), B (0x2000),
   C (0x3000).  A fails at T=5.0s.

   T=5.000s  A crashes.  No more PTP Sync or
             beacons from A.

   T=5.375s  B and C detect: no Sync for 375 ms.
             Both enter holdover mode.
             Both re-run BMC.
             B: UID=0x2000, C: UID=0x3000.
             B wins (lower UID).

   T=5.500s  B begins transmitting PTP Sync as
             new grandmaster.
             B sets grandmaster flag in beacon.

   T=5.500s  C receives B's Sync.
             C exits holdover.
             C performs PTP offset computation
             against B.

   T=8.000s  No beacon from A for 3000 ms.
             B and C remove A from node table.
             B and C release subscriptions to
             A's streams.

   Audio playout on B and C is uninterrupted
   throughout the failover.

            Figure C-3: Grandmaster Failover

C.5.  Node Departure (Graceful)

   Node B (listener) is powered down gracefully.

   T=10.0s  B sends UNSUBSCRIBE for all streams.
            B leaves all stream multicast groups.
            B stops transmitting beacons.

   T=13.0s  Other nodes detect: no beacon from B
            for 3000 ms.
            B is removed from all node tables.
            If B was grandmaster, BMC is re-run.

            Figure C-4: Graceful Node Departure


Appendix D.  Test Vectors

   This appendix provides hex dumps of example
   packets for each packet type.  All values are
   shown in hexadecimal.  Multi-octet fields are
   big-endian.

D.1.  Common Header

   A common header with Version=1, PktType=0x01
   (AUDIO), Flags=0x0000, SourceUID=0x00001234,
   Seq=0x0001, PayloadLen=0x0114 (276):

   01 01 00 00 00 00 12 34 00 01 01 14

D.2.  Beacon Packet

   Complete beacon from node UID=0x00001234,
   Name="Node-A", HWType=4, PTPPri=128,
   Class=248:

   Common header:
   01 02 00 00 00 00 12 34 00 00 00 1E

   Beacon payload:
   00 00 12 34    ; Node UID
   06             ; Name Len = 6
   04             ; HW Type = ABR
   01             ; Talker Streams = 1
   04             ; Listener Slots = 4
   80             ; PTP Priority = 128
   F8             ; PTP Clock Class = 248
   00 04          ; Flags (Ethernet capable)
   02             ; Max Channels = 2
   0C             ; Tunnel Caps (GPIO+MIDI)
   00 05          ; Uptime = 5 seconds
   4E 6F 64 65 2D 41  ; "Node-A" (UTF-8)

   Full packet (30 octets payload):
   01 02 00 00 00 00 12 34 00 00 00 1E
   00 00 12 34 06 04 01 04 80 F8 00 04
   02 0C 00 05 4E 6F 64 65 2D 41

D.3.  Stream Announcement

   Stream ID=0x0001, 2ch, 24-bit, 48kHz,
   1000us, PCM, name="Stereo":

   Common header:
   01 05 00 00 00 00 12 34 00 01 00 1C

   Announcement payload:
   00 01          ; Stream ID
   02             ; Channels = 2
   18             ; Bit Depth = 24
   00 00 BB 80    ; Sample Rate = 48000
   03 E8          ; Packet Interval = 1000 us
   00             ; Encoding = PCM
   06             ; Name Len = 6
   00 01          ; Stream Flags (Active)
   00 00          ; Reserved
   53 74 65 72 65 6F  ; "Stereo"
   01 4C          ; Ch 0 label: len=1, "L"
   01 52          ; Ch 1 label: len=1, "R"

D.4.  Audio Packet

   Stream 0x0001, 2ch, 24-bit, 48kHz, 6 samples
   (125 us interval), PTS = 1,500,000,000 ns.
   All samples = silence (0x000000):

   Common header:
   01 01 00 00 00 00 12 34 00 05 00 38

   Audio header:
   00 01          ; Stream ID
   02             ; Channels = 2
   18             ; Bit Depth = 24
   00 00 BB 80    ; Sample Rate = 48000
   00 06          ; Samples per Channel = 6
   00 00          ; Reserved
   00 00 00 00 59 68 2F 00  ; PTS (1500000000)

   Audio data (2 * 6 * 3 = 36 octets, all zero):
   00 00 00 00 00 00 00 00 00 00 00 00
   00 00 00 00 00 00 00 00 00 00 00 00
   00 00 00 00 00 00 00 00 00 00 00 00

D.5.  Tunnel Packet (GPIO)

   GPIO state 0xC003 to node UID=0x00005678:

   Common header:
   01 20 00 00 00 00 12 34 00 0A 00 08

   Tunnel payload:
   00 00 56 78    ; Target UID
   02             ; Tunnel Type = GPIO
   0A             ; Tunnel Seq = 10
   00 02          ; Tunnel Len = 2
   C0 03          ; Pin States

D.6.  Tunnel Packet (MIDI - Note On)

   MIDI Note On, channel 0, note 60 (C4),
   velocity 100, to node UID=0x00005678:

   Common header:
   01 20 00 00 00 00 12 34 00 0B 00 09

   Tunnel payload:
   00 00 56 78    ; Target UID
   03             ; Tunnel Type = MIDI
   05             ; Tunnel Seq = 5
   00 03          ; Tunnel Len = 3
   90             ; Note On, channel 0
   3C             ; Note 60 (C4)
   64             ; Velocity 100

D.7.  Ping Packet

   Ping from UID=0x1234 to UID=0x5678,
   timestamp=2,000,000,000 ns, seq=42:

   Common header:
   01 40 00 00 00 00 12 34 00 00 00 10

   Ping payload:
   00 00 56 78                ; Target UID
   00 00 00 00 77 35 94 00   ; Sender TS (2e9)
   00 00 00 2A                ; Ping Seq = 42

D.8.  Pong Packet

   Pong responding to the above Ping:

   Common header:
   01 41 00 00 00 00 56 78 00 00 00 18

   Pong payload:
   00 00 12 34                ; Pinger UID
   00 00 00 00 77 35 94 00   ; Echoed TS
   00 00 00 2A                ; Echoed Seq
   00 00 00 00 77 35 95 00   ; Responder TS
                               ; (2000000256 ns)

D.9.  LVDS TDM Frame (48 kHz, 2ch down, 2ch up)

   Frame excerpt (first 32 bytes of 1024):

   Byte  Hex   Description
   ----  ----  -----------
   0000  BC    K28.5 (Comma)
   0001  3C    K28.1 (SOF)
   0002  00 01 Frame Counter = 1
   0004  00    Frame Type = Normal
   0005  04    Flags: 48k, 24-bit, sideband
   0006  03    Node Count = 3
   0007  04    DN Audio Slots = 4
   0008  02    UP Audio Slots = 2
   0009  A7    SlotMap CRC
   000A  00    DN Sideband = 0x00
   000B  00 00 00  DN Ch0 sample (silence, 24b)
   000E  00 00 00  DN Ch1 sample
   0011  00 00 00  DN Ch2 sample
   0014  00 00 00  DN Ch3 sample
   0017  00 00 00  DN MIDI tunnel (3 bytes)
   001A  BC    K28.5 (Guard)
   001B  FB    K27.7 (Turnaround)
   001C  00    UP Sideband = 0x00
   001D  00 00 00  UP Ch0 sample
   0020  00 00 00  UP Ch1 sample
   ...
   03FC  xx xx xx xx  CRC-32


Appendix E.  Implementation Notes

E.1.  Buffer Sizing Guidance

   The following buffer sizes are recommended for
   an ESP32-P4 implementation:

   Audio ring buffer depth:

      Minimum: 4 sample frames.
      Recommended: 8 sample frames (default).
      Maximum: 64 sample frames.

      Memory per ring buffer:
        mem = depth * channels * 4 bytes
        Example: 8 frames * 64 ch * 4 B = 2048 B

   DMA frame buffer:

      Two frame buffers (double-buffering):
        mem = 2 * frame_bytes
        Example: 2 * 1024 = 2048 B

   Tunnel queue:

      8 entries * 256 bytes = 2048 B

   Total SRAM budget for AudioBus core:

      Audio ring (TX + RX):  4096 B
      DMA frame buffers:     2048 B
      Scratch buffers:       4096 B
      Tunnel queues:         2048 B
      Handle + state:         512 B
      ----------------------------
      Total:               ~12800 B (~12.5 KB)

   This fits comfortably in the ESP32-P4's 512 KB
   internal SRAM.

E.2.  CPU Budget

   At 48 kHz, the frame processing ISR and task
   execute once per sample period (20.83 us).

   Estimated CPU cycles per frame on ESP32-P4
   at 400 MHz:

   +--------------------------+--------+---------+
   | Operation                | Cycles | Time    |
   +--------------------------+--------+---------+
   | 8b10b decode (1024 sym)  |  ~3000 |  7.5 us |
   | CRC-32 verify (1024 B)   |  ~2000 |  5.0 us |
   | Frame unpack (64 ch)     |  ~1500 |  3.8 us |
   | Tunnel unpack            |   ~200 |  0.5 us |
   | Frame pack (64 ch)       |  ~1500 |  3.8 us |
   | Tunnel pack              |   ~200 |  0.5 us |
   | 8b10b encode (1024 sym)  |  ~3000 |  7.5 us |
   | CRC-32 compute           |  ~2000 |  5.0 us |
   | Ring buffer R/W          |   ~500 |  1.3 us |
   +--------------------------+--------+---------+
   | Total (slave, full path) | ~13900 | 34.8 us |
   +--------------------------+--------+---------+

            Table E-1: CPU Budget per Frame

   At 48 kHz (20.83 us period), the slave must
   complete its processing within one frame period.
   The budget above (34.8 us) exceeds 20.83 us.

   Optimization strategies:

   1.  Use DMA for 8b10b encode/decode, offloading
       to the PARLIO peripheral (saves ~15 us).

   2.  Use the second RISC-V core for CRC
       computation (saves ~10 us on core 0).

   3.  Use lookup tables for 8b10b (already
       implemented in codec_8b10b.c).

   4.  Process only the slots assigned to this
       node, not the entire frame.

   With DMA and dual-core, the effective per-frame
   budget drops to approximately 8-10 us, well
   within the 20.83 us frame period.

E.3.  Suggested Task Priorities

   FreeRTOS task priorities for an AudioBus node
   (higher number = higher priority):

   +---------------------------+-----------+------+
   | Task                      | Priority  | Core |
   +---------------------------+-----------+------+
   | Frame processing ISR      | (ISR)     | 1    |
   | Frame processing task     | MAX - 2   | 1    |
   | PTP servo task            | MAX - 4   | 0    |
   | Discovery/config task     | MAX - 6   | 0    |
   | Application audio task    | MAX - 8   | 0    |
   | Tunnel processing task    | MAX - 10  | 0    |
   | Beacon/metadata task      | 10        | 0    |
   | WiFi/network stack        | 5         | 0    |
   | Idle                      | 0         | both |
   +---------------------------+-----------+------+

            Table E-2: Task Priorities

   Core pinning:

      The frame processing task MUST be pinned to
      core 1 to avoid interference from WiFi and
      other interrupts on core 0.

      The PTP servo and application tasks run on
      core 0.

E.4.  Clock Accuracy Requirements

   For sample-accurate synchronization across
   nodes, the clock accuracy requirements are:

   +---------------------+------------------------+
   | Requirement         | Value                  |
   +---------------------+------------------------+
   | PTP sync accuracy   | < 1 us                 |
   | Sample clock jitter | < 1 ns RMS             |
   | Frequency accuracy  | < 1 ppm (locked)       |
   | Holdover drift      | < 50 ppm (free-run)    |
   | PLL lock time       | < 100 ms               |
   +---------------------+------------------------+

            Table E-3: Clock Requirements

   The DS92LV1212A CDR provides sub-nanosecond
   jitter on the recovered clock, meeting the
   sample clock jitter requirement for LVDS mode.

   For Ethernet mode, the PTP servo SHOULD use a
   PI (proportional-integral) controller with:

      Kp = 0.1 (proportional gain)
      Ki = 0.001 (integral gain)
      Update rate = 8 Hz (every Sync interval)

   These values provide a settling time of
   approximately 2 seconds and steady-state
   accuracy better than 100 ns.

E.5.  Power-Over-Bus Considerations

   For installations where slave nodes are powered
   over the same cable:

   -  Use a second twisted pair for DC power.
   -  Recommended voltage: 24 VDC (allows ~20 m
      cable with 1 A load on 24 AWG).
   -  Each slave uses a local DC-DC converter
      (e.g., 24V to 3.3V).
   -  Maximum current per pair: 1.5 A (Cat5e
      rated).
   -  Signal and power pairs MUST be separate
      to avoid crosstalk.

   If using Cat5 cable:

      Pair 1 (pins 1,2): LVDS data
      Pair 2 (pins 3,6): Not used
      Pair 3 (pins 4,5): +24V power
      Pair 4 (pins 7,8): Power return (GND)


Acknowledgements

   The authors thank the open-source audio and
   embedded systems communities for their ongoing
   contributions to accessible audio networking
   technology.

   Special thanks to the ESP-IDF development team
   at Espressif Systems for the PARLIO peripheral
   driver and hardware timestamping support.

   The 8b10b encoding tables are derived from the
   original work by A. X. Widmer and P. A.
   Franaszek (IBM, 1983).


Authors' Addresses

   Sylwester Sosnowski
   DatanoiseTV
   Email: tbd@datanoise.tv

3.6.  Comparison with Existing Protocols

   AudioBus is designed to address fundamental
   limitations found in the two dominant
   networked audio protocols: AES67 and Dante.
   This section provides a formal technical
   comparison across the dimensions most
   relevant to real-time audio transport.

3.6.1.  AES67 Overview

   AES67 [AES67-2018] defines interoperability
   for audio-over-IP using RTP over UDP/IP
   (Layer 3), with Session Announcement
   Protocol (SAP) and Session Description
   Protocol (SDP) for stream discovery.  Clock
   synchronization uses IEEE 1588-2008 (PTPv2).

   Key characteristics:

   -  Transport: RTP/UDP/IPv4 multicast.
   -  Discovery: SAP with 300-second default
      announce interval.
   -  Minimum packet interval: 1000 us
      (48 samples at 48 kHz).
   -  Encoding: L24 (24-bit linear PCM).
   -  Reconfiguration: requires new SDP
      session; full stream restart.

3.6.2.  Dante Overview

   Dante (Audinate) is a proprietary networked
   audio protocol.  It uses a combination of
   mDNS/DNS-SD for discovery and proprietary
   packet formats for audio transport over
   UDP/IP.

   Key characteristics:

   -  Transport: proprietary over UDP/IP.
   -  Discovery: mDNS/DNS-SD, 3-10 seconds
      typical.
   -  Minimum packet interval: ~31 us
      (single-sample mode on Ultimo/Brooklyn).
   -  Encoding: 16, 24, or 32-bit PCM.
   -  Reconfiguration: requires Dante
      Controller; brief audio dropout on
      channel changes.
   -  Licensing: per-device royalty to
      Audinate.

3.6.3.  Comparative Analysis

   Table 3.5-1 summarizes the key differences.

   +---------------------+-----------+---------+-------+
   | Feature             | AudioBus  | AES67   | Dante |
   +---------------------+-----------+---------+-------+
   | Transport layer     | L2 (Eth)  | L3 (IP) | L3    |
   | Minimum latency     | 42 us     | 1000 us | 150us |
   | Typical low-lat.    | 125 us    | 1000 us | 250us |
   | Discovery time      | <200 ms   | 300 s   |3-10 s |
   | Dynamic reconfig.   | Hitless   | Restart | Brief |
   |                     |           |         |dropout|
   | Per-ch subscribe    | Yes       | No      | Yes   |
   | Control tunneling   | Native    | No      | No    |
   | Switch requirements | Any L2    | L3+IGMP | Dante |
   |                     | switch    | router  |certif.|
   | Redundancy failover | 0 ms      | Net-dep.| ~5 ms |
   | Protocol openness   | Open spec | Open    |Propri-|
   |                     | no fees   | std     |etary  |
   | Self-describing pkt | Yes       | No      | No    |
   | LVDS serial mode    | Yes       | No      | No    |
   | Max ch (100 Mbps)   | 64@48k    | 48@48k  | 64@48k|
   |                     | /32-bit   | /24-bit |       |
   +---------------------+-----------+---------+-------+

   Table 3.5-1: Protocol Comparison Summary

3.6.4.  Latency Advantage

   AudioBus achieves lower latency than AES67
   and Dante through three design choices:

   (a) Layer 2 transport eliminates IP and UDP
       header processing, routing table lookups,
       and TTL management.  A Layer 2 frame
       traverses a cut-through switch in 2-3 us;
       an IP packet requires store-and-forward
       processing of at minimum 5-10 us.

   (b) Single-sample packet mode (Section 10A)
       reduces packetization delay to one sample
       period (20.83 us at 48 kHz).  AES67
       mandates a minimum of 48 samples per
       packet (1000 us).

   (c) Self-describing packets eliminate the
       need for prior session negotiation.  A
       listener MAY begin decoding audio from
       the first packet received, without
       waiting for an SDP session description
       or controller handshake.

3.6.5.  Discovery Advantage

   AudioBus rapid discovery (Section 8A)
   completes in under 200 ms.  SAP announces
   at a default interval of 300 seconds
   [RFC2974], meaning a new listener must wait
   up to 5 minutes to learn of existing streams.
   Dante's mDNS-based discovery typically
   requires 3-10 seconds.

3.6.6.  Openness Advantage

   AudioBus is fully specified in this open
   document with no licensing fees, royalties,
   or certification requirements.  Dante
   requires per-device licensing from Audinate
   Pty Ltd.  AES67 is an open standard but
   mandates IP infrastructure and does not
   define a complete self-configuring system.


19.  Rapid Discovery Protocol

   (REPLACES Section 8.2 "Beacon Timing")

   This section defines an accelerated three-
   phase discovery mechanism that allows a new
   node to discover all existing streams and
   begin receiving audio within 200 ms of link
   detection.

   The three phases are:

      Phase 1 - ANNOUNCE:     0 to  50 ms
      Phase 2 - OFFER:       50 to 100 ms
      Phase 3 - BIND:       100 to 200 ms

19.1.  Phase 1: ANNOUNCE (0-50 ms)

   Upon transitioning from INIT to DISCOVER
   (Section 3.5, Figure 2), a node MUST transmit
   three BEACON packets in rapid succession.
   These are called "beacon bursts."

   Burst timing:

      Burst 1:  T_link + R
      Burst 2:  T_link + R + 15 ms
      Burst 3:  T_link + R + 30 ms

   Where:

      T_link is the time at which the physical
      link is detected.

      R is a random delay uniformly distributed
      in [0, 5) ms, derived from the low 16 bits
      of the node's UID.  This prevents
      collision when two nodes boot on the same
      switch simultaneously.

   The burst interval of 15 ms is chosen to be
   shorter than the minimum audio packet
   interval in normal mode (125 us is shorter,
   but beacon processing is lower priority;
   15 ms ensures all nodes process at least one
   burst even under load).

   Each burst beacon MUST set the FIRST_SEEN
   flag in the beacon Flags field:

      Bit 4 (0x0800): FIRST_SEEN.  Set to 1
         in beacon burst packets transmitted
         during Phase 1.  Set to 0 in all
         subsequent periodic beacons.

   The FIRST_SEEN flag signals to existing nodes
   that this beacon originates from a newly
   arrived node.  Existing nodes use this flag
   to trigger Phase 2.

   ABNF for the extended flags field:

   flags = 2OCTET  ; big-endian, 16 bits
   ; Bit 0:     Grandmaster
   ; Bit 1:     LVDS capable
   ; Bit 2:     Ethernet capable
   ; Bit 3:     Bridge node
   ; Bit 4:     FIRST_SEEN (new)
   ; Bit 5:     REDUNDANCY (Section X.1)
   ; Bit 6:     ULTRA_LOW capable (Sec 10A)
   ; Bits 7-15: Reserved, MUST be zero

19.2.  Phase 2: OFFER (50-100 ms)

   When an existing node receives a beacon with
   the FIRST_SEEN flag set and the Source UID is
   not present in its local node table, the
   existing node MUST respond within 50 ms by
   transmitting a STREAM_ANNOUNCE (Section 9.1)
   for every stream it currently publishes.

   Offer timing:

      T_offer = T_first_seen + D_offer

   Where D_offer is a per-node random delay
   uniformly distributed in [0, 50) ms:

      D_offer = (UID mod 50)  [milliseconds]

   This spreads OFFER responses across the 50 ms
   window to avoid multicast storms when many
   nodes respond to the same new arrival.

   If the existing node publishes no streams, it
   MUST NOT send a STREAM_ANNOUNCE.  The beacon
   response in the normal 1000 ms cycle is
   sufficient.

   Timeout:  If the new node receives no
   STREAM_ANNOUNCE packets within 100 ms of its
   first burst beacon, it concludes that either:

   (a) No talkers exist on the network, or
   (b) No streams are currently published.

   In either case, the node proceeds to READY
   state (Section 3.5) and begins periodic
   beaconing at the normal 1000 ms interval.

19.3.  Phase 3: BIND (100-200 ms)

   Upon receiving one or more STREAM_ANNOUNCE
   packets, the new node evaluates each against
   its subscription preferences (configured by
   the application layer) and transmits
   SUBSCRIBE packets (Section 9.3) for matching
   streams.

   The first audio packet from a subscribed
   stream arrives within the next packet
   interval after the talker processes the
   multicast group join.  For a 1000 us packet
   interval, this is at most 1 ms.  For a 125 us
   interval, it is at most 125 us.

   Total discovery-to-audio time:

      T_total = T_phase1 + T_phase2 + T_phase3
                + T_packet_interval
              <= 50 + 50 + 100 + 1
              = 201 ms (worst case, 1000 us pkt)

19.4.  Simultaneous Boot

   When two or more nodes boot simultaneously
   (within a 50 ms window), each transmits its
   beacon bursts with independent random delays
   (R).  Because the FIRST_SEEN flag is set in
   all burst beacons, each node treats the other
   as a new arrival and responds with
   STREAM_ANNOUNCE if applicable.

   The random delay R prevents burst collision
   in the common case.  If two bursts do collide
   (transmitted within the same microsecond on
   the same switch port), the switch's multicast
   forwarding delivers both frames to all ports.
   The Ethernet FCS detects any corruption from
   physical-layer collision on half-duplex links
   (which are NOT RECOMMENDED; full-duplex
   SHOULD be used).

   After the burst phase, all simultaneously
   booted nodes hold a complete picture of the
   network within 100 ms.

19.5.  Steady-State Beacon Timing

   After Phase 1 completes, the node reverts to
   periodic beaconing at the nominal interval
   defined in the main specification:

      Nominal interval:  1000 ms
      Jitter tolerance:  +/- 100 ms
      FIRST_SEEN flag:   MUST be 0

   All other beacon timing constraints from the
   main specification continue to apply.

19.6.  Timing Diagram

      New Node          Existing Nodes
         |                    |
   T=0   |---BEACON(FS=1)--->|  Burst 1
   T=15  |---BEACON(FS=1)--->|  Burst 2
   T=30  |---BEACON(FS=1)--->|  Burst 3
         |                    |
         |   Phase 2 begins   |
   T=35  |<--STREAM_ANN #1---|  Node A
   T=42  |<--STREAM_ANN #2---|  Node B
   T=68  |<--STREAM_ANN #3---|  Node C
         |                    |
         |   Phase 3 begins   |
   T=105 |---SUBSCRIBE #1--->|  To Node A
   T=106 |---SUBSCRIBE #2--->|  To Node C
         |                    |
   T=107 |<==AUDIO DATA======|  First pkt
         |                    |
   T=1030|---BEACON(FS=0)--->|  Normal
         |                    |

   FS = FIRST_SEEN flag value.
   Times in milliseconds, approximate.

      Figure 8A-1: Rapid Discovery Timing

19.7.  Comparison with Existing Protocols

   +------------------+----------+---------+------+
   | Metric           | AudioBus | AES67   |Dante |
   +------------------+----------+---------+------+
   | Discovery method | Beacon   | SAP/SDP |mDNS  |
   |                  | burst    |         |      |
   | Time to discover | <100 ms  | 300 s   |3-10s |
   | Time to audio    | <200 ms  | ~301 s  |4-11s |
   | Explicit signal  | FIRST_   | None    |None  |
   |   for new node   | SEEN     |         |      |
   | Burst redundancy | 3x       | 1x      |1x   |
   +------------------+----------+---------+------+

   Table 8A-1: Discovery Protocol Comparison


20.  Ultra-Low Latency Mode

   This section defines a single-sample packet
   mode for applications that require the
   minimum possible end-to-end latency, such as
   real-time instrument monitoring and live
   feedback systems.

20.1.  Single-Sample Packet Format

   In ultra-low latency mode, each audio packet
   carries exactly one sample per channel.  The
   Samples per Channel field (Section 10.1) is
   set to 1.

   The ULTRA_LOW flag MUST be set in the
   stream announcement Stream Flags field
   (Section 9.1):

      Bit 2 (0x2000 in Stream Flags):
         ULTRA_LOW.  Set to 1 when the stream
         operates in single-sample mode.

   ABNF addition to stream-flags:

   ; Bit 0: Active
   ; Bit 1: Persistent
   ; Bit 2: ULTRA_LOW (new)
   ; Bit 3: RATE_CHANGE_PENDING (Sec 9A)
   ; Bit 4: ADAPT_ENABLED (Sec 10B)
   ; Bits 5-15: Reserved, MUST be zero

20.2.  Minimum Frame Size

   A single-sample audio packet for one channel
   of 32-bit audio has the following wire size
   on Ethernet:

   +-----------------------------+--------+
   | Component                   | Octets |
   +-----------------------------+--------+
   | Destination MAC             |      6 |
   | Source MAC                  |      6 |
   | EtherType                   |      2 |
   | AudioBus Common Header      |     12 |
   | Audio Header (Sec 10.1)     |     20 |
   | Audio Data (1 ch x 4 B)     |      4 |
   +-----------------------------+--------+
   | Subtotal                    |     50 |
   | Padding to 64 B minimum     |     10 |
   | FCS                         |      4 |
   +-----------------------------+--------+
   | Total on wire               |     64 |
   +-----------------------------+--------+

   Table 10A-1: Minimum Ultra-Low Frame

   Note: the IEEE 802.3 minimum frame size is
   64 octets including FCS.  The 10-octet
   padding is appended by the Ethernet MAC.

   For multi-channel packets, audio data grows
   by (bit_depth / 8) octets per channel.  The
   frame exceeds 64 octets (no padding needed)
   when:

      channels > 10  (at 32-bit depth)
      channels > 14  (at 24-bit depth)
      channels > 21  (at 16-bit depth)

20.3.  Latency Analysis

   End-to-end latency from talker sample
   capture to listener sample playout through
   one cut-through Ethernet switch:

   +---------------------------+----------+
   | Component                 | Time     |
   +---------------------------+----------+
   | Talker DMA + MAC TX       | ~5 us    |
   | Wire propagation (5 m)    | ~0.025us |
   | Cut-through switch fabric | ~2 us    |
   | Wire propagation (5 m)    | ~0.025us |
   | Listener MAC RX + DMA     | ~5 us    |
   | Listener decode + route   | ~1 us    |
   +---------------------------+----------+
   | Subtotal (network path)   | ~13 us   |
   +---------------------------+----------+
   | Packetization delay       | 20.83 us |
   |   (one sample at 48 kHz)  |          |
   | Frame serialization       | ~5.12 us |
   |   (64 B at 100 Mbps)      |          |
   +---------------------------+----------+
   | Total without buffer      | ~39 us   |
   | With 50 us playout buffer | ~89 us   |
   +---------------------------+----------+

   Table 10A-2: Ultra-Low Latency Breakdown

   The 50 us playout buffer absorbs jitter from
   the cut-through switch.  Without the buffer,
   the theoretical minimum is approximately
   39 us, though this requires deterministic
   DMA scheduling.

   Conservatively stated:

      Without playout buffer: ~42 us
      With playout buffer:    ~92 us

   These figures include margin for real-world
   variation in DMA and switch timing.

20.4.  Latency Comparison

   +---------------------+--------+------+------+
   | Configuration       |AudioBus|AES67 |Dante |
   +---------------------+--------+------+------+
   | Minimum possible    | 42 us  |1000us|150 us|
   | With safety buffer  | 92 us  |1000us|250 us|
   +---------------------+--------+------+------+

   Table 10A-3: Ultra-Low Latency Comparison

   AES67 cannot achieve latencies below 1000 us
   because the minimum specified packet interval
   is 1 ms (48 samples at 48 kHz).  Dante's
   Ultimo chipset supports a single-sample mode
   at approximately 150 us minimum, but this
   mode is limited to specific hardware and is
   not available on all Dante devices.

20.5.  Bandwidth Analysis

   Single-sample mode incurs high per-packet
   overhead relative to audio payload.  The
   bandwidth for one stream is:

      BW = Fs * frame_bytes * 8  [bits/s]

   Where frame_bytes includes Ethernet header
   (14), AudioBus header (12), audio header
   (20), audio data, padding, and FCS (4).

   +------+--------+-----------+-----------+
   | Ch   | Depth  | Frame (B) | BW (Mbps) |
   +------+--------+-----------+-----------+
   |    1 | 32-bit |        64 |     24.58 |
   |    2 | 32-bit |        64 |     24.58 |
   |    2 | 24-bit |        64 |     24.58 |
   |    4 | 32-bit |        68 |     26.11 |
   |    8 | 32-bit |        84 |     32.26 |
   |   16 | 32-bit |       116 |     44.54 |
   |   32 | 32-bit |       180 |     69.12 |
   |   48 | 32-bit |       244 |     93.70 |
   |   64 | 32-bit |       308 |    118.27 |
   +------+--------+-----------+-----------+

   Table 10A-4: Ultra-Low Bandwidth at 48 kHz

   Calculation for 2 channels, 32-bit:

      Audio data = 2 * 4 = 8 bytes
      Total before pad = 14 + 12 + 20 + 8 = 54
      Padded to 60 B + 4 B FCS = 64 B
      BW = 48000 * 64 * 8 = 24,576,000 bps
         = 24.58 Mbps

   Calculation for 32 channels, 32-bit:

      Audio data = 32 * 4 = 128 bytes
      Total = 14 + 12 + 20 + 128 = 174 B
      +FCS = 178 B, round to 180 B (alignment)
      BW = 48000 * 180 * 8 = 69,120,000 bps
         = 69.12 Mbps

   Feasibility on 100 Mbps Ethernet:

      At 100 Mbps, the maximum usable bandwidth
      for audio (at 80% utilization cap per
      Section 5.7) is 80 Mbps.  Ultra-low
      latency mode is feasible for:

      - Up to 32 channels of 32-bit audio
      - Up to 48 channels of 24-bit audio
      - Up to 64 channels of 16-bit audio

      Beyond these limits, implementations MUST
      use normal (multi-sample) packet mode or
      Gigabit Ethernet.

20.6.  Mode Selection

   Ultra-low latency mode SHOULD be used only
   when the application requires end-to-end
   latency below 250 us.  For all other cases,
   the normal packet interval tiers
   (Section 10.5) provide equivalent audio
   quality with substantially lower bandwidth
   overhead and CPU utilization.

   Selection criteria:

   +----------------------------+-----------+
   | Requirement                | Mode      |
   +----------------------------+-----------+
   | Latency < 100 us          | Ultra-low |
   | Latency 100-250 us        | 125 us    |
   | Latency 250-500 us        | 250 us    |
   | Latency > 500 us          | 500+ us   |
   +----------------------------+-----------+

   Table 10A-5: Mode Selection Guide

   A talker MUST NOT use ultra-low mode if the
   network capability probe (Section X.2)
   indicates a NETWORK_QUALITY score below 70.

   Listeners MUST support ultra-low latency
   mode if they advertise the ULTRA_LOW flag
   in their beacon (bit 6 of beacon Flags).
   Listeners that do not support ultra-low
   mode MUST NOT subscribe to streams with the
   ULTRA_LOW stream flag set.


21.  Adaptive Packet Interval

   This section defines an algorithm by which a
   talker automatically adjusts its packet
   interval to match the observed quality of
   the network path.

21.1.  Design Rationale

   A fixed packet interval forces a tradeoff
   between latency (small interval) and
   robustness (large interval).  The adaptive
   algorithm allows a talker to start at a
   conservative interval and reduce it as
   network quality permits, or increase it when
   conditions degrade.

21.2.  Tier Ladder

   The adaptive algorithm operates on a discrete
   set of interval tiers, ordered from highest
   to lowest latency:

   +------+-----------+--------+----------+
   | Tier | Interval  | Samp/ch| Use case |
   |      | (us)      | @48kHz |          |
   +------+-----------+--------+----------+
   |    7 | 4000      |    192 | Max eff. |
   |    6 | 2000      |     96 | High eff.|
   |    5 | 1000      |     48 | Default  |
   |    4 | 500       |     24 | Balanced |
   |    3 | 250       |     12 | Low lat. |
   |    2 | 125       |      6 | ULL      |
   |    1 | 62.5      |      3 | Sub-ULL  |
   |    0 | 31.25     |   1(*) | Near-min |
   |   -1 | 20.83     |      1 |Ultra-low |
   +------+-----------+--------+----------+

   (*) At 48 kHz, 31.25 us yields 1.5 samples
       per interval.  The talker alternates
       between 1 and 2 samples per packet.

   Tier -1 corresponds to ultra-low latency
   mode (Section 10A) and is only available if
   the ULTRA_LOW capability is advertised.

   Table 10B-1: Adaptive Interval Tiers

21.3.  Observation Window

   The talker collects metrics over an
   observation window of 100 consecutive
   packets.  At the end of each window, the
   talker evaluates step-down and step-up
   conditions.

   Two metrics are computed:

   jitter_rms:  The root-mean-square of the
      inter-packet arrival time deviation,
      computed from PONG timestamps
      (Section 13.2) or from listener feedback
      if available.

      jitter_rms = sqrt(
        (1/N) * sum((t_i - t_expected)^2)
      )

      Where N = 100, t_i is the actual arrival
      time, and t_expected is the nominal
      arrival time based on the current packet
      interval.

   loss_ratio:  The fraction of packets lost
      in the window, derived from sequence
      number gaps (Section 10.6).

      loss_ratio = gaps / (100 + gaps)

21.4.  Threshold Conditions

   Step-down (decrease interval, lower latency):

      jitter_rms < 50 us  AND  loss_ratio == 0

   Step-up (increase interval, higher latency):

      jitter_rms > 200 us  OR  loss_ratio > 0.1%

   Neutral zone:

      If neither condition is met, the interval
      remains unchanged.

21.5.  Hysteresis

   A tier change MUST NOT occur until the
   triggering condition has been sustained for
   3 consecutive observation windows (300
   packets).

   This prevents oscillation due to transient
   network events.  The hysteresis counter
   resets to zero whenever the condition is no
   longer met.

   State machine:

     STABLE ──[condition met]──> PENDING(1)
     PENDING(1) ──[met]──> PENDING(2)
     PENDING(2) ──[met]──> CHANGE TIER
     PENDING(N) ──[not met]──> STABLE

21.6.  Tier Change Procedure

   When a tier change is triggered:

   1.  The talker computes the new packet
       interval from the tier ladder.

   2.  The talker transmits a new
       STREAM_ANNOUNCE with the updated Packet
       Interval field.

   3.  Starting at the next packet boundary,
       the talker begins transmitting at the
       new interval.

   Listeners detect the interval change by
   observing the inter-packet arrival time.
   No explicit signaling is required beyond
   the updated STREAM_ANNOUNCE, because the
   audio packet header is self-describing.

   Detection algorithm at the listener:

   1.  Compute arrival delta between consecutive
       packets: delta = t_n - t_(n-1).

   2.  If delta differs from the expected
       interval by more than 50%, update the
       expected interval:
       expected = measured delta (rounded to
       the nearest tier value).

   3.  Adjust playout buffer depth to match
       the new interval.

21.7.  ADAPT_STATE Field

   A talker using adaptive mode MUST set bit 4
   (ADAPT_ENABLED) in the Stream Flags field of
   STREAM_ANNOUNCE (Section 9.1).

   The talker also includes an ADAPT_STATE
   field in the stream announcement.  This
   field occupies the 2-octet Reserved field
   (octets 14-15 of the announce payload) when
   ADAPT_ENABLED is set:

   ABNF:

   adapt-state    = current-tier window-counter

   current-tier   = OCTET
                    ; signed, -1..7
                    ; current tier index

   window-counter = OCTET
                    ; 0..255
                    ; observation windows since
                    ; last tier change

   When ADAPT_ENABLED is not set, these two
   octets MUST be zero (maintaining backward
   compatibility with the Reserved field).

21.8.  Constraints

   The adaptive algorithm MUST NOT reduce the
   interval below the minimum tier supported
   by the talker hardware or advertised in the
   beacon.

   If a listener explicitly requests a specific
   packet interval (via application-layer
   configuration), the talker SHOULD honor
   that request and disable adaptation for the
   affected stream.

   The initial tier on stream creation MUST be
   tier 5 (1000 us) unless the application
   explicitly requests otherwise.


22.  Hitless Reconfiguration

   Because every AudioBus audio packet is self-
   describing (the header carries Channels, Bit
   Depth, and Sample Rate fields; see
   Section 10.1), receivers can adapt to stream
   parameter changes on-the-fly without prior
   negotiation.

   This section defines the procedures for
   hitless reconfiguration of active streams.

22.1.  Channel Addition

   Procedure:

   1.  Talker increments the Channels field in
       the next audio packet header.

   2.  Talker includes audio data for all
       channels (existing plus new) in the
       standard interleave order (Sec 10.3).

   3.  Talker transmits updated STREAM_ANNOUNCE
       with the new channel count.

   Listener behavior:

   1.  Listener detects N_new > N_old by reading
       the Channels field from the audio packet
       header.

   2.  Listener allocates new channel buffers,
       initialized to silence (zero samples).

   3.  Listener begins routing new channels to
       the application.

   4.  Existing channels continue without
       interruption.

   Audio impact: zero dropout, zero glitch.
   The transition is inaudible on existing
   channels.

22.2.  Channel Removal

   Procedure:

   1.  Talker decrements the Channels field in
       the next audio packet header.

   2.  Talker transmits audio data for only
       the remaining channels.

   3.  Talker transmits updated STREAM_ANNOUNCE.

   Listener behavior:

   1.  Listener detects N_new < N_old from the
       audio packet header.

   2.  Listener fades removed channels to
       silence over 1 ms (48 samples at 48 kHz)
       to avoid clicks.

   3.  Listener deallocates removed channel
       buffers after fade completes.

   4.  Remaining channels continue without
       interruption.

   Audio impact: zero dropout on remaining
   channels.  Removed channels receive a 1 ms
   fade-out.

22.3.  Bit Depth Change

   Procedure:

   1.  Talker changes the Bit Depth field in
       the next audio packet header.

   2.  Audio data in the packet uses the new
       sample width immediately.

   3.  Talker transmits updated STREAM_ANNOUNCE.

   Listener behavior:

   1.  Listener reads Bit Depth from every
       audio packet header (as it already MUST
       per Section 10.1).

   2.  Listener applies bit-depth conversion
       (Section 10.2) as needed.

   Audio impact: immediate, inaudible.  No
   buffer flush or stream interruption.

22.4.  Sample Rate Change

   A sample rate change is NOT hitless because
   it requires the listener's media clock PLL
   to re-lock (Section 7.9).  A controlled
   procedure minimizes disruption.

   New packet subtype:

      Packet Type 0x07: STREAM_RATE_CHANGE

   Payload (6 octets):

   +-------------------------------+--------+
   | Field                         | Octets |
   +-------------------------------+--------+
   | Stream ID                     |      2 |
   | New Sample Rate               |      4 |
   +-------------------------------+--------+

   ABNF:

   rate-change-payload = stream-id new-rate
   stream-id           = 2OCTET  ; big-endian
   new-rate            = 4OCTET  ; big-endian Hz

   Procedure:

   1.  Talker sets bit 3 (RATE_CHANGE_PENDING,
       0x1000) in the Stream Flags of
       STREAM_ANNOUNCE.

   2.  Talker transmits STREAM_RATE_CHANGE
       packet 3 times at 33 ms intervals
       (total: 100 ms advance notice).

   3.  At T_change = T_first_rate_change +
       100 ms, the talker:

       a. Updates Sample Rate in the audio
          packet header to the new rate.

       b. Recomputes Samples per Channel
          based on the new rate and current
          packet interval.

       c. Clears RATE_CHANGE_PENDING in
          STREAM_ANNOUNCE.

   4.  Talker transmits updated STREAM_ANNOUNCE
       with the new sample rate.

   Listener behavior:

   1.  On receiving STREAM_RATE_CHANGE, the
       listener begins PLL re-lock to the new
       sample rate.

   2.  At T_change, the listener flushes its
       playout buffer.

   3.  The listener mutes output during PLL
       re-lock (typically 20-50 ms per
       Section E.4).

   4.  When the PLL is locked, playout resumes.

   Audio impact: brief mute of approximately
   50 ms.  No stream teardown or restart.

22.5.  State Diagram

         +------+   ch_add    +------+
         |      |------------>|      |
         |      |   ch_remove |      |
   ----->|STABLE|<------------|ADAPT |
         |      |   depth_chg |      |
         |      |<------------|      |
         +------+             +------+
            |                    ^
            |  rate_change       |
            v                    |
         +------+  PLL locked +------+
         |      |------------>|      |
         | MUTE |             |RELOCK|
         |      |<------------|      |
         +------+             +------+

      Figure 9A-1: Hitless Reconfiguration
                   State Diagram

   STABLE:  Normal operation.  Audio flows.

   ADAPT:  Channel or depth change in progress.
      Listener adjusts buffers.  Audio continues
      on all unaffected channels.  Transition
      back to STABLE is immediate (< 1 ms).

   MUTE:  Rate change announced.  Listener
      mutes output and prepares PLL.

   RELOCK:  Listener PLL is re-locking to the
      new sample rate.  Duration: 20-50 ms.
      On lock, transition to STABLE.

22.6.  Comparison with Existing Protocols

   +-------------------+---------+--------+------+
   | Operation         |AudioBus | AES67  |Dante |
   +-------------------+---------+--------+------+
   | Channel add       | Hitless | New    |Ctrl  |
   |                   | (0 ms)  | SDP    |reconf|
   |                   |         | session|~100ms|
   | Channel remove    | Hitless | New    |Ctrl  |
   |                   | (0 ms)  | SDP    |reconf|
   | Bit depth change  | Hitless | Not    |Not   |
   |                   | (0 ms)  | supp.  |supp. |
   | Sample rate chg   | ~50 ms  | Full   |Full  |
   |                   | mute    |restart |restr.|
   +-------------------+---------+--------+------+

   Table 9A-1: Reconfiguration Comparison


23.  Redundancy and Seamless Failover

   This section defines dual-path redundancy
   for AudioBus Ethernet transport.  Redundancy
   ensures uninterrupted audio delivery when a
   network link or switch fails.

23.1.  Redundancy Modes

   Two redundancy modes are defined:

   Mode 1 - Dual-NIC (Seamless Failover):

      The talker transmits an identical copy of
      every audio packet on two independent
      Ethernet interfaces (NIC-A and NIC-B).
      The two interfaces MUST be connected to
      separate physical switches or separate
      ports of a switch stack to provide path
      diversity.

   Mode 2 - Bonded (Bandwidth Doubling):

      Two Ethernet interfaces are bonded using
      IEEE 802.3ad Link Aggregation (LACP).
      Packets are distributed across both links
      by the bonding driver.  This mode doubles
      available bandwidth but does NOT provide
      seamless failover; it relies on LACP
      failover (typically 50-100 ms).

   Mode 1 is RECOMMENDED for live performance
   and broadcast applications.  Mode 2 is
   RECOMMENDED for high-channel-count studio
   installations where bandwidth is the
   primary constraint.

23.2.  Dual-NIC Packet Duplication

   In Mode 1, the talker transmits every packet
   (AUDIO, BEACON, STREAM_ANNOUNCE, and all
   other types) on both interfaces with
   identical content, including:

   -  Identical Source UID
   -  Identical Sequence Number
   -  Identical Presentation Timestamp
   -  Identical audio payload

   The Source MAC address MAY differ (each NIC
   has its own MAC).  Listeners MUST NOT use
   the Source MAC for packet deduplication;
   they MUST use the Source UID and Sequence
   Number.

23.3.  Duplicate Detection Algorithm

   A listener receiving packets from two paths
   MUST detect and discard duplicates:

   1.  Maintain a per-source last_seq table
       indexed by Source UID.

   2.  On packet arrival, compute:

          is_dup = (source_uid == known_uid)
                   AND (seq == last_seq[uid])

   3.  If is_dup is true, discard the packet
       and increment the duplicate counter.

   4.  If is_dup is false, process the packet
       and update last_seq[uid] = seq.

   Because the listener uses whichever copy
   arrives first, path diversity provides:

   -  Zero-dropout failover:  If one path
      fails, the other continues without
      interruption.

   -  Lower effective jitter:  The listener
      receives min(jitter_A, jitter_B) for
      each packet.

23.4.  Dual-Path Topology

        Talker
       /      \
    NIC-A    NIC-B
      |        |
   Switch-A  Switch-B
      |        |
    NIC-A    NIC-B
       \      /
       Listener

   Figure X.1-1: Dual-Path Redundancy
                  Topology

   If only one switch is available, the two
   NIC connections MAY attach to different
   ports on the same switch.  This provides
   NIC and cable redundancy but not switch
   redundancy.

23.5.  REDUNDANCY Beacon Flag

   A node that supports Mode 1 redundancy
   MUST set bit 5 (0x0400) in the beacon
   Flags field:

      Bit 5 (0x0400): REDUNDANCY.  Set to 1
         if the node has two active Ethernet
         interfaces and is transmitting
         duplicate packets.

   Listeners use this flag to expect packets
   from two paths and to enable duplicate
   detection.

23.6.  Failover Timing

   In Mode 1, failover is defined as the time
   between the last packet on the failed path
   and the first packet processed from the
   surviving path.

   Because both paths carry identical packets
   and the listener always uses the first
   arrival, the failover time is:

      T_failover = 0 ms

   There is no detection delay, no switchover
   negotiation, and no buffer flush.  The
   surviving path was already delivering
   packets; the failed path simply stops.

23.7.  Failover Comparison

   +---------------------+--------+--------+------+
   | Metric              |AudioBus| AES67  |Dante |
   +---------------------+--------+--------+------+
   | Failover time       | 0 ms   | Net-   | ~5ms |
   |                     |        | depend.|      |
   | Mechanism           | Dual-  | RSTP / | Dante|
   |                     | path   | PTP    | sec. |
   |                     | dedup  | re-elec| NIC  |
   | Audio dropout       | None   | Yes    | Brief|
   | Extra hardware      | 2nd    | Redund.| 2nd  |
   |                     | NIC    | switch | NIC  |
   +---------------------+--------+--------+------+

   Table X.1-1: Failover Comparison


24.  Network Capability Probing

   On startup, before entering the DISCOVER
   state, a node MAY characterize its network
   environment to determine the maximum safe
   packet rate and optimal default interval.

24.1.  Probe Sequence

   The probe consists of three phases, each
   using the PING/PONG mechanism defined in
   Section 13.

   Phase 1 - Coarse Jitter Measurement:

      The node sends 10 PING packets at 1 ms
      intervals to the discovery multicast
      address.  Any node that has been on the
      network for more than 5 seconds MUST
      respond with PONG.

      From the PONG responses, the probing
      node computes:

         jitter_coarse = stddev(RTT) / 2

      If no PONG is received, the node is
      alone on the network and skips further
      probing.

   Phase 2 - Fine Jitter Measurement:

      The node sends 10 PING packets at 100 us
      intervals.  From the PONG responses:

         jitter_fine = stddev(RTT) / 2

   Phase 3 - Bandwidth Probing:

      The node sends bursts of 10 back-to-back
      PING packets (no inter-packet gap) and
      measures loss.  It repeats with increasing
      burst size (10, 20, 50, 100 packets)
      until loss is detected.

         loss_threshold = burst_size at which
            loss_ratio > 0

24.2.  NETWORK_QUALITY Score

   The probing node computes a composite score:

      Q = 100
      Q -= min(jitter_coarse / 10, 30)
      Q -= min(jitter_fine * 10, 30)
      Q -= min((100 - loss_threshold), 40)

   Where jitter values are in microseconds and
   loss_threshold is the burst size at first
   loss.

   Score interpretation:

   +----------+---------+-----------------+
   | Score    | Quality | Recommended     |
   |          |         | min interval    |
   +----------+---------+-----------------+
   | 90-100   | Optimal | 31.25 us        |
   | 70-89    | Good    | 125 us          |
   | 50-69    | Fair    | 500 us          |
   | 30-49    | Poor    | 1000 us         |
   |  0-29    | Bad     | 2000 us         |
   +----------+---------+-----------------+

   Table X.2-1: Network Quality Scores

24.3.  MAX_SAFE_INTERVAL

   Based on the NETWORK_QUALITY score, the
   node computes a MAX_SAFE_INTERVAL:

      if Q >= 90: MAX_SAFE_INTERVAL = 31.25 us
      if Q >= 70: MAX_SAFE_INTERVAL = 125 us
      if Q >= 50: MAX_SAFE_INTERVAL = 500 us
      if Q >= 30: MAX_SAFE_INTERVAL = 1000 us
      else:       MAX_SAFE_INTERVAL = 2000 us

   The adaptive algorithm (Section 10B) MUST
   NOT reduce the packet interval below
   MAX_SAFE_INTERVAL.

24.4.  Beacon Advertisement

   The NETWORK_QUALITY score and
   MAX_SAFE_INTERVAL are advertised in the
   beacon.  Two new fields are appended after
   the existing Node Name field:

   ABNF:

   beacon-ext = network-quality
                max-safe-interval

   network-quality    = OCTET
                        ; 0..100
   max-safe-interval  = 2OCTET
                        ; big-endian, us

   Nodes that do not perform probing MUST set
   network-quality to 0xFF (unknown) and
   max-safe-interval to 1000 (default).

   These fields are present only when the
   beacon payload length exceeds the base
   beacon size.  Receivers MUST check the
   payload length before reading these fields
   to maintain backward compatibility.

24.5.  Probe Packet Format

   Probes reuse the existing PING packet format
   (Section 13.1, Type 0x40) with an additional
   flag:

      Bit 1 (0x4000) of the common header
      Flags field: PROBE.  Set to 1 for probe
      PINGs.  Set to 0 for normal PINGs.

   A node receiving a PING with the PROBE flag
   MUST respond with PONG immediately,
   bypassing any normal rate-limiting on PONG
   responses.

   Probe PINGs MUST NOT be transmitted after
   the initial startup probe sequence.  A node
   that has been in DISCOVER or later states
   for more than 5 seconds MUST NOT send probe
   PINGs.


Appendix F.  Latency Comparison with AES67
             and Dante

F.1.  Methodology

   The latency figures in this appendix are
   derived from analytical calculation based on
   protocol specifications and measured hardware
   characteristics.  All figures assume:

   -  IEEE 802.3 100BASE-TX or 1000BASE-T
      Ethernet.
   -  Cut-through switching (not store-and-
      forward) where noted.
   -  5-meter copper patch cables.
   -  Single switch hop unless noted.
   -  48 kHz sample rate, 32-bit depth.

F.2.  Component Latency Definitions

   T_pac:   Packetization delay.  Time to
            accumulate N samples.
            = N / Fs

   T_ser:   Serialization delay.  Time to
            transmit one frame on the wire.
            = frame_bytes * 8 / link_speed

   T_sw:    Switch fabric delay.
            Cut-through: ~2 us.
            Store-and-forward: T_ser + ~2 us.

   T_prop:  Propagation delay.
            = cable_length / (2/3 * c)
            = 5 m / 2e8 = 0.025 us

   T_dma:   DMA transfer from NIC to memory.
            ~3-5 us typical.

   T_buf:   Playout buffer depth.
            = B_samples / Fs

F.3.  AudioBus Latency Calculations

   Ultra-low (single-sample, Section 10A):

      T_pac = 1 / 48000 = 20.83 us
      T_ser = 64 * 8 / 100e6 = 5.12 us
      T_sw  = 2 us (cut-through)
      T_prop= 2 * 0.025 = 0.05 us
      T_dma = 2 * 5 = 10 us
      T_decode = 1 us
      ---
      Subtotal = 39.0 us
      Rounded:   42 us (with margin)

      With 50 us playout buffer:  92 us.

   Low-latency (125 us interval):

      T_pac = 6 / 48000 = 125 us
      T_ser = 68 * 8 / 100e6 = 5.4 us
      T_sw  = 2 us
      T_prop= 0.05 us
      T_dma = 10 us
      ---
      Subtotal = 142.5 us

      With 125 us playout buffer: ~268 us.
      Rounded typical: 125 us network +
        125 us buffer = 250 us.

   Default (1000 us interval):

      T_pac = 48 / 48000 = 1000 us
      T_ser = 218 * 8 / 100e6 = 17.4 us
        (2ch 32-bit: 14+12+20+8+4 = 58 B;
         but 48 smp*2ch*4B = 384 + 46 = 430 B
         rounded: ~432 B)
      T_sw  = 2 us
      T_prop= 0.05 us
      T_dma = 10 us
      ---
      Subtotal = ~1030 us

      With 2000 us playout buffer: ~3030 us.
      Rounded: 3 ms.

F.4.  AES67 Latency Calculations

   AES67 minimum (per AES67-2018 Section 8):

      Minimum packet time: 1000 us (48 smp)
      UDP/IP header: 28 octets additional
      RTP header: 12 octets additional
      Total overhead: 40 octets beyond L2

      T_pac = 1000 us
      T_ser: larger frame due to IP/UDP/RTP
      T_sw  = 10 us (store-and-forward
              required for L3 routing)
      ---
      Minimum achievable: ~1000 us

   Default (AES67):

      Typical packet time: 4000 us (192 smp)
      With recommended playout: ~4000 us.

F.5.  Dante Latency Calculations

   Dante minimum (Ultimo/Brooklyn, per
   Audinate published specifications):

      Minimum setting: 150 us (device-
      dependent, requires Ultimo chipset).

   Dante typical:

      Low-latency setting: 250 us
      Default setting: 1000 us

F.6.  Comprehensive Comparison Table

   +-------------------+--------+-------+-------+
   | Scenario          |AudioBus| AES67 | Dante |
   +-------------------+--------+-------+-------+
   | Min possible      | 42 us  |1000 us| 150 us|
   | Min with buffer   | 92 us  |1000 us| 250 us|
   | Typical low-lat   | 250 us |1000 us| 500 us|
   | Default           | 3 ms   | 4 ms  | 1 ms  |
   | Discovery time    | <200ms | 300 s |3-10 s |
   | Channel change    | 0 ms   |restart| ~100ms|
   | Failover          | 0 ms   |net-dep| ~5 ms |
   +-------------------+--------+-------+-------+

   Table F-1: Comprehensive Latency Comparison

F.7.  Maximum Channel Capacity (100 Mbps)

   Available bandwidth at 80% utilization:
   80 Mbps.

   AudioBus (1000 us interval, 32-bit):

      Per-channel BW = 32 * 48000
                     = 1,536,000 bps
      Overhead per stream = (48000/48)*46*8
                          = 368,000 bps
      64-ch stream = 64*1.536 + 0.368 Mbps
                   = 98.672 Mbps

      This exceeds 80 Mbps.  At 24-bit depth:
      64-ch = 64*24*48000 + 368000
            = 73.728 + 0.368 = 74.096 Mbps

      At 32-bit, max channels on 100 Mbps:
      N = floor((80e6 - 368000)
          / (32 * 48000))
        = floor(79632000 / 1536000)
        = 51 channels

      At 24-bit:
      N = floor(79632000 / (24 * 48000))
        = floor(79632000 / 1152000)
        = 69 channels (capped at 64)

   AES67 (1000 us, 24-bit, L24 encoding):

      Per-channel BW = 24 * 48000
                     = 1,152,000 bps
      RTP/UDP/IP overhead per packet:
        12(RTP) + 8(UDP) + 20(IP) + 14(Eth)
        = 54 octets
      Overhead = (48000/48) * 54 * 8
               = 432,000 bps
      Max channels:
      N = floor((80e6 - 432000) / 1152000)
        = 69 (capped at spec max)
      Practical AES67 limit: ~48 channels
      per stream per common implementations.

   Dante (1000 us, 32-bit):

      Similar to AudioBus but with additional
      UDP/IP overhead.  Practical limit:
      64 channels at 48 kHz per published
      specifications.

   +--------------------+---------+------+------+
   | Config             |AudioBus |AES67 |Dante |
   +--------------------+---------+------+------+
   | 48k/32-bit/100Mbps |  51 ch  | N/A  |64 ch |
   | 48k/24-bit/100Mbps |  64 ch  |48 ch |64 ch |
   | 48k/16-bit/100Mbps |  64 ch  |64 ch |64 ch |
   | 96k/24-bit/100Mbps |  34 ch  |24 ch |32 ch |
   +--------------------+---------+------+------+

   Table F-2: Channel Capacity at 100 Mbps

   Note: AudioBus 32-bit capacity at 100 Mbps
   is 51 channels (not 64) because the L2
   overhead per packet, while smaller than
   L3, is amortized over fewer payload bytes
   in 32-bit mode.  On Gigabit Ethernet, 64
   channels of 32-bit audio at 96 kHz consume
   approximately 148 Mbps, well within the
   800 Mbps usable budget.

F.8.  Why Layer 2 Is Faster than Layer 3

   AudioBus operates at Layer 2 (Ethernet
   frames with a dedicated EtherType).  AES67
   and Dante operate at Layer 3 (IP/UDP).

   Layer 2 advantages:

   1.  No IP header (20 B) or UDP header (8 B).
       This saves 28 octets per packet,
       reducing serialization delay.

   2.  No IP routing table lookup at each hop.
       L2 switches forward based on MAC address
       table (CAM), which is a single-cycle
       hardware operation.

   3.  Cut-through switching is possible at L2.
       L3 routers MUST receive the full IP
       header before making a forwarding
       decision.  L2 switches MAY begin
       forwarding after receiving only the
       destination MAC (6 octets, 0.48 us
       at 100 Mbps).

   4.  No fragmentation.  IP packets may be
       fragmented by intermediate routers,
       adding reassembly delay.  L2 frames
       are never fragmented.

   5.  No ARP resolution.  IP multicast
       requires IGMP group management.  L2
       multicast requires only MAC address
       filtering, which is handled in hardware
       by all commodity switches.

   6.  No TTL decrement or header checksum
       recomputation at each hop.

   The cumulative effect is 2-10 us lower
   per-hop latency for L2 versus L3 transport,
   which compounds across multiple switch hops
   and is significant for ultra-low latency
   applications.

```
