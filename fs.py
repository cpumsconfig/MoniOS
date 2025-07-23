import os
import sys
 
def create_fs_asm(files):
    """完全保持原样的暴力文件系统实现，仅添加二进制支持"""
    asm_content = """; 完全保持原样的暴力文件系统
section .data
global fs_files_count
fs_files_count dd %d
; 文件系统API
global fs_init, fs_list_files, fs_read_file
section .text
fs_init:
    ret
fs_list_files:
    mov esi, file_names
    ret
fs_read_file:
    ; 直接暴力比较文件名
    mov edi, esi
""" % len(files)
 
    for i, filename in enumerate(files):
        if not os.path.exists(filename):
            continue
            
        base = os.path.splitext(filename)[0]
        clean_base = base.replace('.', '_').replace(' ', '_')
        
        asm_content += f"""
    ; 检查文件: {filename}
    mov esi, file_{clean_base}_name
    call str_compare
    je .found_{i+1}
"""
 
    asm_content += """
    ; 文件未找到
    stc
    ret
"""
 
    for i, filename in enumerate(files):
        if not os.path.exists(filename):
            continue
            
        base = os.path.splitext(filename)[0]
        clean_base = base.replace('.', '_').replace(' ', '_')
        
        asm_content += f"""
.found_{i+1}:
    mov esi, file_{clean_base}_content_str
    clc
    ret
"""
 
    asm_content += """
str_compare:
    push eax
    push esi
    push edi
.loop:
    mov al, [esi]
    cmp al, [edi]
    jne .not_equal
    test al, al
    jz .equal
    inc esi
    inc edi
    jmp .loop
.equal:
    xor eax, eax
    jmp .done
.not_equal:
    or eax, 1
.done:
    pop edi
    pop esi
    pop eax
    ret
"""
 
    asm_content += """
section .data
file_names db """
    
    name_list = []
    for filename in files:
        if not os.path.exists(filename):
            continue
        name_list.append(f"'{filename} '")
    asm_content += ', '.join(name_list) + ',0\n\n'
 
    for filename in files:
        if not os.path.exists(filename):
            continue
            
        base = os.path.splitext(filename)[0]
        clean_base = base.replace('.', '_').replace(' ', '_')
        
        # 二进制模式读取文件
        with open(filename, 'rb') as f:
            content = f.read()
        
        # 生成转义后的字符串
        escaped = []
        for byte in content:
            if byte == 0:  # 处理null字节
                escaped.append('\\0')
            elif byte == ord('\''):  # 处理单引号
                escaped.append('\\\'')
            elif byte == ord('\\'):  # 处理反斜杠
                escaped.append('\\\\')
            elif 32 <= byte <= 126:  # 可打印ASCII
                escaped.append(chr(byte))
            else:  # 其他不可打印字符保持原样
                escaped.append(f'\\x{byte:02x}')
        
        asm_content += f"""
; 文件: {filename}
file_{clean_base}_name db '{filename}',0
file_{clean_base}_content_str db '{''.join(escaped)}',0
"""
 
    with open("fs.asm", "w") as f:
        f.write(asm_content)
    print("Created original brute-force fs.asm")
 
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python fs.py file1 [file2 ...]")
        sys.exit(1)
    
    create_fs_asm(sys.argv[1:])
 