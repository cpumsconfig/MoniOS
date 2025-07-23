#!/bin/bash
nasm -f bin boot.asm -o boot.bin
mkisofs -no-emul-boot -b boot.bin -o boot.iso ./ISO
