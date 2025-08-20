#!/bin/bash

# =============================================================================
# hd-img-to-iso 转换脚本
# 将带引导的hd.img转换为支持BIOS/UEFI双启动的ISO镜像
# 要求：需要安装 grub2-common xorriso mtools
# 用法：sudo ./hd-img-to-iso.sh [输入镜像] [输出ISO]
# =============================================================================

# 检查依赖项
check_dependencies() {
    local missing=()
    command -v grub-mkimage >/dev/null 2>&1 || missing+=("grub2")
    command -v xorriso >/dev/null 2>&1 || missing+=("xorriso")
    command -v mcopy >/dev/null 2>&1 || missing+=("mtools")
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "错误：缺少必要的依赖项: ${missing[*]}"
        echo "请使用以下命令安装："
        echo "  Ubuntu/Debian: sudo apt install grub2-common xorriso mtools"
        echo "  CentOS/RHEL:   sudo yum install grub2 xorriso mtools"
        exit 1
    fi
}

# 清理函数
cleanup() {
    echo "正在清理临时文件..."
    [ -d "$TMP_DIR" ] && rm -rf "$TMP_DIR"
    [ -n "$LOOP_DEV" ] && losetup -d "$LOOP_DEV" >/dev/null 2>&1
}

# 主函数
main() {
    # 输入参数处理
    INPUT_IMG="${1:-hd.img}"
    OUTPUT_ISO="${2:-hd.iso}"
    
    # 检查输入文件
    if [ ! -f "$INPUT_IMG" ]; then
        echo "错误：输入文件 $INPUT_IMG 不存在"
        exit 1
    fi
    
    # 检查文件类型
    if ! file "$INPUT_IMG" | grep -q "DOS/MBR boot sector"; then
        echo "警告：输入文件似乎不包含MBR引导记录"
        echo "继续操作吗？(y/N)"
        read -r confirm
        [[ "$confirm" != [yY] ]] && exit 0
    fi
    
    # 创建临时目录
    TMP_DIR="$(mktemp -d)"
    trap cleanup EXIT
    
    # 创建ISO目录结构
    echo "准备ISO目录结构..."
    mkdir -p "$TMP_DIR"/{boot/grub,efi/boot}
    
    # 复制原始镜像到ISO目录
    cp "$INPUT_IMG" "$TMP_DIR/"
    
    # 创建GRUB配置文件
    echo "创建GRUB引导配置..."
    cat > "$TMP_DIR/boot/grub/grub.cfg" << 'EOF'
set timeout=5
set default=0
menuentry "Boot HD.IMG Image" {
    # 设置根设备为光盘
    set root=(cd0)
    
    # 将hd.img加载为回环设备
    loopback loop /hd.img
    
    # 链式加载镜像的MBR
    chainloader (loop)+1
    
    # 启动系统
    boot
}
EOF
    
    # 创建BIOS引导核心
    echo "生成BIOS引导文件..."
    grub-mkimage \
        -O i386-pc \
        -o "$TMP_DIR/boot/grub/core.img" \
        -p /boot/grub \
        biosdisk part_msdos fat ext2 iso9660 loopback
    
    # 创建UEFI引导文件
    echo "生成UEFI引导文件..."
    grub-mkimage \
        -O x86_64-efi \
        -o "$TMP_DIR/efi/boot/bootx64.efi" \
        -p /efi/boot \
        fat iso9660 part_gpt part_msdos loopback
    
    # 检查引导文件大小
    [ ! -s "$TMP_DIR/boot/grub/core.img" ] && { echo "错误：BIOS引导文件创建失败"; exit 1; }
    [ ! -s "$TMP_DIR/efi/boot/bootx64.efi" ] && { echo "错误：UEFI引导文件创建失败"; exit 1; }
    
    # 创建ISO镜像
    echo "构建ISO镜像: $OUTPUT_ISO..."
    xorriso -as mkisofs \
        -iso-level 3 \
        -full-iso9660-filenames \
        -volid "HD_IMAGE_BOOT" \
        -eltorito-boot boot/grub/core.img \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        -eltorito-catalog boot/grub/boot.cat \
        --grub2-boot-info \
        --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img \
        -eltorito-alt-boot \
        -e efi/boot/bootx64.efi \
        -no-emul-boot \
        -append_partition 2 0xef "$TMP_DIR/efi/boot/bootx64.efi" \
        -output "$OUTPUT_ISO" \
        "$TMP_DIR"
    
    # 验证输出
    if [ $? -eq 0 ] && [ -f "$OUTPUT_ISO" ]; then
        echo -e "\n\033[32mISO创建成功: $OUTPUT_ISO\033[0m"
        echo "大小: $(du -h "$OUTPUT_ISO" | cut -f1)"
        
        # 创建启动测试命令
        echo -e "\n测试命令："
        echo "  # BIOS模式测试:"
        echo "  qemu-system-x86_64 -cdrom $OUTPUT_ISO -boot d"
        echo -e "\n  # UEFI模式测试(需安装OVMF):"
        echo "  qemu-system-x86_64 -bios /usr/share/OVMF/OVMF_CODE.fd -cdrom $OUTPUT_ISO"
    else
        echo -e "\n\033[31m错误：ISO创建失败\033[0m"
        exit 1
    fi
}

# 入口点
check_dependencies
main "$@"