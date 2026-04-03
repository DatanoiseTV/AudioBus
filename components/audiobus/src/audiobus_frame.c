/*
 * AudioBus — Frame packing/unpacking and slot map computation
 *
 * The "smart packing" algorithm:
 *   1. At config time, collect channel requests from all discovered nodes
 *   2. Sort by priority: audio first (by node order), then tunnels (by BW)
 *   3. Compute exact byte offsets for every slot — deterministic bin-packing
 *   4. Verify total fits within frame size, CRC the slot map
 *   5. Distribute identical slot map to all nodes
 *
 * At runtime, pack/unpack is a simple memcpy-per-slot from the slot map.
 * No per-frame allocation, no branching on channel counts — pure DMA-friendly
 * linear memory access. This guarantees deterministic latency and zero jitter.
 *
 * SPDX-License-Identifier: MIT
 */

#include "audiobus_types.h"
#include <string.h>
#include <stdlib.h>

/* CRC-32 (same polynomial as Ethernet) for frame integrity.
 * FIX #15: Use constructor attribute for thread-safe init at load time. */
static uint32_t crc32_table[256];

__attribute__((constructor))
static void crc32_init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
}

uint32_t abus_crc32(const uint8_t *data, int len) {
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* CRC-8 for slot map verification (lightweight) */
uint8_t abus_crc8(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xB2 : 0);
        }
    }
    return crc;
}

/* ---------------------------------------------------------------------------
 * Slot map computation — the deterministic packing algorithm
 * --------------------------------------------------------------------------- */

/**
 * Compute the slot map for a given bus configuration.
 *
 * @param nodes         Array of node descriptors (index 0 = master)
 * @param node_count    Number of nodes (including master)
 * @param sample_rate   Bus sample rate
 * @param bit_depth     Default bit depth
 * @param out_slotmap   Output slot map
 * @return 0 on success, -1 if channels don't fit in frame
 */
int abus_slotmap_compute(const abus_node_descriptor_t *nodes, uint8_t node_count,
                         abus_sample_rate_t sample_rate, abus_bit_depth_t bit_depth,
                         abus_slotmap_t *out_slotmap) {
    memset(out_slotmap, 0, sizeof(*out_slotmap));

    /* Determine frame size from sample rate */
    uint16_t frame_bytes;
    if (sample_rate == ABUS_SR_48000 || sample_rate == ABUS_SR_44100) {
        frame_bytes = ABUS_FRAME_BYTES_48K;
    } else {
        frame_bytes = ABUS_FRAME_BYTES_96K;
    }

    out_slotmap->frame_bytes = frame_bytes;
    out_slotmap->payload_bytes = frame_bytes - ABUS_OVERHEAD;

    uint8_t bytes_per_sample;
    switch (bit_depth) {
        case ABUS_DEPTH_16: bytes_per_sample = 2; break;
        case ABUS_DEPTH_24: bytes_per_sample = 3; break;
        case ABUS_DEPTH_32: bytes_per_sample = 4; break;
        default: return -1;
    }

    /*
     * Layout within the payload region (after SYNC + HEADER, before CRC):
     *
     * [DN_SIDEBAND:1][DN_AUDIO:...][DN_AUX:...][GUARD:2][UP_SIDEBAND:1][UP_AUDIO:...][UP_AUX:...]
     *
     * The packing is greedy by node order:
     *   - For each node (1..N), allocate its downstream channels in order
     *   - For each node (1..N), allocate its upstream channels in order
     *   - Then allocate tunnel bandwidth for each node
     *   - Remaining bytes become idle padding
     */

    uint16_t payload_avail = out_slotmap->payload_bytes;
    uint16_t offset = ABUS_PAYLOAD_OFFSET;  /* Current write position in frame */

    /* --- Downstream sideband byte --- */
    out_slotmap->dn_sideband_offset = offset;
    offset += ABUS_AUX_SIDEBAND_BYTES;
    payload_avail -= ABUS_AUX_SIDEBAND_BYTES;

    /* --- Downstream audio --- */
    out_slotmap->dn_audio_offset = offset;
    uint16_t dn_audio_bytes = 0;
    uint8_t slot_idx = 0;

    for (uint8_t n = 0; n < node_count; n++) {
        for (uint8_t ch = 0; ch < nodes[n].max_dn_channels; ch++) {
            if (slot_idx >= ABUS_MAX_CHANNELS_TOTAL) goto overflow;

            abus_audio_slot_t *slot = &out_slotmap->audio_slots[slot_idx++];
            slot->byte_offset = offset;
            slot->byte_width = bytes_per_sample;
            slot->channel_id = slot_idx - 1;
            slot->node_id = n;
            slot->direction = ABUS_DIR_DOWNSTREAM;

            offset += bytes_per_sample;
            dn_audio_bytes += bytes_per_sample;
        }
    }
    out_slotmap->dn_audio_bytes = dn_audio_bytes;

    /* --- Downstream aux (tunnel data) --- */
    out_slotmap->dn_aux_offset = offset;
    uint16_t dn_aux_bytes = 0;
    uint8_t tun_idx = 0;

    for (uint8_t n = 0; n < node_count; n++) {
        if (nodes[n].tunnel_bw_request == 0) continue;

        uint16_t bw = nodes[n].tunnel_bw_request;

        /* GPIO tunnel: fixed 2 bytes per node */
        if (nodes[n].tunnel_request & (1 << ABUS_TUNNEL_GPIO)) {
            if (tun_idx >= ABUS_MAX_TUNNEL_SLOTS) goto overflow;
            abus_tunnel_slot_t *ts = &out_slotmap->tunnel_slots[tun_idx++];
            ts->byte_offset = offset;
            ts->byte_width = 2;
            ts->tunnel_type = ABUS_TUNNEL_GPIO;
            ts->node_id = n;
            ts->direction = ABUS_DIR_DOWNSTREAM;
            offset += 2;
            dn_aux_bytes += 2;
            bw = (bw > 2) ? bw - 2 : 0;
        }

        /* MIDI tunnel: fixed 3 bytes per node (status + data1 + data2) */
        if (nodes[n].tunnel_request & (1 << ABUS_TUNNEL_MIDI)) {
            if (tun_idx >= ABUS_MAX_TUNNEL_SLOTS) goto overflow;
            abus_tunnel_slot_t *ts = &out_slotmap->tunnel_slots[tun_idx++];
            ts->byte_offset = offset;
            ts->byte_width = 3;
            ts->tunnel_type = ABUS_TUNNEL_MIDI;
            ts->node_id = n;
            ts->direction = ABUS_DIR_DOWNSTREAM;
            offset += 3;
            dn_aux_bytes += 3;
            bw = (bw > 3) ? bw - 3 : 0;
        }

        /* SPI/I2C tunnel: remaining requested bandwidth */
        if (bw > 0 && (nodes[n].tunnel_request & ((1 << ABUS_TUNNEL_SPI) | (1 << ABUS_TUNNEL_I2C)))) {
            if (tun_idx >= ABUS_MAX_TUNNEL_SLOTS) goto overflow;
            abus_tunnel_slot_t *ts = &out_slotmap->tunnel_slots[tun_idx++];
            ts->byte_offset = offset;
            ts->byte_width = bw;
            ts->tunnel_type = (nodes[n].tunnel_request & (1 << ABUS_TUNNEL_SPI)) ?
                              ABUS_TUNNEL_SPI : ABUS_TUNNEL_I2C;
            ts->node_id = n;
            ts->direction = ABUS_DIR_DOWNSTREAM;
            offset += bw;
            dn_aux_bytes += bw;
        }
    }
    out_slotmap->dn_aux_bytes = dn_aux_bytes;

    /* --- Guard / turnaround --- */
    offset += ABUS_GUARD_LEN;

    /* --- Upstream sideband byte --- */
    out_slotmap->up_sideband_offset = offset;
    offset += ABUS_AUX_SIDEBAND_BYTES;

    /* --- Upstream audio --- */
    out_slotmap->up_audio_offset = offset;
    uint16_t up_audio_bytes = 0;

    for (uint8_t n = 0; n < node_count; n++) {
        for (uint8_t ch = 0; ch < nodes[n].max_up_channels; ch++) {
            if (slot_idx >= ABUS_MAX_CHANNELS_TOTAL) goto overflow;

            abus_audio_slot_t *slot = &out_slotmap->audio_slots[slot_idx++];
            slot->byte_offset = offset;
            slot->byte_width = bytes_per_sample;
            slot->channel_id = slot_idx - 1;
            slot->node_id = n;
            slot->direction = ABUS_DIR_UPSTREAM;

            offset += bytes_per_sample;
            up_audio_bytes += bytes_per_sample;
        }
    }
    out_slotmap->up_audio_bytes = up_audio_bytes;

    /* --- Upstream aux (mirror of downstream tunnel allocation) --- */
    out_slotmap->up_aux_offset = offset;
    uint16_t up_aux_bytes = 0;

    for (uint8_t n = 0; n < node_count; n++) {
        if (nodes[n].tunnel_bw_request == 0) continue;
        uint16_t bw = nodes[n].tunnel_bw_request;

        if (nodes[n].tunnel_request & (1 << ABUS_TUNNEL_GPIO)) {
            if (tun_idx >= ABUS_MAX_TUNNEL_SLOTS) goto overflow;
            abus_tunnel_slot_t *ts = &out_slotmap->tunnel_slots[tun_idx++];
            ts->byte_offset = offset;
            ts->byte_width = 2;
            ts->tunnel_type = ABUS_TUNNEL_GPIO;
            ts->node_id = n;
            ts->direction = ABUS_DIR_UPSTREAM;
            offset += 2;
            up_aux_bytes += 2;
            bw = (bw > 2) ? bw - 2 : 0;
        }

        if (nodes[n].tunnel_request & (1 << ABUS_TUNNEL_MIDI)) {
            if (tun_idx >= ABUS_MAX_TUNNEL_SLOTS) goto overflow;
            abus_tunnel_slot_t *ts = &out_slotmap->tunnel_slots[tun_idx++];
            ts->byte_offset = offset;
            ts->byte_width = 3;
            ts->tunnel_type = ABUS_TUNNEL_MIDI;
            ts->node_id = n;
            ts->direction = ABUS_DIR_UPSTREAM;
            offset += 3;
            up_aux_bytes += 3;
            bw = (bw > 3) ? bw - 3 : 0;
        }

        if (bw > 0 && (nodes[n].tunnel_request & ((1 << ABUS_TUNNEL_SPI) | (1 << ABUS_TUNNEL_I2C)))) {
            if (tun_idx >= ABUS_MAX_TUNNEL_SLOTS) goto overflow;
            abus_tunnel_slot_t *ts = &out_slotmap->tunnel_slots[tun_idx++];
            ts->byte_offset = offset;
            ts->byte_width = bw;
            ts->tunnel_type = (nodes[n].tunnel_request & (1 << ABUS_TUNNEL_SPI)) ?
                              ABUS_TUNNEL_SPI : ABUS_TUNNEL_I2C;
            ts->node_id = n;
            ts->direction = ABUS_DIR_UPSTREAM;
            offset += bw;
            up_aux_bytes += bw;
        }
    }
    out_slotmap->up_aux_bytes = up_aux_bytes;

    out_slotmap->num_audio_slots = slot_idx;
    out_slotmap->num_tunnel_slots = tun_idx;

    /* Check fit: offset must not exceed frame_bytes - CRC_LEN */
    if (offset > frame_bytes - ABUS_CRC_LEN) {
        goto overflow;
    }

    /* Compute slot map CRC for config verification */
    out_slotmap->slotmap_crc = abus_crc8((const uint8_t *)out_slotmap->audio_slots,
                                         slot_idx * sizeof(abus_audio_slot_t));
    return 0;

overflow:
    return -1;
}

/* ---------------------------------------------------------------------------
 * Frame packing — build a wire-format frame from audio + aux data
 * --------------------------------------------------------------------------- */

/**
 * Pack a complete bus frame.
 *
 * @param slotmap       Active slot map
 * @param frame_counter Monotonic frame counter
 * @param dn_audio      Downstream audio samples (interleaved int32_t, dn_channels wide)
 * @param up_audio      Upstream audio samples (interleaved int32_t, up_channels wide)
 * @param dn_aux        Downstream aux buffer (pre-packed tunnel data)
 * @param up_aux        Upstream aux buffer
 * @param dn_sideband   Downstream sideband byte
 * @param up_sideband   Upstream sideband byte
 * @param out_frame     Output frame buffer (must be slotmap->frame_bytes long)
 */
void abus_frame_pack(const abus_slotmap_t *slotmap,
                     uint16_t frame_counter,
                     const int32_t *dn_audio,
                     const int32_t *up_audio,
                     const uint8_t *dn_aux, uint16_t dn_aux_len,
                     const uint8_t *up_aux, uint16_t up_aux_len,
                     uint8_t dn_sideband,
                     uint8_t up_sideband,
                     uint8_t sample_rate_flag,
                     uint8_t bit_depth_flag,
                     uint8_t node_count,
                     uint8_t *out_frame) {
    uint16_t flen = slotmap->frame_bytes;
    memset(out_frame, 0, flen);

    /* --- Sync --- */
    out_frame[0] = K28_5;   /* Comma (will be encoded as K-char by 8b10b) */
    out_frame[1] = K28_1;   /* SOF */

    /* --- Header --- */
    abus_frame_header_t *hdr = (abus_frame_header_t *)&out_frame[ABUS_HEADER_OFFSET];
    hdr->frame_counter = frame_counter;
    hdr->frame_type = ABUS_FRAME_NORMAL;
    hdr->flags = sample_rate_flag | bit_depth_flag | ABUS_FLAG_SIDEBAND;
    hdr->node_count = node_count;
    hdr->dn_audio_slots = 0;
    hdr->up_audio_slots = 0;
    hdr->slotmap_crc = slotmap->slotmap_crc;

    /* --- Downstream sideband --- */
    out_frame[slotmap->dn_sideband_offset] = dn_sideband;

    /* --- Pack downstream audio into slot positions --- */
    uint8_t dn_ch = 0, up_ch = 0;
    for (int i = 0; i < slotmap->num_audio_slots; i++) {
        const abus_audio_slot_t *slot = &slotmap->audio_slots[i];
        uint16_t off = slot->byte_offset;
        int32_t sample;

        if (slot->direction == ABUS_DIR_DOWNSTREAM) {
            sample = dn_audio ? dn_audio[dn_ch++] : 0;
            hdr->dn_audio_slots++;
        } else {
            sample = up_audio ? up_audio[up_ch++] : 0;
            hdr->up_audio_slots++;
        }

        /* Pack sample in big-endian (MSB first) at the slot's byte offset */
        switch (slot->byte_width) {
            case 4:
                out_frame[off + 0] = (sample >> 24) & 0xFF;
                out_frame[off + 1] = (sample >> 16) & 0xFF;
                out_frame[off + 2] = (sample >>  8) & 0xFF;
                out_frame[off + 3] = (sample >>  0) & 0xFF;
                break;
            case 3:
                out_frame[off + 0] = (sample >> 24) & 0xFF;
                out_frame[off + 1] = (sample >> 16) & 0xFF;
                out_frame[off + 2] = (sample >>  8) & 0xFF;
                break;
            case 2:
                out_frame[off + 0] = (sample >> 24) & 0xFF;
                out_frame[off + 1] = (sample >> 16) & 0xFF;
                break;
        }
    }

    /* --- Direction guard / turnaround marker --- */
    uint16_t guard_offset = slotmap->dn_aux_offset + slotmap->dn_aux_bytes;
    out_frame[guard_offset + 0] = K28_5;    /* Comma */
    out_frame[guard_offset + 1] = K27_7;    /* Turnaround */

    /* --- Upstream sideband --- */
    out_frame[slotmap->up_sideband_offset] = up_sideband;

    /* --- Pack aux/tunnel data --- */
    if (dn_aux && dn_aux_len > 0 && dn_aux_len <= slotmap->dn_aux_bytes) {
        memcpy(&out_frame[slotmap->dn_aux_offset], dn_aux, dn_aux_len);
    }
    if (up_aux && up_aux_len > 0 && up_aux_len <= slotmap->up_aux_bytes) {
        memcpy(&out_frame[slotmap->up_aux_offset], up_aux, up_aux_len);
    }

    /* --- CRC-32 over entire frame (excluding CRC field itself) --- */
    uint32_t crc = abus_crc32(out_frame, flen - ABUS_CRC_LEN);
    out_frame[flen - 4] = (crc >> 24) & 0xFF;
    out_frame[flen - 3] = (crc >> 16) & 0xFF;
    out_frame[flen - 2] = (crc >>  8) & 0xFF;
    out_frame[flen - 1] = (crc >>  0) & 0xFF;
}

/* ---------------------------------------------------------------------------
 * Frame unpacking — extract audio + aux from a received wire-format frame
 * --------------------------------------------------------------------------- */

/**
 * Validate and unpack a received frame.
 *
 * @param slotmap       Active slot map (must match sender's)
 * @param frame         Received frame data
 * @param frame_len     Length of received frame
 * @param out_dn_audio  Output downstream audio (interleaved int32_t)
 * @param out_up_audio  Output upstream audio (interleaved int32_t)
 * @param out_dn_aux    Output downstream aux buffer
 * @param out_up_aux    Output upstream aux buffer
 * @param out_dn_sideband Output downstream sideband byte
 * @param out_up_sideband Output upstream sideband byte
 * @param out_hdr       Output frame header
 * @return 0 on success, -1 on CRC error, -2 on sync error, -3 on slotmap mismatch
 */
int abus_frame_unpack(const abus_slotmap_t *slotmap,
                      const uint8_t *frame, uint16_t frame_len,
                      int32_t *out_dn_audio,
                      int32_t *out_up_audio,
                      uint8_t *out_dn_aux, uint16_t *out_dn_aux_len,
                      uint8_t *out_up_aux, uint16_t *out_up_aux_len,
                      uint8_t *out_dn_sideband,
                      uint8_t *out_up_sideband,
                      abus_frame_header_t *out_hdr) {
    if (frame_len < ABUS_OVERHEAD) return -2;

    /* Check sync pattern */
    if (frame[0] != K28_5 || frame[1] != K28_1) {
        return -2;
    }

    /* Verify CRC */
    uint32_t expected_crc = ((uint32_t)frame[frame_len - 4] << 24) |
                            ((uint32_t)frame[frame_len - 3] << 16) |
                            ((uint32_t)frame[frame_len - 2] <<  8) |
                            ((uint32_t)frame[frame_len - 1]);
    uint32_t computed_crc = abus_crc32(frame, frame_len - ABUS_CRC_LEN);
    if (expected_crc != computed_crc) {
        return -1;
    }

    /* Parse header */
    const abus_frame_header_t *hdr = (const abus_frame_header_t *)&frame[ABUS_HEADER_OFFSET];
    if (out_hdr) *out_hdr = *hdr;

    /* Verify slot map CRC matches */
    if (hdr->slotmap_crc != slotmap->slotmap_crc) {
        return -3;
    }

    /* Extract sideband bytes */
    if (out_dn_sideband) *out_dn_sideband = frame[slotmap->dn_sideband_offset];
    if (out_up_sideband) *out_up_sideband = frame[slotmap->up_sideband_offset];

    /* Unpack audio from slot positions */
    uint8_t dn_ch = 0, up_ch = 0;
    for (int i = 0; i < slotmap->num_audio_slots; i++) {
        const abus_audio_slot_t *slot = &slotmap->audio_slots[i];
        uint16_t off = slot->byte_offset;
        int32_t sample = 0;

        /* FIX #11: Bounds check slot offset against frame length */
        if (off + slot->byte_width > frame_len) return -3;

        /* Unpack big-endian, sign-extend to 32-bit */
        switch (slot->byte_width) {
            case 4:
                sample = ((int32_t)frame[off + 0] << 24) |
                         ((int32_t)frame[off + 1] << 16) |
                         ((int32_t)frame[off + 2] <<  8) |
                         ((int32_t)frame[off + 3]);
                break;
            case 3:
                sample = ((int32_t)(int8_t)frame[off + 0] << 24) |
                         ((int32_t)frame[off + 1] << 16) |
                         ((int32_t)frame[off + 2] <<  8);
                break;
            case 2:
                sample = ((int32_t)(int8_t)frame[off + 0] << 24) |
                         ((int32_t)frame[off + 1] << 16);
                break;
        }

        if (slot->direction == ABUS_DIR_DOWNSTREAM) {
            if (out_dn_audio) out_dn_audio[dn_ch] = sample;
            dn_ch++;
        } else {
            if (out_up_audio) out_up_audio[up_ch] = sample;
            up_ch++;
        }
    }

    /* Copy aux regions */
    if (out_dn_aux && slotmap->dn_aux_bytes > 0) {
        memcpy(out_dn_aux, &frame[slotmap->dn_aux_offset], slotmap->dn_aux_bytes);
        if (out_dn_aux_len) *out_dn_aux_len = slotmap->dn_aux_bytes;
    }
    if (out_up_aux && slotmap->up_aux_bytes > 0) {
        memcpy(out_up_aux, &frame[slotmap->up_aux_offset], slotmap->up_aux_bytes);
        if (out_up_aux_len) *out_up_aux_len = slotmap->up_aux_bytes;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Helper: compute maximum channels for a given configuration
 * --------------------------------------------------------------------------- */

int abus_max_channels_per_direction(abus_sample_rate_t sr, abus_bit_depth_t depth,
                                    uint16_t tunnel_bytes_per_direction) {
    uint16_t frame_bytes;
    if (sr == ABUS_SR_48000 || sr == ABUS_SR_44100) {
        frame_bytes = ABUS_FRAME_BYTES_48K;
    } else {
        frame_bytes = ABUS_FRAME_BYTES_96K;
    }

    uint16_t payload = frame_bytes - ABUS_OVERHEAD;

    /* Each direction gets: sideband(1) + audio + aux */
    /* Both directions share the payload, plus guard(2) between them */
    /* payload = dn_sideband(1) + dn_audio + dn_aux + guard(2) +
     *           up_sideband(1) + up_audio + up_aux */

    uint16_t fixed_per_dir = ABUS_AUX_SIDEBAND_BYTES + tunnel_bytes_per_direction;
    uint16_t available = payload - 2 * fixed_per_dir - ABUS_GUARD_LEN;

    /* Split available evenly between downstream and upstream audio */
    uint16_t per_dir = available / 2;

    uint8_t bps;
    switch (depth) {
        case ABUS_DEPTH_16: bps = 2; break;
        case ABUS_DEPTH_24: bps = 3; break;
        case ABUS_DEPTH_32: bps = 4; break;
        default: return 0;
    }

    int max_ch = per_dir / bps;
    return (max_ch > ABUS_MAX_CHANNELS) ? ABUS_MAX_CHANNELS : max_ch;
}
