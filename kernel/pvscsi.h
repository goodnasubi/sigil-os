#ifndef SIGIL_OS_KERNEL_PVSCSI_H
#define SIGIL_OS_KERNEL_PVSCSI_H

#include <stdint.h>
#include <stddef.h>

#define PVSCSI_REG_OFFSET_COMMAND          0x0
#define PVSCSI_REG_OFFSET_COMMAND_DATA     0x4
#define PVSCSI_REG_OFFSET_COMMAND_STATUS   0x8
#define PVSCSI_REG_OFFSET_LAST_STS_0       0x100
#define PVSCSI_REG_OFFSET_INTR_STATUS      0x1004
#define PVSCSI_REG_OFFSET_INTR_MASK        0x2010
#define PVSCSI_REG_OFFSET_KICK_NON_RW_IO   0x3014
#define PVSCSI_REG_OFFSET_KICK_RW_IO       0x4018

#define PVSCSI_CMD_ADAPTER_RESET           1
#define PVSCSI_CMD_ISSUE_SCSI              2
#define PVSCSI_CMD_SETUP_RINGS             3

#define PVSCSI_FLAG_CMD_DIR_NONE           (1u << 2)
#define PVSCSI_FLAG_CMD_DIR_TOHOST         (1u << 3)
#define PVSCSI_FLAG_CMD_DIR_TODEVICE       (1u << 4)

#define PVSCSI_SETUP_RINGS_MAX_NUM_PAGES   32
#define PVSCSI_REQ_RING_ENTRIES_LOG2       5
#define PVSCSI_REQ_RING_ENTRIES            (1u << PVSCSI_REQ_RING_ENTRIES_LOG2)
#define PVSCSI_CMP_RING_ENTRIES_LOG2       7
#define PVSCSI_CMP_RING_ENTRIES            (1u << PVSCSI_CMP_RING_ENTRIES_LOG2)

struct pvscsi_cmd_desc_setup_rings {
    uint32_t req_ring_num_pages;
    uint32_t cmp_ring_num_pages;
    uint64_t rings_state_ppn;
    uint64_t req_ring_ppns[PVSCSI_SETUP_RINGS_MAX_NUM_PAGES];
    uint64_t cmp_ring_ppns[PVSCSI_SETUP_RINGS_MAX_NUM_PAGES];
} __attribute__((packed));

struct pvscsi_rings_state {
    uint32_t req_prod_idx;
    uint32_t req_cons_idx;
    uint32_t req_num_entries_log2;
    uint32_t cmp_prod_idx;
    uint32_t cmp_cons_idx;
    uint32_t cmp_num_entries_log2;
    uint32_t req_call_threshold;
    uint8_t _pad[100];
    uint32_t msg_prod_idx;
    uint32_t msg_cons_idx;
    uint32_t msg_num_entries_log2;
} __attribute__((packed));

struct pvscsi_ring_req_desc {
    uint64_t context;
    uint64_t data_addr;
    uint64_t data_len;
    uint64_t sense_addr;
    uint32_t sense_len;
    uint32_t flags;
    uint8_t cdb[16];
    uint8_t cdb_len;
    uint8_t lun[8];
    uint8_t tag;
    uint8_t bus;
    uint8_t target;
    uint16_t vcpu_hint;
    uint8_t unused[58];
} __attribute__((packed));
_Static_assert(sizeof(struct pvscsi_ring_req_desc) == 128, "pvscsi req desc size");

struct pvscsi_ring_cmp_desc {
    uint64_t context;
    uint64_t data_len;
    uint32_t sense_len;
    uint16_t host_status;
    uint16_t scsi_status;
    uint32_t pad[2];
} __attribute__((packed));
_Static_assert(sizeof(struct pvscsi_ring_cmp_desc) == 32, "pvscsi cmp desc size");

struct pvscsi_dev {
    uintptr_t mmio_base;
    struct pvscsi_rings_state *rings_state;
    struct pvscsi_ring_req_desc *req_ring;
    struct pvscsi_ring_cmp_desc *cmp_ring;
    uint8_t initialized;
};

int pvscsi_init(struct pvscsi_dev *dev);
int pvscsi_read_sectors(struct pvscsi_dev *dev, uint64_t lba, uint32_t count, void *buf);
int pvscsi_write_sectors(struct pvscsi_dev *dev, uint64_t lba, uint32_t count, const void *buf);

#endif
