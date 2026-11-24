#include "pvscsi.h"
#include "pci.h"
#include "page.h"

static inline void mmio_write32(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

int pvscsi_init(struct pvscsi_dev *dev)
{
    if (!dev)
        return -1;

    dev->initialized = 0;
    struct pci_device pdev;
    if (pci_find_device(PCI_VENDOR_VMWARE, PCI_DEVICE_PVSCSI, &pdev) != 0)
        return -1;

    pci_enable_bus_master(&pdev);
    dev->mmio_base = (uintptr_t)pdev.bar[0];
    if (!dev->mmio_base)
        return -1;

    /* Allocate 1 page for rings state, 1 page for req ring, 1 page for cmp ring */
    void *state_ptr = page_alloc();
    void *req_ptr = page_alloc();
    void *cmp_ptr = page_alloc();

    if (!state_ptr || !req_ptr || !cmp_ptr)
        return -2;

    uint64_t state_phys = (uint64_t)(uintptr_t)state_ptr;
    uint64_t req_phys = (uint64_t)(uintptr_t)req_ptr;
    uint64_t cmp_phys = (uint64_t)(uintptr_t)cmp_ptr;

    dev->rings_state = (struct pvscsi_rings_state *)state_ptr;
    dev->req_ring = (struct pvscsi_ring_req_desc *)req_ptr;
    dev->cmp_ring = (struct pvscsi_ring_cmp_desc *)cmp_ptr;

    /* Zero pages */
    uint8_t *p = (uint8_t *)dev->rings_state;
    for (size_t i = 0; i < 4096; ++i) p[i] = 0;
    p = (uint8_t *)dev->req_ring;
    for (size_t i = 0; i < 4096; ++i) p[i] = 0;
    p = (uint8_t *)dev->cmp_ring;
    for (size_t i = 0; i < 4096; ++i) p[i] = 0;

    dev->rings_state->req_num_entries_log2 = PVSCSI_REQ_RING_ENTRIES_LOG2;
    dev->rings_state->cmp_num_entries_log2 = PVSCSI_CMP_RING_ENTRIES_LOG2;
    dev->rings_state->req_prod_idx = 0;
    dev->rings_state->req_cons_idx = 0;
    dev->rings_state->cmp_prod_idx = 0;
    dev->rings_state->cmp_cons_idx = 0;

    /* Reset adapter */
    mmio_write32(dev->mmio_base + PVSCSI_REG_OFFSET_COMMAND, PVSCSI_CMD_ADAPTER_RESET);

    /* Setup rings command */
    struct pvscsi_cmd_desc_setup_rings setup;
    uint8_t *s = (uint8_t *)&setup;
    for (size_t i = 0; i < sizeof(setup); ++i) s[i] = 0;
    setup.req_ring_num_pages = 1;
    setup.cmp_ring_num_pages = 1;
    setup.rings_state_ppn = state_phys >> 12;
    setup.req_ring_ppns[0] = req_phys >> 12;
    setup.cmp_ring_ppns[0] = cmp_phys >> 12;

    mmio_write32(dev->mmio_base + PVSCSI_REG_OFFSET_COMMAND, PVSCSI_CMD_SETUP_RINGS);
    for (size_t i = 0; i < sizeof(setup); i += 4) {
        const uint8_t *src = (const uint8_t *)&setup + i;
        uint32_t word = (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
                       ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
        mmio_write32(dev->mmio_base + PVSCSI_REG_OFFSET_COMMAND_DATA, word);
    }

    dev->initialized = 1;
    return 0;
}

static int pvscsi_scsi_rw(struct pvscsi_dev *dev, uint64_t lba, uint32_t count, void *buf, int is_write)
{
    if (!dev || !dev->initialized)
        return -1;
    if (!buf || count == 0 || count > 256)
        return -2;
    /* Bounds checking: test disk limit 128 GiB = 0x10000000 sectors */
    if (lba > 0x10000000 || lba + count > 0x10000000)
        return -3;

    for (int retry = 0; retry < 3; ++retry) {
        uint8_t sense_buf[64];
        for (size_t i = 0; i < sizeof(sense_buf); ++i) sense_buf[i] = 0;

        uint32_t prod = dev->rings_state->req_prod_idx;
        uint32_t mask = PVSCSI_REQ_RING_ENTRIES - 1;
        struct pvscsi_ring_req_desc *req = &dev->req_ring[prod & mask];

        uint8_t *r = (uint8_t *)req;
        for (size_t i = 0; i < sizeof(*req); ++i) r[i] = 0;

        req->context = 0x53435349u; /* 'SCSI' */
        req->data_addr = (uint64_t)(uintptr_t)buf;
        req->data_len = (uint64_t)count * 512;
        req->sense_addr = (uint64_t)(uintptr_t)sense_buf;
        req->sense_len = sizeof(sense_buf);
        req->flags = is_write ? PVSCSI_FLAG_CMD_DIR_TODEVICE : PVSCSI_FLAG_CMD_DIR_TOHOST;

        /* SCSI READ(10) (0x28) / WRITE(10) (0x2A) */
        req->cdb[0] = is_write ? 0x2A : 0x28;
        req->cdb[1] = 0;
        req->cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
        req->cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
        req->cdb[4] = (uint8_t)((lba >> 8) & 0xFF);
        req->cdb[5] = (uint8_t)(lba & 0xFF);
        req->cdb[6] = 0;
        req->cdb[7] = (uint8_t)((count >> 8) & 0xFF);
        req->cdb[8] = (uint8_t)(count & 0xFF);
        req->cdb[9] = 0;
        req->cdb_len = 10;
        req->target = 0;
        req->bus = 0;
        req->lun[1] = 0;

        __asm__ volatile("" : : : "memory");
        dev->rings_state->req_prod_idx = prod + 1;

        /* Kick device */
        mmio_write32(dev->mmio_base + PVSCSI_REG_OFFSET_KICK_RW_IO, 0);

        /* Poll completion */
        int completed = 0;
        for (uint32_t timeout = 0; timeout < 10000000; ++timeout) {
            if (dev->rings_state->cmp_cons_idx != dev->rings_state->cmp_prod_idx) {
                uint32_t cmp_cons = dev->rings_state->cmp_cons_idx;
                struct pvscsi_ring_cmp_desc cmp = dev->cmp_ring[cmp_cons & (PVSCSI_CMP_RING_ENTRIES - 1)];
                dev->rings_state->cmp_cons_idx = cmp_cons + 1;
                if (cmp.host_status != 0 || cmp.scsi_status != 0) {
                    /* Sense Key 0x06 = UNIT ATTENTION (e.g. Bus Reset). Condition cleared, retry. */
                    if ((sense_buf[2] & 0x0F) == 0x06) {
                        completed = 2; /* Retry */
                        break;
                    }
                    return -4;
                }
                return 0;
            }
            __asm__ volatile("pause" : : : "memory");
        }

        if (completed == 2)
            continue;

        return -5; /* Timeout */
    }

    return -4;
}

int pvscsi_read_sectors(struct pvscsi_dev *dev, uint64_t lba, uint32_t count, void *buf)
{
    return pvscsi_scsi_rw(dev, lba, count, buf, 0);
}

int pvscsi_write_sectors(struct pvscsi_dev *dev, uint64_t lba, uint32_t count, const void *buf)
{
    return pvscsi_scsi_rw(dev, lba, count, (void *)(uintptr_t)buf, 1);
}
