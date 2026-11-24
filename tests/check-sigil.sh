#!/bin/sh
set -eu
export LC_ALL=C

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
: "${SIGIL_BIN:=/home/hatch/workspace/sigil/target/release/sigil}"

if [ ! -x "$SIGIL_BIN" ]; then
    if which sigil >/dev/null 2>&1; then
        SIGIL_BIN=$(which sigil)
    else
        echo "sigil binary not found at $SIGIL_BIN" >&2
        exit 1
    fi
fi

echo "Using sigil binary: $SIGIL_BIN"

# 1. 通常モジュールからの原始操作は処理系が拒否することを確認する
echo "=== Test 1: normal module calling raw op without unchecked ==="
out1=$( "$SIGIL_BIN" check --root "$ROOT_DIR" "$ROOT_DIR/tests/sigil/normal_raw.sigil" 2>&1 || true )
printf '%s\n' "$out1"
if ! printf '%s\n' "$out1" | grep -Eq '原始操作は.*unchecked.*の節を持つ関数の本体でだけ呼べる'; then
    echo "FAILED: Test 1 did not reject raw op outside unchecked" >&2
    exit 1
fi
echo "OK: Rejected raw op in normal module without unchecked"

echo "=== Test 2: normal module (trust none) with unchecked is rejected ==="
out2=$( "$SIGIL_BIN" check --root "$ROOT_DIR" "$ROOT_DIR/tests/sigil/normal_unchecked.sigil" 2>&1 || true )
printf '%s\n' "$out2"
if ! printf '%s\n' "$out2" | grep -Eq 'unchecked.*は.*trust base.*のモジュールの関数にだけ書ける'; then
    echo "FAILED: Test 2 did not reject unchecked in trust none module" >&2
    exit 1
fi
echo "OK: Rejected unchecked in trust none module"

echo "=== Test 3: trust base module without --trust-base is rejected ==="
out3=$( "$SIGIL_BIN" check --root "$ROOT_DIR" "$ROOT_DIR/tests/sigil/tb_unauthorized.sigil" 2>&1 || true )
printf '%s\n' "$out3"
if ! printf '%s\n' "$out3" | grep -Eq '許可がない'; then
    echo "FAILED: Test 3 did not reject unauthorized trust base module" >&2
    exit 1
fi
echo "OK: Rejected unauthorized trust base module"

echo "=== Test 4: authorized trust base module passes check ==="
out4=$( "$SIGIL_BIN" check --root "$ROOT_DIR" --trust-base tests.sigil.kernel_tb "$ROOT_DIR/tests/sigil/kernel_tb.sigil" 2>&1 )
printf '%s\n' "$out4"
if ! printf '%s\n' "$out4" | grep -Eq 'ok'; then
    echo "FAILED: Test 4 failed to check authorized trust base module" >&2
    exit 1
fi
echo "OK: Authorized trust base module passed check"

# 2. 許可された前提は sigil spec に出ることを確認する
echo "=== Test 5: sigil spec shows allowed premises ==="
out5=$( "$SIGIL_BIN" spec --root "$ROOT_DIR" "$ROOT_DIR/tests/sigil/kernel_tb.sigil" 2>&1 )
printf '%s\n' "$out5"
if ! printf '%s\n' "$out5" | grep -Eq '検査を外す: 確保器が初期化済みかつページ管理が有効' ||
   ! printf '%s\n' "$out5" | grep -Eq '使う原始操作: raw\.alloc（前提: 確保器が初期化済みであり、要求サイズが有効である）, raw\.free（前提: 番地が alloc で得られた生きた領域の先頭であり、サイズ・アライメントが一致する）'; then
    echo "FAILED: Test 5 sigil spec output did not contain expected premises and raw ops" >&2
    exit 1
fi
echo "OK: sigil spec correctly showed allowed premises and raw operations"

echo "ALL SIGIL COMPILER CHECKS PASSED"
