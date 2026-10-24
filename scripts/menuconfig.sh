#!/bin/sh
# MNG/MEMO - tiny "menuconfig": asks about every option and writes .config
# Usage: make menuconfig      (or ./scripts/menuconfig.sh)
OUT=.config
[ -f "$OUT" ] || cp configs/defconfig "$OUT"
ask() {   # ask NAME "description"
    cur=$(grep "^$1=" "$OUT" | cut -d= -f2)
    printf '%-28s [%s] (y/n, Enter keeps): ' "$2" "${cur:-n}"
    read ans
    case "$ans" in y|Y) val=y ;; n|N) val=n ;; *) val=${cur:-n} ;; esac
    grep -v "^$1=" "$OUT" > "$OUT.tmp"; echo "$1=$val" >> "$OUT.tmp"; mv "$OUT.tmp" "$OUT"
}
printf 'Architecture [%s] (i386/x86_64): ' "$(grep '^ARCH=' $OUT | cut -d= -f2)"
read a
if [ -n "$a" ]; then grep -v '^ARCH=' "$OUT" > "$OUT.tmp"; echo "ARCH=$a" >> "$OUT.tmp"; mv "$OUT.tmp" "$OUT"; fi
ask CONFIG_SERIAL   "Serial console (COM1)"
ask CONFIG_PCI      "PCI bus enumeration"
ask CONFIG_ATA      "ATA PIO disk driver"
ask CONFIG_RTC      "CMOS real time clock"
printf 'Default keyboard layout [%s] (us uk de fr es it tr dvorak colemak): ' "$(grep '^KEYMAP=' $OUT | cut -d= -f2)"
read k
if [ -n "$k" ]; then grep -v '^KEYMAP=' "$OUT" > "$OUT.tmp"; echo "KEYMAP=$k" >> "$OUT.tmp"; mv "$OUT.tmp" "$OUT"; fi
ask CONFIG_DEBUG    "Debug build (-O0 -g)"
sort -o "$OUT" "$OUT"
echo "Saved $OUT. Now run: make"
