org 0x7c00

; 简化FAT12头部
jmp short start
nop
db "MONIOS   "      ; OEM标识
dw 512             ; 字节/扇区
db 1               ; 扇区/簇
dw 1               ; 保留扇区
db 2               ; FAT数量
dw 224             ; 根目录条目
dw 2880            ; 总扇区数
db 0xF0            ; 介质描述符
dw 9               ; 扇区/FAT
dw 18              ; 扇区/磁道
dw 2               ; 磁头数
dd 0               ; 隐藏扇区
dd 0               ; 大总扇区数
db 0x80            ; 驱动器号
db 0               ; 保留
db 0x29            ; 扩展签名
dd 0x12345678      ; 卷序列号
db "SYS     "      ; 卷标
db "FAT12   "      ; 文件系统类型

start:
    ; 设置栈指针
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; 保存启动驱动器号
    mov [boot_drive], dl

    ; 开启A20地址线
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 显示加载消息
    mov si, load_msg
    call print_string

    ; 重置磁盘系统
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; 加载内核到内存 0x10000 (64KB)
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    
    ; 使用LBA模式读取磁盘
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    
    ; 显示加载成功消息
    mov si, load_ok_msg
    call print_string
    
    ; 切换到保护模式
    cli
    
    ; 加载GDT
    lgdt [gdt_desc]
    
    ; 设置保护模式位
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; 远跳转到保护模式
    jmp CODE_SEG:protected_mode

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

disk_error:
    mov si, error_msg
    call print_string
    jmp $

; 32位GDT定义
gdt_start:
    ; 空描述符
    dq 0x0000000000000000
    
    ; 代码段描述符 (基址0)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
    
    ; 数据段描述符 (基址0)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
    
    ; 显存段描述符 (基址0xB8000)
    dw 0xFFFF
    dw 0x8000
    db 0x0B
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10
VIDEO_SEG equ 0x18

; 磁盘地址包 (DAP)
dap:
    db 0x10        ; 结构大小
    db 0           ; 保留
    dw 128         ; 扇区数 (64KB)
    dw 0           ; 目标偏移
    dw 0x1000      ; 目标段
    dd 1           ; 起始LBA (扇区1)
    dd 0           ; 高32位LBA

; 32位保护模式入口
[bits 32]
protected_mode:
    ; 设置数据段
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    mov esp, 0x90000
    
    ; 设置显存段
    mov ax, VIDEO_SEG
    mov gs, ax
    
    ; 显示保护模式标记
    mov byte [gs:0], 'P'
    mov byte [gs:1], 0x0F
    mov byte [gs:2], 'M'
    mov byte [gs:3], 0x0F
    
    ; 直接跳转到内核入口点 (移除无效的ELF检查)
    jmp CODE_SEG:0x10000

; 数据定义
boot_drive db 0
load_msg db "Loading kernel...", 0
load_ok_msg db " OK", 0
error_msg db " DiskErr!", 0

; 填充并添加引导签名
times 510-($-$$) db 0
dw 0xaa55