#include "vmxnet3.h"
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

int vmxnet3_init(struct vmxnet3_dev *dev)
{
    if (!dev)
        return -1;

    dev->initialized = 0;
    struct pci_device pdev;
    if (pci_find_device(PCI_VENDOR_VMWARE, PCI_DEVICE_VMXNET3, &pdev) != 0)
        return -1;

    pci_enable_bus_master(&pdev);
    dev->bar0 = (uintptr_t)pdev.bar[0];
    dev->bar1 = (uintptr_t)pdev.bar[1];
    if (!dev->bar0 || !dev->bar1)
        return -1;

    /* Read MAC address from BAR1 */
    uint32_t mac_l = mmio_read32(dev->bar1 + VMXNET3_REG_MACL);
    uint32_t mac_h = mmio_read32(dev->bar1 + VMXNET3_REG_MACH);
    dev->mac[0] = (uint8_t)(mac_l & 0xFF);
    dev->mac[1] = (uint8_t)((mac_l >> 8) & 0xFF);
    dev->mac[2] = (uint8_t)((mac_l >> 16) & 0xFF);
    dev->mac[3] = (uint8_t)((mac_l >> 24) & 0xFF);
    dev->mac[4] = (uint8_t)(mac_h & 0xFF);
    dev->mac[5] = (uint8_t)((mac_h >> 8) & 0xFF);

    /* Version handshake */
    mmio_write32(dev->bar1 + VMXNET3_REG_VRRS, 1);
    if (mmio_read32(dev->bar1 + VMXNET3_REG_VRRS) != 1)
        return -2;
    mmio_write32(dev->bar1 + VMXNET3_REG_UVRS, 1);
    if (mmio_read32(dev->bar1 + VMXNET3_REG_UVRS) != 1)
        return -3;

    /* Allocate shared structs */
    void *shared_ptr = page_alloc();
    void *qdesc_ptr = page_alloc();
    void *rings_ptr = page_alloc(); /* Holds tx_ring, tx_comp, rx_ring, rx_comp */

    if (!shared_ptr || !qdesc_ptr || !rings_ptr)
        return -4;

    uint64_t shared_phys = (uint64_t)(uintptr_t)shared_ptr;
    uint64_t qdesc_phys = (uint64_t)(uintptr_t)qdesc_ptr;
    uint64_t rings_phys = (uint64_t)(uintptr_t)rings_ptr;

    dev->shared = (struct vmxnet3_driver_shared *)shared_ptr;
    dev->tx_qdesc = (struct vmxnet3_tx_queue_desc *)qdesc_ptr;
    dev->rx_qdesc = (struct vmxnet3_rx_queue_desc *)((uintptr_t)qdesc_ptr + 256);

    /* Zero memory */
    uint8_t *p = (uint8_t *)dev->shared;
    for (size_t i = 0; i < 4096; ++i) p[i] = 0;
    p = (uint8_t *)dev->tx_qdesc;
    for (size_t i = 0; i < 4096; ++i) p[i] = 0;
    p = (uint8_t *)(uintptr_t)rings_phys;
    for (size_t i = 0; i < 4096; ++i) p[i] = 0;

    dev->tx_ring = (struct vmxnet3_tx_desc *)(uintptr_t)rings_phys;
    dev->tx_comp_ring = (struct vmxnet3_tx_comp_desc *)((uintptr_t)rings_phys + 512);
    dev->rx_ring = (struct vmxnet3_rx_desc *)((uintptr_t)rings_phys + 1024);
    dev->rx_comp_ring = (struct vmxnet3_rx_comp_desc *)((uintptr_t)rings_phys + 1536);

    /* Allocate rx packet buffers (32 buffers, allocate contiguous pages) */
    void *rx_buf_pages = page_alloc_contiguous(16);
    if (!rx_buf_pages)
        return -5;
    uint64_t rx_buf_phys = (uint64_t)(uintptr_t)rx_buf_pages;
    for (uint32_t i = 0; i < VMXNET3_RING_SIZE; ++i) {
        dev->rx_buffers[i] = (uint8_t *)(uintptr_t)(rx_buf_phys + (uint64_t)i * 2048);
        dev->rx_ring[i].addr = (uint64_t)(uintptr_t)dev->rx_buffers[i];
        dev->rx_ring[i].len = 1514;
        dev->rx_ring[i].btype = 0; /* head */
        dev->rx_ring[i].dtype = 0; /* data */
        dev->rx_ring[i].gen = 1;   /* initial generation */
    }

    /* Configure shared */
    dev->shared->magic = VMXNET3_REV1_MAGIC;
    dev->shared->size = sizeof(struct vmxnet3_driver_shared);

    dev->shared->dev_read.misc.driver_info.version = 0x01000000;
    dev->shared->dev_read.misc.driver_info.gos_bits = 2; /* 64-bit */
    dev->shared->dev_read.misc.driver_info.gos_type = 1;
    dev->shared->dev_read.misc.driver_info.vmxnet3_rev_spt = 1;
    dev->shared->dev_read.misc.driver_info.upt_ver_spt = 1;
    dev->shared->dev_read.misc.queue_desc_pa = qdesc_phys;
    dev->shared->dev_read.misc.queue_desc_len = 512;
    dev->shared->dev_read.misc.mtu = 1500;
    dev->shared->dev_read.misc.max_num_rx_sg = 1;
    dev->shared->dev_read.misc.num_tx_queues = 1;
    dev->shared->dev_read.misc.num_rx_queues = 1;

    dev->shared->dev_read.intr_conf.num_intrs = 1;
    dev->shared->dev_read.intr_conf.auto_mask = 0;

    /* Accept unicast, multicast, broadcast, and promiscuous */
    dev->shared->dev_read.rx_filter_conf.rx_mode = 0x1Fu;

    /* Configure Tx queue */
    dev->tx_qdesc->conf.tx_ring_base_pa = rings_phys;
    dev->tx_qdesc->conf.comp_ring_base_pa = rings_phys + 512;
    dev->tx_qdesc->conf.tx_ring_size = VMXNET3_RING_SIZE;
    dev->tx_qdesc->conf.comp_ring_size = VMXNET3_RING_SIZE;
    dev->tx_qdesc->conf.intr_idx = 0;

    /* Configure Rx queue */
    dev->rx_qdesc->conf.rx_ring_base_pa[0] = rings_phys + 1024;
    dev->rx_qdesc->conf.rx_ring_base_pa[1] = 0;
    dev->rx_qdesc->conf.comp_ring_base_pa = rings_phys + 1536;
    dev->rx_qdesc->conf.rx_ring_size[0] = VMXNET3_RING_SIZE;
    dev->rx_qdesc->conf.rx_ring_size[1] = 0;
    dev->rx_qdesc->conf.comp_ring_size = VMXNET3_RING_SIZE;
    dev->rx_qdesc->conf.intr_idx = 0;

    dev->tx_prod = 0;
    dev->tx_gen = 1;
    dev->tx_comp_idx = 0;
    dev->tx_comp_gen = 1;

    dev->rx_prod = 0;
    dev->rx_gen = 0;
    dev->rx_comp_idx = 0;
    dev->rx_comp_gen = 1;

    /* Tell device shared address */
    mmio_write32(dev->bar1 + VMXNET3_REG_DSAL, (uint32_t)shared_phys);
    mmio_write32(dev->bar1 + VMXNET3_REG_DSAH, (uint32_t)(shared_phys >> 32));

    /* Activate device */
    mmio_write32(dev->bar1 + VMXNET3_REG_CMD, VMXNET3_CMD_ACTIVATE_DEV);
    uint32_t status = mmio_read32(dev->bar1 + VMXNET3_REG_CMD);
    if (status != 0)
        return -6;

    /* Notify Rx producer index to BAR0 */
    mmio_write32(dev->bar0 + VMXNET3_REG_RXPROD, 0);

    dev->initialized = 1;
    return 0;
}

int vmxnet3_send_packet(struct vmxnet3_dev *dev, const void *buf, uint16_t len)
{
    if (!dev || !dev->initialized)
        return -1;
    if (!buf || len == 0 || len > 1514)
        return -2;

    uint32_t idx = dev->tx_prod;
    struct vmxnet3_tx_desc *txd = &dev->tx_ring[idx];

    txd->addr = (uint64_t)(uintptr_t)buf;
    txd->len = len;
    txd->dtype = 0;
    txd->oco = 0;
    txd->ext1 = 0;
    txd->msscof = 0;
    txd->hlen = 0;
    txd->om = 0;
    txd->eop = 1;
    txd->cq = 1;
    txd->ext2 = 0;
    txd->ti = 0;
    txd->tci = 0;
    __asm__ volatile("" : : : "memory");
    txd->gen = dev->tx_gen;

    idx++;
    if (idx >= VMXNET3_RING_SIZE) {
        idx = 0;
        dev->tx_gen ^= 1;
    }
    dev->tx_prod = idx;

    /* Ring Tx doorbell */
    mmio_write32(dev->bar0 + VMXNET3_REG_TXPROD, dev->tx_prod);

    /* Poll Tx completion */
    for (uint32_t timeout = 0; timeout < 10000000; ++timeout) {
        struct vmxnet3_tx_comp_desc *cd = &dev->tx_comp_ring[dev->tx_comp_idx];
        if (cd->gen == dev->tx_comp_gen) {
            dev->tx_comp_idx++;
            if (dev->tx_comp_idx >= VMXNET3_RING_SIZE) {
                dev->tx_comp_idx = 0;
                dev->tx_comp_gen ^= 1;
            }
            return 0;
        }
        __asm__ volatile("pause" : : : "memory");
    }

    return -3; /* Tx timeout */
}

int vmxnet3_receive_packet(struct vmxnet3_dev *dev, void *buf, uint16_t max_len, uint16_t *out_len)
{
    if (!dev || !dev->initialized)
        return -1;
    if (!buf || max_len == 0)
        return -2;

    for (uint32_t timeout = 0; timeout < 50000000; ++timeout) {
        struct vmxnet3_rx_comp_desc *rcd = &dev->rx_comp_ring[dev->rx_comp_idx];
        if (rcd->gen == dev->rx_comp_gen) {
            uint32_t rxd_idx = rcd->rxd_idx;
            uint16_t pkt_len = (uint16_t)rcd->len;

            dev->rx_comp_idx++;
            if (dev->rx_comp_idx >= VMXNET3_RING_SIZE) {
                dev->rx_comp_idx = 0;
                dev->rx_comp_gen ^= 1;
            }

            uint16_t copy_len = pkt_len < max_len ? pkt_len : max_len;
            const uint8_t *src = dev->rx_buffers[rxd_idx];
            uint8_t *dst = (uint8_t *)buf;
            for (uint16_t i = 0; i < copy_len; ++i)
                dst[i] = src[i];

            if (out_len)
                *out_len = pkt_len;

            /* Replenish Rx descriptor */
            uint32_t rx_idx = dev->rx_prod;
            dev->rx_ring[rx_idx].len = 1514;
            dev->rx_ring[rx_idx].btype = 0;
            dev->rx_ring[rx_idx].dtype = 0;
            __asm__ volatile("" : : : "memory");
            dev->rx_ring[rx_idx].gen = dev->rx_gen;

            rx_idx++;
            if (rx_idx >= VMXNET3_RING_SIZE) {
                rx_idx = 0;
                dev->rx_gen ^= 1;
            }
            dev->rx_prod = rx_idx;

            /* Doorbell */
            mmio_write32(dev->bar0 + VMXNET3_REG_RXPROD, dev->rx_prod);

            return 0;
        }
        __asm__ volatile("pause" : : : "memory");
    }

    return -3; /* Rx timeout */
}
