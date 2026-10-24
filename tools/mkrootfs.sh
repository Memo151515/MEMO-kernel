#!/bin/sh
# Builds build/rootfs.ext4 from the rootfs/ directory (ext4, 16 MiB) plus a demo glibc program.
# Needs: gcc (static glibc: libc6-dev), e2fsprogs (mke2fs -d).
set -e
cd "$(dirname "$0")/.."
mkdir -p build rootfs/bin
cat > build/hello.c <<'EOC'
#include <stdio.h>
int main(int argc, char **argv) {
    printf("hello from glibc on MNG/MEMO, argc=%d\n", argc);
    for (int i = 0; i < argc; i++) printf("  argv[%d] = %s\n", i, argv[i]);
    return 0;
}
EOC
gcc -static-pie -O2 -o rootfs/bin/hello build/hello.c
[ -f rootfs/readme.txt ] || echo "MNG/MEMO rootfs (ext4)" > rootfs/readme.txt
rm -f build/rootfs.ext4
mke2fs -q -t ext4 -b 4096 -d rootfs build/rootfs.ext4 16M
echo "Created build/rootfs.ext4"
