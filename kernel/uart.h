#ifndef SIGIL_OS_KERNEL_UART_H
#define SIGIL_OS_KERNEL_UART_H

#include <stdint.h>

/* 16550 互換 UART のレジスタ（ベースからのオフセット）。 */
#define UART_COM1_BASE 0x3f8

/*
 * ベース `base` の UART で、モデム制御レジスタのループバックを使って 1 バイトを
 * 送り、同じ UART の受信側から読み戻す。ループバック中は外部の信号線から切り離される
 * ため、COM1 をコンソールに使っていても出力は乱れない。呼び出し前後で
 * 割り込み許可・回線制御・モデム制御レジスタを元に戻す。
 * 成功で 0。`rx` が NULL、送受信の待ちの上限超過、回線エラーは -1。
 */
int uart16550_loopback_byte(uint16_t base, uint8_t tx, uint8_t *rx);

/* 受信済みのバイトを 1 つ読む。受信データが無ければ -1（待たない）。`rx` が NULL も -1。 */
int uart16550_read_nowait(uint16_t base, uint8_t *rx);

/* FIFO が有効（IIR の上位 2 ビットが 11）なら 1、そうでなければ 0。 */
int uart16550_fifo_enabled(uint16_t base);

#endif
