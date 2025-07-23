; memory.asm - Dynamic memory allocation implementation
; Supports: mem_alloc, mem_free
; Compile with: nasm -f elf32 memory.asm -o memory.o

%define HEAP_SIZE 0x10000   ; 64KB heap size

section .data
align 4
heap_start: times HEAP_SIZE db 0  ; Heap memory area
free_list:  dd 0                  ; Free list head pointer
; 用户程序内存区域定义
USER_BASE    equ 0x100000
USER_SIZE    equ 0x10000
section .text
global mem_init, mem_alloc, mem_free

; Initialize memory management system
mem_init:
    mov eax, heap_start        ; Heap start address
    mov dword [eax], HEAP_SIZE ; Initialize block size (free)
    mov dword [eax+4], 0       ; prev = null
    mov dword [eax+8], 0       ; next = null
    
    mov edx, eax
    add edx, HEAP_SIZE
    sub edx, 4
    mov dword [edx], HEAP_SIZE ; Set block tail size
    
    mov dword [free_list], eax ; free_list = heap_start
    ret

; Memory allocation function
; Input: eax = requested size
; Output: eax = allocated memory address (0 on failure)
mem_alloc:
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov esi, eax             ; Save requested size
    add eax, 4               ; Add header size
    and eax, 0xFFFFFFF8      ; Align to 8 bytes
    
    cmp eax, 16
    jge .size_ok
    mov eax, 16              ; Minimum block size 16 bytes
.size_ok:
    mov edi, eax             ; edi = required block size

    mov ebx, [free_list]     ; ebx = current free block
    test ebx, ebx
    jz .alloc_fail           ; No free blocks

.search_loop:
    mov eax, [ebx]           ; Load block size
    and eax, 0xFFFFFFF8      ; Clear flags
    cmp eax, edi
    jge .found_block         ; Found large enough block
    
    mov ebx, [ebx+8]         ; Next block
    test ebx, ebx
    jnz .search_loop

.alloc_fail:
    xor eax, eax             ; Allocation failed
    jmp .alloc_done

.found_block:
    mov edx, [ebx]           ; edx = original size+flags
    and edx, 0xFFFFFFF8      ; edx = actual size
    mov ecx, edx
    sub ecx, edi             ; Remaining size
    
    cmp ecx, 16
    jl .alloc_whole          ; Remainder too small, allocate whole block

    ; Split block
    mov dword [ebx], edi     ; Set new block size
    or dword [ebx], 1        ; Set allocated flag
    
    lea eax, [ebx + edi - 4]
    mov dword [eax], edi     ; Set tail

    ; Create new free block
    lea esi, [ebx + edi]     ; New block address
    mov dword [esi], ecx     ; Set size
    
    lea eax, [esi + ecx - 4]
    mov dword [eax], ecx     ; Set tail

    ; Update linked list
    mov eax, [ebx+4]         ; Previous block
    mov edx, [ebx+8]         ; Next block
    
    mov [esi+4], eax         ; new_block->prev = prev
    mov [esi+8], edx         ; new_block->next = next
    
    test eax, eax
    jz .update_head
    mov [eax+8], esi         ; prev->next = new_block
    jmp .update_next
.update_head:
    mov [free_list], esi     ; Update list head
.update_next:
    test edx, edx
    jz .alloc_return
    mov [edx+4], esi         ; next->prev = new_block

.alloc_return:
    lea eax, [ebx+4]         ; Return data area address
    jmp .alloc_done

.alloc_whole:
    ; Allocate whole block
    or dword [ebx], 1        ; Set allocated flag

    ; Remove from linked list
    mov eax, [ebx+4]         ; Previous block
    mov edx, [ebx+8]         ; Next block
    
    test eax, eax
    jz .remove_head
    mov [eax+8], edx         ; prev->next = next
    jmp .remove_next
.remove_head:
    mov [free_list], edx     ; Update list head
.remove_next:
    test edx, edx
    jz .alloc_return
    mov [edx+4], eax         ; next->prev = prev
    jmp .alloc_return

.alloc_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret

; Memory free function
; Input: eax = memory address to free
mem_free:
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    lea ebx, [eax-4]         ; ebx = block header address
    mov esi, [ebx]           ; esi = header info
    and esi, 0xFFFFFFF8      ; esi = actual size

    ; Clear allocated flag
    mov dword [ebx], esi     ; Mark as free
    
    lea eax, [ebx + esi - 4]
    mov dword [eax], esi     ; Update tail

    ; Insert at head of free list
    mov edx, [free_list]     ; Current list head
    mov [ebx+8], edx         ; block->next = free_list
    mov dword [ebx+4], 0     ; block->prev = null
    
    test edx, edx
    jz .no_prev
    mov [edx+4], ebx         ; old_head->prev = block
.no_prev:
    mov [free_list], ebx     ; free_list = block

    ; Merge with next block if free
    lea edi, [ebx + esi]     ; edi = next block address
    
    mov eax, heap_start
    mov ecx, eax
    add ecx, HEAP_SIZE       ; ecx = heap end address
    
    cmp edi, ecx
    jae .merge_prev          ; Beyond heap range
    
    mov eax, [edi]           ; Load next block header
    test eax, 1              ; Check allocated flag
    jnz .merge_prev          ; Allocated, skip

    ; Merge current block with next
    and eax, 0xFFFFFFF8      ; Next block size
    add esi, eax             ; New size
    mov [ebx], esi           ; Update current block size
    
    lea eax, [ebx + esi - 4]
    mov [eax], esi           ; Update tail

    ; Remove next block from list
    mov eax, [edi+4]         ; Next block prev
    mov edx, [edi+8]         ; Next block next
    
    test eax, eax
    jz .remove_head_free
    mov [eax+8], edx         ; prev->next = next
    jmp .update_succ
.remove_head_free:
    mov [free_list], edx     ; Update list head
.update_succ:
    test edx, edx
    jz .merge_prev
    mov [edx+4], eax         ; next->prev = prev

.merge_prev:
    ; Check previous block
    mov eax, heap_start
    cmp ebx, eax
    jbe .free_done           ; At heap start
    
    mov eax, [ebx-4]         ; Previous block tail size
    lea edx, [ebx-4]
    sub edx, eax             ; edx = previous block address
    
    cmp edx, heap_start
    jb .free_done            ; Invalid address
    
    mov ecx, [edx]           ; Previous block header
    test ecx, 1              ; Check allocated flag
    jnz .free_done           ; Allocated

    ; Merge previous block with current
    and ecx, 0xFFFFFFF8      ; Previous block size
    add esi, ecx             ; New size
    mov [edx], esi           ; Update previous block size
    
    lea eax, [edx + esi - 4]
    mov [eax], esi           ; Update tail

    ; Current block merged, no further list action needed
    mov ebx, edx             ; Update current block pointer

.free_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    ret