#ifndef SIGIL_OS_KERNEL_BOARD_TEST_H
#define SIGIL_OS_KERNEL_BOARD_TEST_H

/*
 * P6-4: 組み込み実機でも同じカーネル境界の上で観測する項目（メモリ初期化・割り込み・
 * 機器 1 種（COM1 の 16550 互換 UART）への入出力）を報告する。行の先頭は `p6-4 `。
 * 失敗時は `STOP ...` を出して停止する。
 */
void run_p6_4_board_tests(void);

#endif
