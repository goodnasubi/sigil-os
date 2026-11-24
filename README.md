# sigil-os
Sigil P6 の OS 本体・対象別プラットフォーム層・OS のビルド／起動テストを置くリポジトリ（ADR 0054 決定6）

## P6-1 の起動

- [起動契約と ESXi での試験手順](docs/boot-contract.md)
- [検証記録と Sigil 側の依存条件](docs/p6-1-verification.md)

x86-64 の GNU ツールチェーン、GNU-EFI、mtools、xorriso を使う。Debian 13
では `build-essential binutils gnu-efi mtools xorriso` が必要。

```sh
make                          # build/loader.efi
make images                   # UEFI ローダー試験用 ISO 3 種
make check-boot               # 補助試験。qemu-system-x86 と ovmf が必要
make iso KERNEL_ELF=/path/to/kernel.elf
```

`make iso` は起動契約に従う ELF を `build/sigil-os.iso` に格納する。
指定基準 SHA の `sigil build` は Linux 用の ELF を生成する。P6 用の ELF を生成するには
Sigil 側の出力プロファイルが必要。
