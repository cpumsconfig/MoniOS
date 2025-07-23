org 0x100000

_start:
    ; 直接写入显存 (文本模式 0xB8000)
    mov edi, 0xB8000    ; 显存起始地址
    mov byte [edi], 'H'  ; 字符
    mov byte [edi+1], 0x0F  ; 属性 (白底黑字)
    mov byte [edi+2], 'e'
    mov byte [edi+3], 0x0F
    mov byte [edi+4], 'l'
    mov byte [edi+5], 0x0F
    mov byte [edi+6], 'l'
    mov byte [edi+7], 0x0F
    mov byte [edi+8], 'o'
    mov byte [edi+9], 0x0F
    mov byte [edi+10], '!'
    mov byte [edi+11], 0x0F
    ret

msg db "Hello, World!", 0