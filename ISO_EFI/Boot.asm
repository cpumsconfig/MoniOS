BITS 16
ORG 0x7C00

; 引导扇区入口点
start:
    ; 初始化段寄存器
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; 设置栈指针

    ; 保存引导驱动器号
    mov [drive], dl

    ; 打印加载消息
    mov si, loading_msg
    call print_string

    ; 查找并加载 disk.img
    call load_disk_image

    ; 跳转到加载的镜像
    jmp 0x0000:0x8000

; 加载 disk.img 文件
load_disk_image:
    ; 读取 ISO 9660 主卷描述符 (LBA 16)
    mov ax, 16          ; LBA 16
    mov bx, buffer      ; 缓冲区地址
    mov cx, 1           ; 读取1个扇区
    call read_sectors

    ; 检查卷描述符标识符 ("CD001")
    mov di, buffer + 1
    mov si, cd001
    mov cx, 5
    repe cmpsb
    jne error           ; 标识不匹配则报错

    ; 获取根目录记录位置
    mov eax, [buffer + 158 + 2] ; 根目录记录的LBA (小端序)
    mov [root_lba], eax

    ; 读取根目录记录
    mov ax, [root_lba]
    mov bx, buffer
    mov cx, 1
    call read_sectors

    ; 在根目录中查找 "MoniOS" 目录
    mov di, buffer
    .search_dir:
        cmp byte [di], 0
        je error        ; 到达目录结尾未找到
        mov cx, 6       ; 比较前6字符
        mov si, monios
        repe cmpsb
        je .found_monios
        add di, cx      ; 跳到下一个条目
        jmp .search_dir

    .found_monios:
        ; 获取 Monios 目录的 LBA
        mov eax, [di - 6 + 2] ; 目录记录的LBA
        mov [dir_lba], eax

    ; 读取 Monios 目录内容
    mov ax, [dir_lba]
    mov bx, buffer
    mov cx, 1
    call read_sectors

    ; 在目录中查找 "disk.img" 文件
    mov di, buffer
    .search_file:
        cmp byte [di], 0
        je error
        mov cx, 8       ; 比较8字符
        mov si, diskimg
        repe cmpsb
        je .found_file
        add di, 33      ; 跳到下一个条目 (标准目录条目长度)
        jmp .search_file

    .found_file:
        ; 获取文件的 LBA 和大小
        mov eax, [di - 8 + 2]   ; 文件 LBA
        mov ecx, [di - 8 + 10]  ; 文件大小 (字节)
        mov [file_lba], eax
        mov [file_size], ecx

    ; 计算需要读取的扇区数
    xor edx, edx
    mov eax, [file_size]
    mov ebx, 512
    div ebx             ; 文件大小 / 512
    cmp edx, 0
    je .no_remainder
    inc eax             ; 有余数则增加一个扇区
    .no_remainder:
    mov cx, ax          ; CX = 扇区数

    ; 读取文件到内存 0x8000
    mov ax, [file_lba]
    mov bx, 0x8000      ; 加载地址
    call read_sectors

    ret

; 读取扇区函数
; 输入: AX = LBA, CX = 扇区数, ES:BX = 缓冲区
read_sectors:
    pusha
    mov [lba], ax
    mov [count], cx

    ; 转换 LBA 为 CHS
    xor dx, dx
    mov bx, [sectors_per_track]
    div bx              ; AX = LBA / SPT, DX = 余数 (扇区号 - 1)
    inc dx
    mov [sector], dl

    xor dx, dx
    mov bx, [heads]
    div bx              ; DX = 磁头号, AX = 柱面号
    mov [head], dl
    mov [cylinder], ax

    .retry:
        mov ah, 0x02    ; BIOS 读扇区功能
        mov al, [count] ; 读取扇区数
        mov ch, [cylinder] ; 柱面号低8位
        mov cl, [sector] ; 扇区号 (位 0-5)
        mov dh, [head]  ; 磁头号
        mov dl, [drive] ; 驱动器号
        int 0x13        ; 调用 BIOS
        jnc .done       ; 成功则返回

        ; 出错重试
        dec byte [retry_count]
        jz error
        mov ah, 0x00    ; 重置磁盘
        int 0x13
        jmp .retry

    .done:
        popa
        ret

; 打印字符串函数
; 输入: SI = 字符串地址
print_string:
    pusha
    .loop:
        lodsb           ; 加载字符到 AL
        or al, al
        jz .done        ; 遇到0结束
        mov ah, 0x0E    ; BIOS 打印字符
        int 0x10
        jmp .loop
    .done:
        popa
        ret

; 错误处理
error:
    mov si, error_msg
    call print_string
    jmp $               ; 无限循环

; 数据定义
loading_msg db "Loading MoniOS...", 0
error_msg db "Error!", 0
cd001 db "CD001"
monios db "MoniOS"       ; 目录名 (补空格到10字符)
diskimg db "disk.img"    ; 文件名 (补空格到10字符)

drive db 0
sectors_per_track dw 18  ; 标准1.44MB软盘参数
heads dw 2
lba dw 0
count dw 0
sector db 0
head db 0
cylinder dw 0
retry_count db 3

root_lba dd 0
dir_lba dd 0
file_lba dd 0
file_size dd 0

; 引导扇区签名
times 510 - ($ - $$) db 0
dw 0xAA55

; 缓冲区 (1KB)
buffer equ 0x7E00