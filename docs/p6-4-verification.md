# P6-4 検証記録

- 実施日: 2026-11-24 JST（この環境で実施した範囲）
- 実行環境: Claude Code（クラウド、Ubuntu 24.04 x86-64、QEMU 8.2.2、EDK II OVMF 2024.02-2）
- 依存する Sigil の基準 SHA: `75d0d3ae6120f1eed2d3f36de30fbd28ed16051e`（Sigil main の `HEAD` を確認。P6-3 から変化なし）
- 対象とする実機の構成: [p6-4-board.md](p6-4-board.md)（要件として固定。**機種は未決**）

## 結論（先に）

| ADR 0054 決定 7 の P6-4 の完了条件 | 状況 |
|---|---|
| x86-64 の組み込み実機を 1 構成選び、機種、起動設定、必要な機器を固定する | **一部のみ**。起動設定（UEFI・USB 起動・COM1）と必要な機器（COM1 の 16550 互換 UART）は要件として固定した。**機種は未決**で、選定の提案までにとどめた（購入は所有者の判断） |
| 実機のファームウェアから起動し、共有のカーネル境界を通じてメモリ初期化・割り込み・機器 1 種への入出力を観測する | 実機では **未実施**（実機がない）。カーネルの観測項目と試験は実装し、QEMU/OVMF の USB 起動で補助的に確認した |
| 実機での起動と試験結果を記録する | 実機の記録は **未実施**。実機での手順と記入欄を下に置いた |

このため、**P6-4 の完了条件は満たしていない**。実機の機種が決まり、実機で下の手順を実施するまで残る。

## この環境で確認したこと（補助自動試験環境。実機の代わりではない）

補助自動試験環境: QEMU 8.2.2（x86-64 Q35, TCG, 256 MiB RAM, EDK II OVMF 2024.02-2, COM1 シリアル出力, Secure Boot 無効）。
USB 接続のストレージ（`-device qemu-xhci -device usb-storage`）から `build/sigil-os-usb.img` を起動する。
VMware の機器（PVSCSI・VMXNET3）は付けない。

### 追加した観測項目（共有のカーネル境界の上）

`kernel/board_test.c` が、P6-2 の既存の初期化（ページ管理・割り込み）の後に、次を観測して `p6-4 ` で始まる行を出す。
P6-1〜P6-3 の起動契約・既存の試験の出力は変えていない（既存の試験は `p6-4 ` の行を読み飛ばす）。

| 観測 | 内容 | 出力 |
|---|---|---|
| メモリ初期化 | ページ管理が管理するページ数が 0 でなく、空き＋確保済み＝管理数である | `p6-4 memory init: OK managed_pages=N free_pages=N` |
| 割り込み | P6-2 のテストベクトル `0x20` の割り込みが 1 回以上受信されている | `p6-4 interrupt: OK vector=0x20 count=N` |
| 機器 1 種（COM1 の UART）への入出力 | モデム制御レジスタのループバックで、8 バイト（`00 55 aa ff` と ASCII の `SIGL`）を送り、同じ UART から全バイトを読み戻して一致を確認する。FIFO の有無を報告する | `p6-4 uart loopback: OK com1=0x3f8 bytes=8 fifo=on` |
| 不正な要求の拒否 | 受信先が NULL の送受信、受信データが無いときの読み出しを拒否する | `p6-4 uart rejected: OK` |

ループバックは外部の信号線から切り離されるため、実機でもコンソールとして使っている COM1 の出力を乱さない
（実行前後で IER・MCR を元に戻す）。切り替える前に、直前のコンソール出力の送信が完全に終わる（LSR の TEMT）のを
上限つきで待つ。待ちの上限を超えた場合は失敗として扱う。この UART の観測は PCI や VMware の機器に依存しない。

### 実行結果

```sh
make usb                      # build/sigil-os-usb.img を作る（boot/make-usb.sh、GPT 付き）
tests/check-p6-4.sh build     # USB 起動の QEMU 試験
```

観測ログ（`tests/check-p6-4.sh`、QEMU は時間切れで停止するため status 124 が正）:

```
p6-4 board run (status 124):
ExitBootServices OK
kernel entry
boot-info version=1
boot-info check=OK
page manager init: OK
page test reserved excluded=OK
page test oom handled=OK
interrupt init: OK
interrupt test received=OK vector=0x20 count=1
allocator init: OK
allocator core list test=OK
allocator core string test=OK
allocator oom handled=OK
p6-2 minimal kernel: ALL CHECKS PASSED
p6-4 memory init: OK managed_pages=8192 free_pages=8186
p6-4 interrupt: OK vector=0x20 count=1
p6-4 uart loopback: OK com1=0x3f8 bytes=8 fifo=on
p6-4 uart rejected: OK
p6-4 board observation: ALL CHECKS PASSED
p6-2 kernel done
p6-4 board (USB boot) QEMU boot and observation test OK
```

### 後退の確認（この環境で実施）

| 試験 | 結果 |
|---|---|
| `tests/check-p6-2.sh build` | 合格（`p6-2 minimal kernel VM boot and execution test OK`） |
| `tests/check-p6-3.sh build` | 合格（`p6-3 minimal devices VM boot and execution test OK`） |
| `make check-boot` | 合格（終了コード 0） |
| `make check-kernel` | 合格（`kernel ELF checks OK`、終了コード 0） |
| `tests/check-sigil.sh` | **未実施**。この環境に Sigil のバイナリがなく（`/home/hatch/workspace/sigil/target/release/sigil` が無い）、実行できなかった。変更は Sigil のソースに触れていない |

### この環境で確認していないこと

- **実機での起動、および実機上の観測のすべて**（メモリ初期化・割り込み・UART の入出力）。QEMU の結果は実機の結果ではない。
- 実機の UEFI が `\EFI\BOOT\BOOTX64.EFI` から USB 起動するか、実機の COM1 が I/O ポート `0x3f8` の 16550 互換か。
- 実機での**ハードウェア割り込み**（PIC・IOAPIC 経由）。割り込みの観測は P6-2 と同じソフトウェア割り込み（`int $0x20`）のみ。
- 実機のメモリマップに対するページ管理（QEMU のマップとは異なる）。

## 実機での手順（機種の決定後に実施する）

1. 機種の決定後、実機の UEFI 設定で Secure Boot を無効にし、COM1 を I/O ポート `0x3f8`・115200 baud・8N1 にする（シリアルのリダイレクトは無効にする）。
2. `make usb` を実行し、`dd if=build/sigil-os-usb.img of=/dev/sdX bs=4M conv=fsync` で USB メモリに書く（`/dev/sdX` は書き込み先を確かめてから指定する）。
3. 実機の COM1 を USB-シリアル変換でホストに接続し、ホスト側でシリアルを記録しながら、実機を USB から起動する。
4. 記録したシリアルが、上の観測ログの `ExitBootServices OK` から `p6-2 kernel done` までと一致することを確かめる（`managed_pages` と `free_pages` は実機の値でよい。ゼロでないこと）。
5. 次を下の表に記入し、シリアルの記録を添える。

| 記入欄 | 値 |
|---|---|
| 機種（型番・ファームウェアの版） | 未記入 |
| 起動設定（Secure Boot・起動順序・COM1 の設定） | 未記入 |
| 接続した機器（USB メモリ・USB-シリアル変換など） | 未記入 |
| `kernel entry` への到達 | 未記入 |
| `p6-4 memory init` | 未記入 |
| `p6-4 interrupt` | 未記入 |
| `p6-4 uart loopback` / `p6-4 uart rejected` | 未記入 |
