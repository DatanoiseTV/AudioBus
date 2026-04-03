/*
 * AudioBus — Node discovery and hot-plug protocol
 *
 * Discovery sequence (master-initiated):
 *   1. Master sends DISCOVERY beacon with target_node_id = 1
 *   2. Nearest undiscovered node (upstream port CDR locked, no address assigned)
 *      receives the beacon and responds with its node descriptor
 *   3. Master assigns address, node enables its downstream port
 *   4. Master increments target_node_id and repeats until no response (timeout)
 *
 * Hot-plug detection:
 *   - Each node monitors DS92LV1212A LOCK pin on both ports
 *   - Loss of lock on downstream port → downstream node unplugged
 *   - Master polls link status periodically; on change, re-discovers from that point
 *   - New node plugged in → lock acquired on upstream port → node sends presence beacon
 *
 * Configuration distribution:
 *   - After discovery, master computes slot map and sends CONFIG frame to all nodes
 *   - Each node ACKs with a STATUS frame
 *   - On all ACKs received, master transitions to RUNNING state
 *
 * SPDX-License-Identifier: MIT
 */

#include "audiobus_types.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Discovery beacon frame format (fits in frame_type = ABUS_FRAME_DISCOVERY)
 *
 * Payload after standard frame header:
 *   [0]     sub_type: 0x01=beacon, 0x02=response, 0x03=assign
 *   [1]     target_node_id (for beacon) or assigned_node_id (for assign)
 *   [2..N]  node_descriptor (for response, serialized)
 * --------------------------------------------------------------------------- */

#define DISC_SUBTYPE_BEACON     0x01
#define DISC_SUBTYPE_RESPONSE   0x02
#define DISC_SUBTYPE_ASSIGN     0x03
#define DISC_SUBTYPE_CONFIG     0x04
#define DISC_SUBTYPE_CONFIG_ACK 0x05

int abus_discovery_pack_beacon(uint8_t *buf, int buf_len, uint8_t target_node_id) {
    if (buf_len < 2) return -1;
    buf[0] = DISC_SUBTYPE_BEACON;
    buf[1] = target_node_id;
    return 2;
}

int abus_discovery_pack_response(uint8_t *buf, int buf_len,
                                 const abus_node_descriptor_t *desc) {
    int need = 2 + sizeof(abus_node_descriptor_t);
    if (buf_len < need) return -1;
    buf[0] = DISC_SUBTYPE_RESPONSE;
    buf[1] = 0;  /* Not yet assigned */
    memcpy(&buf[2], desc, sizeof(abus_node_descriptor_t));
    return need;
}

int abus_discovery_unpack(const uint8_t *buf, int len,
                          abus_node_descriptor_t *out_desc) {
    if (len < 2) return -1;

    uint8_t sub_type = buf[0];

    if (sub_type == DISC_SUBTYPE_RESPONSE) {
        if (len < (int)(2 + sizeof(abus_node_descriptor_t))) return -1;
        if (out_desc) {
            memcpy(out_desc, &buf[2], sizeof(abus_node_descriptor_t));
        }
        return 0;
    }

    if (sub_type == DISC_SUBTYPE_BEACON) {
        /* Caller can read target_node_id from buf[1] */
        return 0;
    }

    return -1;
}

/* ---------------------------------------------------------------------------
 * Configuration distribution
 *
 * The slot map is serialized into the frame payload.
 * To keep it simple, we transmit the raw slotmap struct.
 * In a production system you'd use a compact binary format.
 * --------------------------------------------------------------------------- */

int abus_discovery_pack_config(uint8_t *buf, int buf_len,
                               const abus_slotmap_t *slotmap) {
    int need = 2 + sizeof(abus_slotmap_t);
    if (buf_len < need) return -1;
    buf[0] = DISC_SUBTYPE_CONFIG;
    buf[1] = 0;
    memcpy(&buf[2], slotmap, sizeof(abus_slotmap_t));
    return need;
}

int abus_discovery_unpack_config(const uint8_t *buf, int len,
                                 abus_slotmap_t *out_slotmap) {
    if (len < (int)(2 + sizeof(abus_slotmap_t))) return -1;
    if (buf[0] != DISC_SUBTYPE_CONFIG) return -1;
    if (out_slotmap) {
        memcpy(out_slotmap, &buf[2], sizeof(abus_slotmap_t));
    }
    return 0;
}
