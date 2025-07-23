org 0x10000       ; 新的加载地址
bits 16

start:
    ; 设置段寄存器为0，偏移从0x10000开始
    xor ax, ax
    mov ds, ax
    mov es, ax
    
    ; 设置栈指针 (避免与代码区冲突)
    mov ss, ax
    mov sp, 0x9000
    
    ; 设置80x25文本模式
    mov ax, 0x0003
    int 0x10

    ; 设置显存段
    mov ax, 0xB800
    mov es, ax

    ; 填充整个屏幕为蓝底白字
    mov cx, 80*25 ; 屏幕字符总数
    xor di, di    ; 显存起始位置
    mov ah, 0x1F  ; 属性: 白字蓝底
    mov al, ' '   ; 空格字符
.fill_screen:
    stosw         ; 存储字符+属性
    loop .fill_screen

    ; 计算字符串位置 (使用绝对地址)
    mov si, error_title
    mov di, (0*80 + 5)*2 ; 第0行第5列
    call print_string

    mov si, error_message
    mov di, (10*80 + 5)*2 ; 第10行第5列
    call print_string

    mov si, reboot_msg
    mov di, (20*80 + 5)*2 ; 第20行第5列
    call print_string

    ; 停止执行
    cli
    hlt
    jmp $

print_string:
    mov ah, 0x1F  ; 蓝底白字属性
.loop:
    lodsb         ; 从DS:SI加载字符
    test al, al
    jz .done
    stosw         ; 存储到ES:DI
    jmp .loop
.done:
    ret

; 错误信息数据
error_title db '*** SYSTEM CRASHED ***', 0
error_message db 'KERNEL PANIC: UNRECOVERABLE ERROR', 13, 10
              db 'Error Code: 0xDEADBEEF', 13, 10
              db 'Fault Address: 0x10000', 13, 10
              db 'Please contact system administrator', 0
reboot_msg db 'Press hardware reset to restart', 0

; 填充空间 (不再是引导扇区)
times 1024 - ($-$$) db 0  ; 填充到1KB边界