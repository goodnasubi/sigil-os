# sigil-os
Sigil P6 の OS 本体・対象別プラットフォーム層・OS のビルド／起動テストを置くリポジトリ（ADR 0054 決定6）

- [sigil-os の違いと優位性（図解）](docs/sigil-os-overview.html)

## P6-1 の起動
- [起動契約と ESXi での試験手順](docs/boot-contract.md)
- [P6-1 検証記録と Sigil 側の依存条件](docs/p6-1-verification.md)

## P6-2 ESXi の最小カーネル（ADR 0054 決定 7）
- [P6-2 検証記録](docs/p6-2-verification.md)

## P6-3 ESXi の最小機器（ADR 0054 決定 7）
- [P6-3 検証記録](docs/p6-3-verification.md)

x86-64 の GNU ツールチェーン、GNU-EFI、mtools、xorriso を使う。Debian 13 / Ubuntu 24.04
では `build-essential binutils gnu-efi mtools xorriso qemu-system-x86 ovmf` が必要。

```sh
make                          # build/loader.efi, build/kernel.elf, build/sigil-os.iso
make check                    # 全試験（check-boot, check-kernel, check-p6-2, check-p6-3, check-sigil）
make check-p6-3               # P6-3 最小機器 VM 起動試験（PVSCSI・VMXNET3）
make check-p6-2               # P6-2 最小カーネル VM 起動試験
make check-sigil              # Sigil コンパイラ検査（原始操作拒否・sigil spec 前提表示）
make check-kernel             # カーネル静的 ELF 検査
make check-boot               # P6-1 ローダー試験
make iso KERNEL_ELF=/path/to/kernel.elf
```

`make iso` は起動契約に従う ELF を `build/sigil-os.iso` に格納する。
依存する Sigil 基準 SHA は `75d0d3ae6120f1eed2d3f36de30fbd28ed16051e`。
