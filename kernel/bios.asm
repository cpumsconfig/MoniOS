; lib/bios.asm
; 使用 NASM 语法，声明为 16 位代码

; 关闭中断，切换到实模式，调用 BIOS 中断，再切换回保护模式





[BITS 16]

; 声明一个全局函数，使其在 C 中可见
global bios_int
global _bios_int

_bios_int equ bios_int

; C 函数调用在 16 位模式下通常是远调用（far call），包括段和偏移
; 但在简单的扁平模型中，我们可以使用 near call
; bios_int(struct regs *r, int interrupt)
; 参数通过栈传递。在 16 位 small 模型下，指针是 2 字节，int 是 2 字节
; BP+4: 第一个参数 (r 的指针)
; BP+6: 第二个参数 (interrupt)

; 在保护模式下调用BIOS中断的函数
; 输入参数：
;   AX = 中断号
;   其他寄存器根据具体中断需求设置
; 输出结果：
;   根据具体中断返回相应结果
; 修改的寄存器：根据具体中断而定


bios_int:
    push    bp              ; 保存基址寄存器
    mov     bp, sp          ; 设置基址指针，以便通过 [bp+X] 访问参数

    ; 保存所有段寄存器和通用寄存器
    push    es
    push    ds
    pusha

    ; 加载参数
    mov     bx, [bp+4]      ; bx = r 的指针
    mov     ax, [bx]        ; 加载 r->ax
    mov     cx, [bx+2]      ; 加载 r->cx
    mov     dx, [bx+4]      ; 加载 r->dx
    mov     si, [bx+6]      ; 加载 r->si
    mov     di, [bx+8]      ; 加载 r->di

    ; 加载段寄存器
    mov     ds, [bx+12]     ; 加载 r->ds
    mov     es, [bx+14]     ; 加载 r->es

    ; 执行中断
    mov     bx, [bp+6]      ; bx = interrupt number
    mov     ax, bx          ; 将中断号放入 ax (或者根据具体中断需求放入其他寄存器)
    int     0x1A            ; 假设我们总是调用 INT 1Ah
                            ; 如果中断号是可变的，需要从栈上获取并放入正确的寄存器
                            ; 例如: int bx (如果 bx 中存中断号)

    ; 保存返回值 (通常在 ax 中)
    mov     bx, [bp+4]      ; 重新获取 r 的指针
    mov     [bx], ax        ; 将 ax 的值存回 r->ax

    ; 恢复所有寄存器
    popa
    pop     ds
    pop     es

    pop     bp              ; 恢复基址寄存器
    ret                     ; 返回到 C 调用者



