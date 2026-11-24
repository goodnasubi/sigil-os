# P6-2 検証記録

- 実施日: 2026-11-24 JST
- 実行環境: ローカル agy（Ubuntu 24.04 x86-64 / Antigravity CLI / Gemini 3.8 Flash Medium）
- 依存する Sigil の基準 SHA: `426800cbd7717c2d2c6087e45e8f70f8733c07ce`（PR #743 マージ済み）
- 対象ハイパーバイザー・ファームウェア構成:
  - 補助自動試験: QEMU 8.2.2（x86-64 Q35, TCG, 256 MiB RAM, EDK II OVMF 2024.02-2, COM1 シリアル出力, Secure Boot 無効）
  - ESXi 受入環境構成: VMware ESXi 8.0U3e（MacBook Pro 上の VMware Fusion Pro 13.5.2 ネスト構成、UEFI x86-64 VM, Secure Boot 無効, COM1 シリアル出力）

## 完了条件と検証結果一覧（ADR 0054 決定 7）

| 完了条件 | 検証手順・観測結果 | 判定 |
|---|---|---|
| ESXi VM で起動する | UEFI ローダー（`loader.efi`）から ExitBootServices 正常終了を経て静的 ELF カーネル（`kernel.elf`）の入口へ遷移し、起動情報 v1（40 バイト）を検証 (`ExitBootServices OK` → `kernel entry` → `boot-info version=1` → `boot-info check=OK`) | 合格 |
| 予約領域を除くページ管理を初期化する | UEFI メモリマップから `EFI_CONVENTIONAL_MEMORY (7)` を走査し、低位メモリ `[0x0, 0x100000)`、カーネル窓 `[0x02000000, 0x02200000)`、boot_info / memory_map 領域、非 Conventional 予約領域（ACPI, MMIO 等）を除外して 4KiB ページ枠管理を初期化 (`page manager init: OK`) | 合格 |
| 割り込みを初期化する | 256 エントリの 64-bit IDT を構成、CS セレクタと 0x8E 属性（割り込みゲート）を設定し `lidt` で登録 (`interrupt init: OK`) | 合格 |
| 確保器を起動して Sigil の確保依存の核を使う | ページ管理の上でヒープ確保器（スラブ／サイズクラス 16〜2048B、大ブロックページ確保）を起動し、ADR 0054 決定 3 の確保依存の核（動的 `List`・`String`）の生成・追加・伸長・内容一致・解放を確認。リークなし (`allocator init: OK` → `allocator core list test=OK` → `allocator core string test=OK`) | 合格 |
| 予約領域を配らないことを試験する | ページ確保で配られる全ページが低位領域・カーネル領域・予約領域に重複しないこと、および `page_is_reserved_address` の除外判定を試験 (`page test reserved excluded=OK`) | 合格 |
| 資源不足の扱いを試験する | ページ枯渇時の `page_alloc()` および確保器の最大サイズ／過大要求時の `sigil_alloc()` がクラッシュせず NULL を安全に返却し、呼び出し側で正常に処理されることを試験 (`page test oom handled=OK`, `allocator oom handled=OK`) | 合格 |
| 割り込みの受信を試験する | テストベクトル 0x20 に対するソフトウェア割り込み `int $0x20` を発行し、ISR ディスパッチャが受信してカウンタとベクトル番号を記録し `iretq` で正常復帰することを確認 (`interrupt test received=OK vector=0x20 count=1`) | 合格 |
| 通常モジュールからの原始操作は処理系が拒否することを確認する | `tests/check-sigil.sh` により、`trust none` からの `raw.*` 呼び出し、`trust none` での `unchecked` 宣言、未承認 `trust base` がそれぞれ `sigil check` で拒否されることを確認 | 合格 |
| 許可された前提は `sigil spec` に出ることを確認する | `tests/check-sigil.sh` により、承認済み `trust base` モジュールの `sigil spec` 出力に `検査を外す: 確保器が初期化済みかつページ管理が有効` および `raw.alloc` / `raw.free` の前提文が出力されることを確認 | 合格 |

## 試験の実行コマンドと観測ログ

### 1. ビルドと全自動検査 (`make check`)

```sh
make check
```

- P6-1 ローダー試験:
  - `probe.iso`: `ExitBootServices OK` → `kernel entry` → `boot-info version=1` → `boot-info check=OK`
  - `bad-elf.iso`: `STOP unsupported ELF`
  - `bad-info.iso`: `ExitBootServices OK` → `kernel entry` → `STOP boot-info version`
- P6-2 カーネル ELF 静的検査 (`tests/check-kernel.sh`):
  - EXEC 形式、動的リンク・INTERP・DYNAMIC なし、未解決シンボルなし、ホスト Linux システムコール・ランタイムシンボルなし (`kernel ELF checks OK`)
- Sigil コンパイラ検査 (`tests/check-sigil.sh`):
  - 通常モジュールの原始操作拒否 (`RawOutsideUnchecked`): OK
  - trust none モジュールの unchecked 拒否: OK
  - 未承認 trust base モジュールの拒否: OK
  - 承認済み trust base モジュールの check 成功: OK
  - `sigil spec` への前提および原始操作（`raw.alloc` / `raw.free`）の表示: OK
- P6-2 最小カーネル VM 起動・実行検査 (`tests/check-p6-2.sh`):
  - QEMU/OVMF 上で `sigil-os.iso` を起動し、以下のシリアル出力を完全一致で観測:
    ```
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
    p6-2 kernel done
    ```
