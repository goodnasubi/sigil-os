# P6-3 検証記録

- 実施日: 2026-11-24 JST
- 実行環境: ローカル agy（Ubuntu 24.04 x86-64 / Antigravity CLI / Gemini 3.8 Flash Medium）
- 依存する Sigil の基準 SHA: `75d0d3ae6120f1eed2d3f36de30fbd28ed16051e`
- 対象仮想機器構成（各 1 構成に固定）:
  - 準仮想化ストレージ: VMware Paravirtual SCSI (PVSCSI) コントローラー（PCI Vendor ID `0x15AD`, Device ID `0x07C0`, Revision `0x00`）
  - 準仮想化ネットワーク: VMware VMXNET3 Virtual Ethernet Adapter（PCI Vendor ID `0x15AD`, Device ID `0x07B0`, Version 1.0）
- ハイパーバイザー・ファームウェア構成と受入環境の区分:
  - 補助自動試験環境（本検証で自動確認）:
    - QEMU 8.2.2（x86-64 Q35, TCG, 256 MiB RAM, EDK II OVMF 2024.02-2, COM1 シリアル出力, Secure Boot 無効）
    - ストレージ: `-device pvscsi,id=scsi0 -device scsi-hd,bus=scsi0.0,drive=drive0 -drive file=build/p6-3-testdisk.raw,if=none,id=drive0,format=raw`（10 MiB raw ディスク）
    - ネットワーク: `-netdev user,id=net0 -device vmxnet3,netdev=net0`（QEMU Slirp ユーザーモード NAT, ゲートウェイ `10.0.2.2`）
  - ESXi 受入環境構成（所有者の受入確認に委ねる）:
    - VMware ESXi 8.0U3e（所有者の MacBook Pro 上の VMware Fusion Pro 13.5.2 内ネスト構成、UEFI x86-64 VM, Secure Boot 無効, COM1 シリアル出力）
    - ストレージ: VMware 準仮想化 SCSI コントローラ (PVSCSI) および仮想ディスク (VMDK)
    - ネットワーク: VMXNET3 仮想ネットワークアダプタ
  - 環境区分の明記:
    - 本開発 VM には ESXi が存在しないため、P6-2 と同様に QEMU/OVMF を補助自動試験環境として用いて全自動試験（`make check-p6-3` / `make check`）を実施した。
    - QEMU 上で確認したこと: PVSCSI コントローラの初期化、既知データ読み書き、境界外・無効要求の拒否、VMXNET3 アダプタの初期化、QEMU Slirp ゲートウェイ（10.0.2.2）との ARP 要求・応答パケット送受信、境界外・無効パケット要求の拒否、未提供・未初期化機器操作の拒否。
    - ESXi 上で未確認のこと: 実 ESXi 8.0U3e ハイパーバイザー上での実機 VM 動作観測。これは所有者の受入確認に委ねる。

## 完了条件と検証結果一覧（ADR 0054 決定 7）

| 完了条件 | 検証手順・観測結果 | 判定 |
|---|---|---|
| ESXi のサポート対象の仮想機器と版を各 1 構成に固定する | 準仮想化ストレージを VMware PVSCSI（15ad:07c0 rev 0x00）、準仮想化ネットワークを VMware VMXNET3（15ad:07b0 v1.0）の各 1 構成に固定し、PCI 構成空間の走査・BAR 読み取り・Bus Master/Memory Space 制御を実装 (`pci init: OK`) | 合格 |
| 準仮想化ストレージの試験用ディスクで既知データの読み書きを再現する | PVSCSI コントローラのリセット・リング設定を行い、SCSI READ(10)/WRITE(10) コマンドにより LBA 16 へ 512 バイトの既知マジック文字列を含むデータを書き込み、読み戻して全バイトの一致を確認 (`pvscsi init: OK` → `pvscsi test read/write: OK`) | 合格 |
| 準仮想化ネットワークで試験相手との送受信を再現する | VMXNET3 アダプタの MAC アドレス取得、共有メモリ・Tx/Rx ディスクリプタリング設定、デバイス有効化を行い、試験相手（QEMU Slirp ゲートウェイ 10.0.2.2）への ARP 要求パケット（60 バイト）を送信後、返信される ARP 応答パケットを受信し、EtherType (0x0806)、Opcode (Reply: 2)、送信元 IP (10.0.2.2) の一致を確認 (`vmxnet3 init: OK` → `vmxnet3 test send/receive: OK`) | 合格 |
| 範囲外の要求や機器未提供時は拒否を確認する | 1. PVSCSI: 範囲外 LBA (0xFFFFFFFF)、セクタ数 0、セクタ数過大 (300)、NULL バッファの要求が正しく拒否されることを確認 (`pvscsi test oob rejected: OK`)<br>2. VMXNET3: MTU 超過パケット長 (2000)、パケット長 0、NULL バッファでの送信・受信要求が正しく拒否されることを確認 (`vmxnet3 test oob rejected: OK`)<br>3. 機器未提供: 未初期化状態の構造体に対する操作、および PCI バス上に存在しない機器 (Vendor 0xFFFF/0x1234) の探索が拒否されることを確認 (`device unprovided test: OK`) | 合格 |

## 試験の実行コマンドと観測ログ

### 1. P6-3 最小機器試験の単体実行

```sh
tests/check-p6-3.sh build
```

観測ログ:
```
qemu-system-x86_64: terminating on signal 15 from pid 15600 (timeout)
p6-3 kernel run (status 124):
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
pci init: OK
pvscsi init: OK
pvscsi test read/write: OK
pvscsi test oob rejected: OK
vmxnet3 init: OK
vmxnet3 test send/receive: OK
vmxnet3 test oob rejected: OK
device unprovided test: OK
p6-3 minimal devices: ALL CHECKS PASSED
p6-3 kernel done
p6-3 minimal devices VM boot and execution test OK
```

### 2. 全体自動検査 (`make check`)

```sh
make check
```

観測結果:
- `tests/check-boot.sh build`: P6-1 ローダー正常系（`probe.iso`）および異常系（`bad-elf.iso`, `bad-info.iso`）拒否の全項目合格
- `tests/check-kernel.sh build/kernel.elf`: カーネル静的 ELF 検査合格（動的リンク・未解決シンボルなし、ホストシンボルなし）
- `tests/check-sigil.sh`: Sigil コンパイラ検査合格（通常モジュールの原始操作拒否、未承認 trust base 拒否、`sigil spec` での前提表示）
- `tests/check-p6-2.sh build`: P6-2 最小カーネル VM 起動試験合格（後退なし）
- `tests/check-p6-3.sh build`: P6-3 最小機器 VM 起動試験合格（PVSCSI 既知データ読み書き・境界外拒否、VMXNET3 パケット送受信・境界外拒否、機器未提供拒否）
- 全試験終了コード 0
