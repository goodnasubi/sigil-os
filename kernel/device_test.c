#include "device_test.h"
#include "pci.h"
#include "pvscsi.h"
#include "vmxnet3.h"
#include "serial.h"
#include "alloc.h"

int test_devices_present(void)
{
    struct pci_device dev;
    if (pci_find_device(PCI_VENDOR_VMWARE, PCI_DEVICE_PVSCSI, &dev) == 0 &&
        pci_find_device(PCI_VENDOR_VMWARE, PCI_DEVICE_VMXNET3, &dev) == 0) {
        return 1;
    }
    return 0;
}

static void test_pvscsi(void)
{
    struct pvscsi_dev dev;
    if (pvscsi_init(&dev) != 0)
        boot_stop("STOP pvscsi init failed\r\n");
    serial_write("pvscsi init: OK\r\n");

    /* Allocate 512-byte write and read buffers */
    uint8_t *wbuf = sigil_alloc(512, 16);
    uint8_t *rbuf = sigil_alloc(512, 16);
    if (!wbuf || !rbuf)
        boot_stop("STOP pvscsi buffer alloc\r\n");

    const char *magic = "SIGIL_OS_P6_3_STORAGE_KNOWN_DATA_VERIFICATION_TEST";
    size_t mlen = 0;
    while (magic[mlen]) mlen++;

    for (size_t i = 0; i < 512; ++i) {
        if (i < mlen)
            wbuf[i] = (uint8_t)magic[i];
        else
            wbuf[i] = (uint8_t)(i & 0xFF);
        rbuf[i] = 0;
    }

    uint64_t test_lba = 16;
    if (pvscsi_write_sectors(&dev, test_lba, 1, wbuf) != 0)
        boot_stop("STOP pvscsi write failed\r\n");

    if (pvscsi_read_sectors(&dev, test_lba, 1, rbuf) != 0)
        boot_stop("STOP pvscsi read failed\r\n");

    for (size_t i = 0; i < 512; ++i) {
        if (rbuf[i] != wbuf[i])
            boot_stop("STOP pvscsi data mismatch\r\n");
    }
    serial_write("pvscsi test read/write: OK\r\n");

    /* Bounds and invalid request rejection */
    if (pvscsi_read_sectors(&dev, 0xFFFFFFFFu, 1, rbuf) == 0 ||
        pvscsi_write_sectors(&dev, 0xFFFFFFFFu, 1, wbuf) == 0 ||
        pvscsi_read_sectors(&dev, test_lba, 0, rbuf) == 0 ||
        pvscsi_read_sectors(&dev, test_lba, 1, 0) == 0 ||
        pvscsi_write_sectors(&dev, test_lba, 300, wbuf) == 0) {
        boot_stop("STOP pvscsi invalid request not rejected\r\n");
    }
    serial_write("pvscsi test oob rejected: OK\r\n");

    sigil_free(wbuf, 512, 16);
    sigil_free(rbuf, 512, 16);
}

static void test_vmxnet3(void)
{
    struct vmxnet3_dev dev;
    if (vmxnet3_init(&dev) != 0)
        boot_stop("STOP vmxnet3 init failed\r\n");
    serial_write("vmxnet3 init: OK\r\n");

    /* Craft ARP request packet */
    uint8_t pkt[64];
    for (size_t i = 0; i < sizeof(pkt); ++i) pkt[i] = 0;

    /* Ethernet header */
    /* Dest MAC: Broadcast FF:FF:FF:FF:FF:FF */
    for (int i = 0; i < 6; ++i) pkt[i] = 0xFF;
    /* Source MAC: dev.mac */
    for (int i = 0; i < 6; ++i) pkt[6 + i] = dev.mac[i];
    /* EtherType: ARP (0x0806) */
    pkt[12] = 0x08;
    pkt[13] = 0x06;

    /* ARP Header */
    pkt[14] = 0x00; pkt[15] = 0x01; /* HW type: Ethernet */
    pkt[16] = 0x08; pkt[17] = 0x00; /* Proto: IPv4 */
    pkt[18] = 0x06;                 /* HW size: 6 */
    pkt[19] = 0x04;                 /* Proto size: 4 */
    pkt[20] = 0x00; pkt[21] = 0x01; /* Opcode: Request */

    /* Sender MAC */
    for (int i = 0; i < 6; ++i) pkt[22 + i] = dev.mac[i];
    /* Sender IP: 10.0.2.15 */
    pkt[28] = 10; pkt[29] = 0; pkt[30] = 2; pkt[31] = 15;

    /* Target MAC: 00:00:00:00:00:00 */
    for (int i = 0; i < 6; ++i) pkt[32 + i] = 0x00;
    /* Target IP: 10.0.2.2 (QEMU gateway peer) */
    pkt[38] = 10; pkt[39] = 0; pkt[40] = 2; pkt[41] = 2;

    /* Send and receive ARP packets (> 32 to verify ring wraparound and rx_gen toggle) */
    uint8_t rx_buf[1514];
    uint16_t rx_len = 0;
    for (int iter = 0; iter < 35; ++iter) {
        if (vmxnet3_send_packet(&dev, pkt, 60) != 0)
            boot_stop("STOP vmxnet3 send failed\r\n");

        /* Receive ARP reply with retry & deadline, discarding unrelated broadcast/multicast frames */
        int got_reply = 0;
        for (int retry = 0; retry < 3 && !got_reply; ++retry) {
            if (retry > 0) {
                if (vmxnet3_send_packet(&dev, pkt, 60) != 0)
                    boot_stop("STOP vmxnet3 send failed\r\n");
            }

            for (int rx_attempt = 0; rx_attempt < 100; ++rx_attempt) {
                rx_len = 0;
                if (vmxnet3_receive_packet(&dev, rx_buf, sizeof(rx_buf), &rx_len) != 0)
                    break; /* Rx timeout; retry sending if retries remain */

                /* Validate whether received frame is the expected ARP Reply:
                 * Minimum length: 42 bytes (14 Ethernet + 28 ARP)
                 * EtherType: 0x0806 (ARP)
                 * Opcode: 0x0002 (Reply)
                 * Sender IP: 10.0.2.2
                 * Target IP: 10.0.2.15
                 */
                if (rx_len >= 42 &&
                    rx_buf[12] == 0x08 && rx_buf[13] == 0x06 &&
                    rx_buf[20] == 0x00 && rx_buf[21] == 0x02 &&
                    rx_buf[28] == 10 && rx_buf[29] == 0 && rx_buf[30] == 2 && rx_buf[31] == 2 &&
                    rx_buf[38] == 10 && rx_buf[39] == 0 && rx_buf[40] == 2 && rx_buf[41] == 15) {
                    got_reply = 1;
                    break;
                }

                /* Discard unrelated broadcast/multicast/other frame and continue */
            }
        }

        if (!got_reply)
            boot_stop("STOP vmxnet3 reply not received\r\n");
    }

    serial_write("vmxnet3 test send/receive: OK\r\n");

    /* Bounds and invalid request rejection */
    if (vmxnet3_send_packet(&dev, pkt, 2000) == 0 ||
        vmxnet3_send_packet(&dev, pkt, 0) == 0 ||
        vmxnet3_send_packet(&dev, 0, 60) == 0 ||
        vmxnet3_receive_packet(&dev, 0, sizeof(rx_buf), &rx_len) == 0 ||
        vmxnet3_receive_packet(&dev, rx_buf, 0, &rx_len) == 0) {
        boot_stop("STOP vmxnet3 invalid request not rejected\r\n");
    }
    serial_write("vmxnet3 test oob rejected: OK\r\n");
}

static void test_unprovided(void)
{
    /* 1. Uninitialized device operations rejected */
    struct pvscsi_dev uninit_pvscsi;
    uint8_t *p = (uint8_t *)&uninit_pvscsi;
    for (size_t i = 0; i < sizeof(uninit_pvscsi); ++i) p[i] = 0;

    uint8_t dummy[512];
    if (pvscsi_read_sectors(&uninit_pvscsi, 0, 1, dummy) == 0 ||
        pvscsi_write_sectors(&uninit_pvscsi, 0, 1, dummy) == 0) {
        boot_stop("STOP uninitialized pvscsi not rejected\r\n");
    }

    struct vmxnet3_dev uninit_vmxnet3;
    p = (uint8_t *)&uninit_vmxnet3;
    for (size_t i = 0; i < sizeof(uninit_vmxnet3); ++i) p[i] = 0;

    if (vmxnet3_send_packet(&uninit_vmxnet3, dummy, 60) == 0 ||
        vmxnet3_receive_packet(&uninit_vmxnet3, dummy, sizeof(dummy), 0) == 0) {
        boot_stop("STOP uninitialized vmxnet3 not rejected\r\n");
    }

    /* 2. Device not found on PCI bus rejected */
    struct pci_device dummy_pdev;
    if (pci_find_device(0xFFFF, 0xFFFF, &dummy_pdev) == 0 ||
        pci_find_device(0x1234, 0x5678, &dummy_pdev) == 0) {
        boot_stop("STOP non-existent pci device found\r\n");
    }

    serial_write("device unprovided test: OK\r\n");
}

void run_p6_3_device_tests(void)
{
    serial_write("pci init: OK\r\n");
    test_pvscsi();
    test_vmxnet3();
    test_unprovided();
    serial_write("p6-3 minimal devices: ALL CHECKS PASSED\r\n");
}
