# P6-1 検証記録

実施日 2026-11-21 JST。実行環境 Codex Cloud（Debian 13 x86-64）。
Sigil の起草基準は `a283bf54607d96e4c1aa27755a2cf2c9ea7a8ce8`。
今回のローダーと試験用 ELF のビルドは Sigil に依存しない。

## 補助試験の構成と手順

QEMU 10.0.13（Debian `1:10.0.13+ds-0+deb13u1`）、x86-64 Q35、
TCG、256 MiB、仮想 CD/DVD、COM1 のファイル出力、ネットワークなし。
EDK II OVMF `2025.02-8+deb13u1` の `OVMF_CODE_4M.fd` と
各回で新しい `OVMF_VARS_4M.fd`、Secure Boot 無効。GCC 14.2.0、
GNU binutils 2.44、GNU-EFI `3.0.18-1+deb13u1`、mtools 4.0.48、
xorriso 1.5.6。

```sh
make clean
make check-boot
readelf -hlWd build/probe.elf
nm -u build/probe.elf
nm build/probe.elf
objdump -d --no-show-raw-insn build/probe.elf
```

Codex Cloud では Debian パッケージを作業ディレクトリに展開し、
`EFI_INCLUDE`、`EFI_LIB`、`OVMF_CODE`、`OVMF_VARS`、`QEMU_DATA`
を展開先へ向けて実行した。起動コマンドとシリアル出力の照合は
[`tests/check-boot.sh`](../tests/check-boot.sh) に保存してある。
`make images` で再生成できる成果物は `build/loader.efi`、
`build/probe.elf` と以下の ISO 3 種。

| ISO | 入力 | シリアルで観測した順序 |
| --- | --- | --- |
| `probe.iso` | C 製の確保不要な試験用 ELF | `ExitBootServices OK` → `kernel entry` → `boot-info version=1` → `boot-info check=OK` |
| `bad-elf.iso` | PE/COFF を `kernel.elf` として格納 | `STOP unsupported ELF` |
| `bad-info.iso` | ローダーの試験ビルドが起動情報に版 2 を設定 | `ExitBootServices OK` → `kernel entry` → `STOP boot-info version` |

3 ケースとも最後の行以降に試験マーカーを出さず、12 秒の
タイムアウトまで終了コード 124 で観測した。正常系の
`boot-info check=OK` は初期化済みデータと BSS の検査も含む。

`readelf` は `EXEC`、x86-64、入口 `0x2000080`、2 個の
`LOAD`（R E と RW）、`There is no dynamic section in this file.`
を表示した。`INTERP` と `DYNAMIC` はなく、
`nm -u build/probe.elf` の出力は空。`nm` の全シンボルは
`boot_stop`、`cleared`、`initialized`、`kernel_entry`、
`serial_write`。確保器の参照やホストシステムコール命令の
検査に一致はなく、`probe ELF checks OK` を出力した。
試験用 ELF は `build/probe.o`の単一オブジェクトと
`tests/probe.ld` から `ld -static --no-undefined` でリンクし、
ホストの CRT、ライブラリや外部ランタイムをリンクしない。
UEFI 側では GNU-EFI の CRT とライブラリを使用する。

## P6-1 の受入状況

| 完了条件 | Sigil 成果物での状況 |
| --- | --- |
| 確保不要の Sigil プログラムから静的 ELF を出力し UEFI で起動 | P6 用出力プロファイルが必要 |
| 動的リンカと未解決の外部参照がない | P6 用出力の生成後に `readelf -hlWd` と `nm -u` で確認 |
| ビルド依存と生成コードにホスト ABI がない | 基準版の Linux 起動コードが阻害。P6 用のビルド依存と逆アセンブリを再検査 |
| 生成コードと同梱ランタイムに確保器参照がない | 基準版の暗黙の確保器結合が阻害。P6 用のリンク入力とシンボルを再検査 |
| ESXi VM で Boot Services 終了後の入口、版、検査結果を観測 | 未実施。利用可能な ESXi VM と接続情報を確認できず、ESXi 版とファームウェア構成は未取得 |
| 対象外形式・不正な起動情報で停止 | QEMU/OVMF の試験用入力で確認済み。ESXi と Sigil 成果物での検証は未実施 |

## Sigil 側で必要な別変更

基準 SHA のソースを確認した。`native::build_program` は Linux x86-64
に限定され、`codegen::compile` で `sigil_syscall6` と既存の
`_start` を生成する。`start_stack` では Linux の `mmap` と
`mprotect` を使い、失敗時は Linux の終了システムコールを呼ぶ。
基準版の回帰試験は、OS 層を使わない関数でも `std.os.alloc` が
暗黙に結合されることを確認している。今回の依存調査は基準版の
ソース確認で、基準版の CLI ビルド・ELF 生成は未実施。
[`native.rs`](https://github.com/goodnasubi/sigil/blob/a283bf54607d96e4c1aa27755a2cf2c9ea7a8ce8/src/native.rs#L54)
[`codegen.rs`](https://github.com/goodnasubi/sigil/blob/a283bf54607d96e4c1aa27755a2cf2c9ea7a8ce8/src/codegen.rs#L802)
[`start.rs`](https://github.com/goodnasubi/sigil/blob/a283bf54607d96e4c1aa27755a2cf2c9ea7a8ce8/src/codegen/func/start.rs#L107)
[`m5_native.rs`](https://github.com/goodnasubi/sigil/blob/a283bf54607d96e4c1aa27755a2cf2c9ea7a8ce8/tests/m5_native.rs#L80)

Sigil 側の変更では、`targets bare` の機能制約と独立に選択できる
x86-64 freestanding 出力を定義し、起動時に受け取る ABI と
静的リンク契約を検証する。確保不要な標準ライブラリの依存閉包から
確保器と `std.os` を外し、必要な最小ランタイムについても同じ検査を
行う。確保や OS 機能を用いる入力の拒否試験と、既存 Linux 出力の
回帰試験を追加する。その変更を取り込んだ完全 SHA に依存を更新し、
[ESXi の手順](boot-contract.md#esxi-での試験手順) で受入結果を保存する。
