; 设置VGA图形模式的函数
[global set_vga_mode]
set_vga_mode:
    mov al, 0x13
    mov ah, 0x00
    int 0x10
    
    mov byte [vmode], 8
    mov word [scrnx], 320
    mov word [scrny], 200
    mov dword [vram], 0x000a0000
    ret

; 在保护模式下调用BIOS中断的函数
[global call_bios_int]
call_bios_int:
    ; 保存原始堆栈指针
    mov [saved_esp], esp
    
    pusha
    push ds
    push es
    push fs
    push gs
    
    cli  ; 禁用中断
    
    ; 保存GDT/IDT
    sgdt [gdtr_save]
    sidt [idtr_save]
    
    ; 复制实模式代码到安全区域 (0x8000)
    mov edi, 0x8000
    mov esi, rm_code_start
    mov ecx, (rm_code_end - rm_code_start) ; 直接计算大小
    cld
    rep movsb
    
    ; 复制保护模式返回代码 - 使用固定大小解决标签引用问题
    mov edi, 0x8100
    mov esi, protected_mode
    mov ecx, 50  ; 估计的代码大小
    cld
    rep movsb
    
    ; 复制临时GDT
    mov edi, 0x8200
    mov esi, tmp_gdt
    mov ecx, (tmp_gdt_end - tmp_gdt) ; 直接计算大小
    cld
    rep movsb
    
    ; 设置返回地址
    mov eax, protected_ret
    mov [0x8200 + (ret_addr - tmp_gdt)], eax
    
    ; 切换到实模式
    jmp 0x18:0x8000

; 实模式代码段
[section .text32]
[bits 32]
rm_code_start:
    ; 设置实模式段寄存器
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; 设置实模式堆栈 (0x7000-0x7FFF)
    mov sp, 0x7000
    
    ; 关闭保护模式
    mov eax, cr0
    and eax, 0x7FFFFFFE  ; 同时清除分页标志
    mov cr0, eax
    
    ; 清空流水线
    jmp 0x0000:.flush
.flush:
    ; 启用A20地址线
    in al, 0x92
    or al, 2
    out 0x92, al
    
    ; 设置BIOS中断向量
    xor ax, ax
    mov ds, ax
    
    ; 调用BIOS中断设置VGA模式
    mov ax, 0x0013
    int 0x10
    
    ; 准备返回保护模式
    cli
    
    ; 加载临时GDT
    mov eax, 0x8200
    add eax, (gdt_ptr - tmp_gdt)
    lgdt [eax]
    
    ; 开启保护模式
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; 跳转到保护模式代码
    jmp dword 0x08:0x8100

; 保护模式返回代码
[bits 32]
protected_mode:
    ; 设置保护模式段寄存器
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; 恢复原始GDT和IDT
    lgdt [gdtr_save]
    lidt [idtr_save]
    
    ; 跳转回原始位置
    mov eax, 0x8200
    add eax, (ret_addr - tmp_gdt)
    jmp far [eax]
rm_code_end:

; 保护模式返回点
[section .text]
[bits 32]
protected_ret:
    ; 恢复段寄存器
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; 恢复原始堆栈指针
    mov esp, [saved_esp]
    
    ; 恢复寄存器状态
    pop gs
    pop fs
    pop es
    pop ds
    popa
    
    sti  ; 重新启用中断
    ret
protected_ret_end:

; 数据段
[section .data]
; 保存关键状态
saved_esp:   dd 0

; 显示参数
vmode:      db 0
scrnx:      dw 0
scrny:      dw 0
vram:       dd 0

; GDT/IDT保存区域
gdtr_save:  dw 0
            dd 0
idtr_save:  dw 0
            dd 0

; 临时GDT结构
tmp_gdt:
    ; 空描述符
    dq 0
    
    ; 32位代码段 (0x08)
    dw 0xFFFF       ; 段界限 0-15
    dw 0x0000       ; 段基址 0-15
    db 0x00         ; 段基址 16-23
    db 0x9A         ; P=1, DPL=0, 代码段
    db 0xCF         ; G=1, D/B=1, 界限 16-19=0xF
    db 0x00         ; 段基址 24-31
    
    ; 32位数据段 (0x10)
    dw 0xFFFF       ; 段界限 0-15
    dw 0x0000       ; 段基址 0-15
    db 0x00         ; 段基址 16-23
    db 0x92         ; P=1, DPL=0, 数据段
    db 0xCF         ; G=1, D/B=1, 界限 16-19=0xF
    db 0x00         ; 段基址 24-31
    
    ; 16位代码段 (0x18)
    dw 0xFFFF       ; 段界限 0-15
    dw 0x0000       ; 段基址 0-15
    db 0x00         ; 段基址 16-23
    db 0x9A         ; P=1, DPL=0, 代码段
    db 0x0F         ; G=1, D/B=0 (16位), 界限 16-19=0xF
    db 0x00         ; 段基址 24-31

; GDT指针
gdt_ptr:
    dw 23           ; GDT界限 (3*8-1=23)
    dd 0            ; GDT基址 (运行时设置)

; 返回地址结构
ret_addr:
    dd 0            ; 返回地址偏移
    dw 0x08         ; 代码段选择子

tmp_gdt_end: