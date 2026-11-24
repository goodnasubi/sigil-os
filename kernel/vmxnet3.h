#ifndef SIGIL_OS_KERNEL_VMXNET3_H
#define SIGIL_OS_KERNEL_VMXNET3_H

#include <stdint.h>
#include <stddef.h>

#define VMXNET3_REG_VRRS     0x0
#define VMXNET3_REG_UVRS     0x8
#define VMXNET3_REG_DSAL     0x10
#define VMXNET3_REG_DSAH     0x18
#define VMXNET3_REG_CMD      0x20
#define VMXNET3_REG_MACL     0x28
#define VMXNET3_REG_MACH     0x30

#define VMXNET3_REG_IMR      0x0
#define VMXNET3_REG_TXPROD   0x600
#define VMXNET3_REG_RXPROD   0x800

#define VMXNET3_REV1_MAGIC        0xbabefee1u
#define VMXNET3_CMD_ACTIVATE_DEV  0xCAFE0000u
#define VMXNET3_RING_SIZE         32u

#define VMXNET3_MAX_INTRS         25

struct vmxnet3_tx_desc {
    uint64_t addr;
    uint32_t len : 14;
    uint32_t gen : 1;
    uint32_t oco : 1;
    uint32_t dtype : 1;
    uint32_t ext1 : 1;
    uint32_t msscof : 14;
    uint32_t hlen : 10;
    uint32_t om : 2;
    uint32_t eop : 1;
    uint32_t cq : 1;
    uint32_t ext2 : 1;
    uint32_t ti : 1;
    uint32_t tci : 16;
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_tx_desc) == 16, "vmxnet3 tx desc size");

struct vmxnet3_tx_comp_desc {
    uint32_t txd_idx : 12;
    uint32_t ext1 : 20;
    uint32_t ext2;
    uint32_t ext3;
    uint32_t rsvd : 24;
    uint32_t type : 7;
    uint32_t gen : 1;
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_tx_comp_desc) == 16, "vmxnet3 tx comp desc size");

struct vmxnet3_rx_desc {
    uint64_t addr;
    uint32_t len : 14;
    uint32_t btype : 1;
    uint32_t dtype : 1;
    uint32_t rsvd : 15;
    uint32_t gen : 1;
    uint32_t ext1;
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_rx_desc) == 16, "vmxnet3 rx desc size");

struct vmxnet3_rx_comp_desc {
    uint32_t rxd_idx : 12;
    uint32_t ext1 : 2;
    uint32_t eop : 1;
    uint32_t sop : 1;
    uint32_t rq_id : 10;
    uint32_t rss_type : 4;
    uint32_t cnc : 1;
    uint32_t ext2 : 1;
    uint32_t rss_hash;
    uint32_t len : 14;
    uint32_t err : 1;
    uint32_t ts : 1;
    uint32_t tci : 16;
    uint32_t csum : 16;
    uint32_t tuc : 1;
    uint32_t udp : 1;
    uint32_t tcp : 1;
    uint32_t ipc : 1;
    uint32_t v6 : 1;
    uint32_t v4 : 1;
    uint32_t frg : 1;
    uint32_t fcs : 1;
    uint32_t type : 7;
    uint32_t gen : 1;
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_rx_comp_desc) == 16, "vmxnet3 rx comp desc size");

struct vmxnet3_tx_queue_ctrl {
    uint32_t tx_num_deferred;
    uint32_t tx_threshold;
    uint64_t reserved;
} __attribute__((packed));

struct vmxnet3_tx_queue_conf {
    uint64_t tx_ring_base_pa;
    uint64_t data_ring_base_pa;
    uint64_t comp_ring_base_pa;
    uint64_t dd_pa;
    uint64_t reserved;
    uint32_t tx_ring_size;
    uint32_t data_ring_size;
    uint32_t comp_ring_size;
    uint32_t dd_len;
    uint8_t intr_idx;
    uint8_t _pad[7];
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_tx_queue_conf) == 64, "tx queue conf size");

struct vmxnet3_tx_queue_desc {
    struct vmxnet3_tx_queue_ctrl ctrl;
    struct vmxnet3_tx_queue_conf conf;
    uint8_t _pad[256 - sizeof(struct vmxnet3_tx_queue_ctrl) - sizeof(struct vmxnet3_tx_queue_conf)];
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_tx_queue_desc) == 256, "tx queue desc size");

struct vmxnet3_rx_queue_ctrl {
    uint8_t update_rx_prod;
    uint8_t _pad[7];
    uint64_t reserved;
} __attribute__((packed));

struct vmxnet3_rx_queue_conf {
    uint64_t rx_ring_base_pa[2];
    uint64_t comp_ring_base_pa;
    uint64_t dd_pa;
    uint64_t reserved;
    uint32_t rx_ring_size[2];
    uint32_t comp_ring_size;
    uint32_t dd_len;
    uint8_t intr_idx;
    uint8_t _pad[7];
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_rx_queue_conf) == 64, "rx queue conf size");

struct vmxnet3_rx_queue_desc {
    struct vmxnet3_rx_queue_ctrl ctrl;
    struct vmxnet3_rx_queue_conf conf;
    uint8_t _pad[256 - sizeof(struct vmxnet3_rx_queue_ctrl) - sizeof(struct vmxnet3_rx_queue_conf)];
} __attribute__((packed));
_Static_assert(sizeof(struct vmxnet3_rx_queue_desc) == 256, "rx queue desc size");

struct vmxnet3_driver_info {
    uint32_t version;
    uint32_t gos_bits : 2;
    uint32_t gos_type : 4;
    uint32_t gos_ver : 16;
    uint32_t gos_misc : 10;
    uint32_t vmxnet3_rev_spt;
    uint32_t upt_ver_spt;
} __attribute__((packed));

struct vmxnet3_misc_conf {
    struct vmxnet3_driver_info driver_info;
    uint64_t upt_features;
    uint64_t dd_pa;
    uint64_t queue_desc_pa;
    uint32_t dd_len;
    uint32_t queue_desc_len;
    uint32_t mtu;
    uint16_t max_num_rx_sg;
    uint8_t num_tx_queues;
    uint8_t num_rx_queues;
    uint32_t reserved[4];
} __attribute__((packed));

struct vmxnet3_intr_conf {
    uint8_t auto_mask;
    uint8_t num_intrs;
    uint8_t event_intr_idx;
    uint8_t mod_levels[VMXNET3_MAX_INTRS];
    uint32_t intr_ctrl;
    uint32_t reserved[2];
} __attribute__((packed));

struct vmxnet3_rx_filter_conf {
    uint32_t rx_mode;
    uint16_t mf_table_len;
    uint16_t _pad1;
    uint64_t mf_table_pa;
    uint32_t vf_table[128];
} __attribute__((packed));

struct vmxnet3_variable_len_conf_desc {
    uint32_t conf_ver;
    uint32_t conf_len;
    uint64_t conf_pa;
} __attribute__((packed));

struct vmxnet3_ds_dev_read {
    struct vmxnet3_misc_conf misc;
    struct vmxnet3_intr_conf intr_conf;
    struct vmxnet3_rx_filter_conf rx_filter_conf;
    struct vmxnet3_variable_len_conf_desc rss_conf_desc;
    struct vmxnet3_variable_len_conf_desc pm_conf_desc;
    struct vmxnet3_variable_len_conf_desc plugin_conf_desc;
} __attribute__((packed));

struct vmxnet3_driver_shared {
    uint32_t magic;
    uint32_t size;
    struct vmxnet3_ds_dev_read dev_read;
    uint32_t ecr;
    uint32_t reserved;
    uint32_t reserved1[4];
} __attribute__((packed));

struct vmxnet3_dev {
    uintptr_t bar0;
    uintptr_t bar1;
    uint8_t mac[6];
    struct vmxnet3_driver_shared *shared;
    struct vmxnet3_tx_queue_desc *tx_qdesc;
    struct vmxnet3_rx_queue_desc *rx_qdesc;
    struct vmxnet3_tx_desc *tx_ring;
    struct vmxnet3_tx_comp_desc *tx_comp_ring;
    struct vmxnet3_rx_desc *rx_ring;
    struct vmxnet3_rx_comp_desc *rx_comp_ring;
    uint8_t *rx_buffers[VMXNET3_RING_SIZE];
    uint32_t tx_prod;
    uint8_t tx_gen;
    uint32_t tx_comp_idx;
    uint8_t tx_comp_gen;
    uint32_t rx_prod;
    uint8_t rx_gen;
    uint32_t rx_comp_idx;
    uint8_t rx_comp_gen;
    uint8_t initialized;
};

int vmxnet3_init(struct vmxnet3_dev *dev);
int vmxnet3_send_packet(struct vmxnet3_dev *dev, const void *buf, uint16_t len);
int vmxnet3_receive_packet(struct vmxnet3_dev *dev, void *buf, uint16_t max_len, uint16_t *out_len);

#endif
