OBJS = out/kernel.o out/common.o out/monitor.o out/main.o out/gdtidt.o out/nasmfunc.o out/isr.o out/interrupt.o \
     out/string.o out/timer.o out/memory.o out/mtask.o out/keyboard.o out/keymap.o out/fifo.o out/syscall.o out/syscall_impl.o \
     out/stdio.o out/kstdio.o out/hd.o out/fat16.o out/cmos.o out/file.o out/exec.o out/elf.o out/ansi.o out/time.o out/bios.o \
	 out/shutdown.o  out/net.o out/network.o out/screen.o out/execute.o out/log.o out/dma.o out/sb16.o

LIBC_OBJECTS = out/syscall_impl.o out/stdio.o out/string.o out/malloc.o out/time.o out/screen.o out/common.o

APPS = out/test_c.bin out/shell.bin out/c4.bin out/colorful.bin out/blackcat.bin out/mz.bin

# 修复1：添加缺失的start.o编译规则
out/start.o: apps/start.c
	i686-elf-gcc -c -I include -o out/start.o apps/start.c -fno-builtin

# 修复2：分离C应用程序构建规则，避免覆盖问题
out/%.bin : apps/%.asm
	nasm apps/$*.asm -o out/$*.o -f elf
	i686-elf-ld -s -Ttext 0x0 -o out/$*.bin out/$*.o

out/tulibc.a : $(LIBC_OBJECTS)
	i686-elf-ar rcs out/tulibc.a $(LIBC_OBJECTS)

# 修复3：正确构建C应用程序，使用独立的start.o
out/%.bin : apps/%.c out/start.o out/tulibc.a
	i686-elf-gcc -c -I include -o out/$*.o apps/$*.c -fno-builtin
	i686-elf-ld -s -Ttext 0x0 -o out/$*.bin out/$*.o out/start.o out/tulibc.a out/bios.o

out/%.o : kernel/%.c
	i686-elf-gcc -c -I include -O0 -fno-builtin -fno-stack-protector -o out/$*.o kernel/$*.c


# 其它的 kernel/*.asm 文件仍然使用 32 位的规则
out/%.o : kernel/%.asm
	nasm -f elf -o out/$*.o kernel/$*.asm

# bios.asm 是一个例外，它是16位实模式代码，需要用 elf32 格式汇编
# 将此特殊规则放在通用规则之后，确保其优先级最高
out/bios.o : kernel/bios.asm
	nasm -f elf32 kernel/bios.asm -o out/bios.o



out/%.o : lib/%.c
	i686-elf-gcc -c -I include -O0 -fno-builtin -fno-stack-protector -o out/$*.o lib/$*.c

out/%.o : lib/%.asm
	nasm -f elf -o out/$*.o lib/$*.asm

out/%.o : drivers/%.c
	i686-elf-gcc -c -I include -O0 -fno-builtin -fno-stack-protector -o out/$*.o drivers/$*.c

out/%.o : drivers/%.asm
	nasm -f elf -o out/$*.o drivers/$*.asm

out/%.o : fs/%.c
	i686-elf-gcc -c -I include -O0 -fno-builtin -fno-stack-protector -o out/$*.o fs/$*.c

out/%.o : fs/%.asm
	nasm -f elf -o out/$*.o fs/$*.asm

out/%.bin : boot/%.asm
	nasm -I boot/include -o out/$*.bin boot/$*.asm

out/kernel.bin : $(OBJS)
	i686-elf-ld -s -Ttext 0x100000 -o out/kernel.bin $(OBJS)

hd.img : out/boot.bin out/loader.bin out/kernel.bin $(APPS)
	ftimage hd.img -size 80 -bs out/boot.bin
	ftcopy hd.img -srcpath out/loader.bin -to -dstpath /loader.bin 
	ftcopy hd.img -srcpath out/kernel.bin -to -dstpath /kernel.bin
	ftcopy hd.img -srcpath out/test_c.bin -to -dstpath /test_c.bin
	ftcopy hd.img -srcpath out/shell.bin -to -dstpath /shell.bin
	ftcopy hd.img -srcpath out/c4.bin -to -dstpath /c4.bin
	ftcopy hd.img -srcpath apps/test_c.c -to -dstpath /test_c.c
	ftcopy hd.img -srcpath apps/c4.c -to -dstpath /c4.c
	ftcopy hd.img -srcpath out/colorful.bin -to -dstpath /colorful.bin
	ftcopy hd.img -srcpath out/blackcat.bin -to -dstpath /blackcat.bin
	ftcopy hd.img -srcpath out/mz.bin -to -dstpath /mz.bin

run : hd.img
	qemu-system-i386 -hda hd.img \
  -net nic,model=ne2k_pci,macaddr=52:54:00:12:34:56 \
  -net user \
  -audiodev dsound,id=audio0 \
  -device sb16,audiodev=audio0

vmdk : hd.img
	qemu-img convert -f raw -O vmdk hd.img hd.vmdk

clean :
	cmd /c del /f /s /q out

default : clean run