#!/usr/bin/env bash
set -euo pipefail

CC=gcc
CFLAGS="-Wall -Wextra -O2"
LDFLAGS="-lws2_32"
OUTDIR=build

RECEIVER_SRC="receiver.c crc.c"
SENDER_SRC="sender.c crc.c"

RECEIVER_EXE=receiver.exe
SENDER_EXE=sender.exe

echo "==> Using compiler: $CC"
echo "==> Creating output directory: $OUTDIR"

mkdir -p "$OUTDIR"

echo "==> Compiling receiver..."
$CC $CFLAGS $RECEIVER_SRC -o "$OUTDIR/$RECEIVER_EXE" $LDFLAGS

echo "==> Compiling sender..."
$CC $CFLAGS $SENDER_SRC -o "$OUTDIR/$SENDER_EXE" $LDFLAGS

echo "==> Build successful"
echo "    Receiver: $OUTDIR/$RECEIVER_EXE"
echo "    Sender  : $OUTDIR/$SENDER_EXE"
