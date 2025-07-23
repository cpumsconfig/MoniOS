BOOT_SRC=boot.asm
KERNEL_SRC=kernel.asm
SHELL_SRC=shell.asm
IO_SRC=io.asm
NETWORK_SRC=network.asm
FS_ASM=fs.asm
MEMORY_SRC=memory.asm  # 新增内存管理模块
OUTPUT_ISO=monios.iso
WORK_DIR=build
BOOT_BIN=$(WORK_DIR)/boot.bin
KERNEL_BIN=$(WORK_DIR)/kernel.bin
DISK_IMG=$(WORK_DIR)/disk.img

# 声明伪目标
.PHONY: build clean all

build:
	# 强制重新创建构建目录
	rm -rf $(WORK_DIR)
	mkdir -p $(WORK_DIR)

	# 步骤1：编译引导程序
	@echo "[1/6] 编译引导程序..."
	nasm -f bin $(BOOT_SRC) -o $(BOOT_BIN) || (echo "引导程序编译失败"; exit 1)

	# 验证引导签名
	@boot_size=$$(stat -c%s "$(BOOT_BIN)"); \
	if [ $$boot_size -ne 512 ]; then \
		echo "引导程序大小应为512字节，实际为 $$boot_size 字节"; \
		exit 1; \
	fi; \
	last_two_bytes=$$(tail -c 2 "$(BOOT_BIN)" | hexdump -v -e '1/1 "%02X"'); \
	if [ "$$last_two_bytes" != "55AA" ]; then \
		echo "引导签名错误: 应为55AA，实际为$$last_two_bytes"; \
		exit 1; \
	fi; \
	echo "引导签名验证通过"

	# 步骤2：编译内核组件
	@echo "[2/6] 编译内核组件..."
	
	@echo "编译IO模块..."
	nasm -f elf32 $(IO_SRC) -o $(WORK_DIR)/io.o || (echo "IO模块编译失败"; exit 1)
	
	@echo "编译内核主模块..."
	nasm -f elf32 $(KERNEL_SRC) -o $(WORK_DIR)/kernel.o || (echo "内核编译失败"; exit 1)
	
	@echo "编译Shell模块..."
	nasm -f elf32 $(SHELL_SRC) -o $(WORK_DIR)/shell.o || (echo "Shell编译失败"; exit 1)
	
	@echo "编译Network模块..."
	nasm -f elf32 $(NETWORK_SRC) -o $(WORK_DIR)/network.o || (echo "Network编译失败"; exit 1)
	
	@echo "编译内存管理模块..."  # 新增编译步骤
	nasm -f elf32 memory.asm -o $(WORK_DIR)/memory.o || (echo "内存管理模块编译失败"; exit 1)
	
	@echo "编译应用程序"
	nasm -f bin a.asm -o a.bin
	nasm -f bin bluescreen.asm -o moninit.bin

	# 步骤3：生成文件系统（仅包含 a.asm）
	@echo "[3/6] 生成文件系统..."
	python3 fs.py a.bin moninit.bin || (echo "文件系统生成失败"; exit 1)
	
	@echo "编译文件系统模块..."
	nasm -f elf32 $(FS_ASM) -o $(WORK_DIR)/fs.o || (echo "文件系统编译失败"; exit 1)

	# 步骤4：链接内核
	@echo "[4/6] 链接内核..."
	# 链接内核（包括新增的内存模块）
	ld -m elf_i386 -Ttext 0x10000 -e _start -nostdlib \
	$(WORK_DIR)/kernel.o \
	$(WORK_DIR)/io.o \
	$(WORK_DIR)/shell.o \
	$(WORK_DIR)/network.o \
	$(WORK_DIR)/fs.o \
	$(WORK_DIR)/memory.o \
	-o $(WORK_DIR)/kernel.elf || (echo "内核链接失败"; exit 1)
	
	# 提取二进制内核
	objcopy -O binary $(WORK_DIR)/kernel.elf $(KERNEL_BIN) || (echo "内核提取失败"; exit 1)

	# 检查内核大小
	@kernel_size=$$(stat -c%s "$(KERNEL_BIN)"); \
	echo "内核大小: $$kernel_size 字节"; \
	if [ $$kernel_size -gt 65536 ]; then \
		echo "警告: 内核超过64KB ($$kernel_size 字节)"; \
	fi

	# 步骤5：创建磁盘映像
	@echo "[5/6] 创建磁盘映像..."
	# 创建1.44MB空映像
	dd if=/dev/zero of=$(DISK_IMG) bs=512 count=2880 status=none || (echo "磁盘映像创建失败"; exit 1)

	# 写入引导扇区
	dd if=$(BOOT_BIN) of=$(DISK_IMG) conv=notrunc status=none || (echo "引导扇区写入失败"; exit 1)

	# 写入内核 (从扇区1开始)
	dd if=$(KERNEL_BIN) of=$(DISK_IMG) bs=512 seek=1 conv=notrunc status=none || (echo "内核写入失败"; exit 1)

	
	
	@echo "\n构建成功! ISO镜像: $(OUTPUT_ISO)"
	@echo "可使用以下命令运行测试:"
	@echo "  qemu-system-x86_64 -cdrom $(OUTPUT_ISO) -serial stdio"
	@echo "或直接运行磁盘映像:"
	@echo "  qemu-system-x86_64 -drive format=raw,file=$(DISK_IMG) -serial stdio"

clean:
	rm -rf $(WORK_DIR) $(OUTPUT_ISO) $(FS_ASM)

all: clean build