; naskfunc
; tab = 4

[FORMAT "WCOFF"]            ; 制作目标文件的模式
[INSTRSET "i486p"]          ; 使用到486为止的指令
[BITS 32]                   ; 制作32位模式用的机械语言
[FILE "naskfunc.nas"]       ; 源程序文件名

    global      _io_hlt, _io_cli, _io_sti, _io_stihlt
    global      _io_in8, _io_in16, _io_in32
    global      _io_out8, _io_out16, _io_out32
    global      _io_load_eflags, _io_store_eflags
    global      _load_gdtr, _load_idtr
    global      _load_cr0, _store_cr0
	global      _load_tr
    global      _asm_inthandler20, _asm_inthandler21
    global      _asm_inthandler27, _asm_inthandler2c
    global      _memtest_sub
	global      _taskswitch3
	global      _taskswitch4
	global		_farjmp
    extern      _inthandler20, _inthandler21
    extern      _inthandler27, _inthandler2c

; 以下是实际函数

[SECTION .text]             ; 目标文件中写了这些之后再写程序
_io_hlt:  ; void io_hlt(void);
    hlt
    ret

_io_cli:  ; void io_cli(void);
    cli
    ret

_io_sti:  ; void io_sti(void);
    sti
    ret

_io_stihlt:  ; void io_stihlt(void);
    sti
    hlt
    ret

_io_in8:  ; int io_in8(int port);
    mov     edx, [esp+4]    ;port
    mov     eax, 0
    in      al, dx
    ret

_io_in16:  ; int io_in16(int port);
    mov     edx, [esp+4]
    mov     eax, 0
    in      ax, dx
    ret

_io_in32:  ; int io_in32(int port);
    mov     edx, [esp+4]
    in      eax, dx

_io_out8:  ; void io_out8(int port, int data);
    mov     edx, [esp+4]  ; port
    mov     al, [esp+8]   ; data
    out     dx, al
    ret

_io_out16:  ; void io_out16(int port, int data);
    mov     edx, [esp+4]
    mov     ax, [esp+8]
    out     dx, ax
    ret

_io_out32:  ; void io_out32(int port, int data);
    mov     edx, [esp+4]
    mov     eax, [esp+8]
    out     dx, eax
    ret

_io_load_eflags:  ; int io_store_eflags(int eflags);
    pushfd  ; push eflags
    pop     eax
    ret

_io_store_eflags:  ; void io_store_eflags(int eflags);
    mov     eax, [esp+4]
    push    eax
    popfd  ; pop eflags
    ret

_load_gdtr:  ; void load_gdtr(int limit, int addr);
    mov     ax, [esp+4]
    mov     [esp+6], ax
    lgdt    [esp+6]
    ret

_load_idtr:  ; void load_idtr(int limit, int addr);
    mov     ax, [esp+4]
    mov     [esp+6], ax
    lidt    [esp+6]
    ret

_load_cr0:  ; void int load_cr0(void);
    mov     eax, cr0
    ret

_store_cr0:  ; void store_cr0(int cr0);
    mov     eax, [esp+4]
    mov     cr0, eax
    ret

_load_tr:  ;void load_tr(int tr)
	ltr		[esp+4]
	ret

_asm_inthandler20:
    push    es
    push    ds
    pushad
    mov     eax, esp
    push    eax
    mov     ax, ss
    mov     ds, ax
    mov     es, ax
    call    _inthandler20
    pop     eax
    popad
    pop     ds
    pop     es
    iretd

_asm_inthandler21:
    push    es
    push    ds
    pushad
    mov     eax, esp
    push    eax
    mov     ax, ss
    mov     ds, ax
    mov     es, ax
    call    _inthandler21
    pop     eax
    popad
    pop     ds
    pop     es
    iretd

_asm_inthandler27:
    push    es
    push    ds
    pushad
    mov     eax, esp
    push    eax
    mov     ax, ss
    mov     ds, ax
    mov     es, ax
    call    _inthandler27
    pop     eax
    popad
    pop     ds
    pop     es
    iretd

_asm_inthandler2c:
    push    es
    push    ds
    pushad
    mov     eax, esp
    push    eax
    mov     ax, ss
    mov     ds, ax
    mov     es, ax
    call    _inthandler2c
    pop     eax
    popad
    pop     ds
    pop     es
    iretd

_memtest_sub:  ; unsigned int memtest_sub(unsigned int start,unsigned int end);
    push    edi
    push    esi
    push    ebx
    mov     esi, 0xaa55aa55
    mov     edi, 0x55aa55aa
    mov     eax, [esp+12+4]

mts_loop:
    mov     ebx, eax
    add     ebx, 0x0ffc
    mov     edx, [ebx]
    mov     [ebx], esi
    xor     dword [ebx], 0xffffffff
    cmp     edi,[ebx]
    jne     mts_fin
    xor     dword [ebx], 0xffffffff
    cmp     esi, [ebx]
    jne     mts_fin
    mov     [ebx], edx
    add     eax, 0x1000
    cmp     eax, [esp+12+8]
    jbe     mts_loop
    pop     ebx
    pop     esi
    pop     edi
    ret
mts_fin:
    mov     [ebx],edx
    pop     ebx
    pop     esi
    pop     edi
    ret

_farjmp:  ;void farjmp(int eip,int cs);
	jmp		far [esp+4]
	ret

_taskswitch3:  ; void taskswitch3(void);
	jmp		3*8:0
	ret

_taskswitch4:  ; void taskswitch4(void);
	jmp		4*8:0
	ret

; _write_mem8:  ; void write_mem8(int addr, int data);
;     mov     ecx, [esp+4]    ; [esp+4]中存放的是地址，将其读入ecx
;     mov     al, [esp+8]     ; [esp+8]中存放的是数据，将其读入al
;     mov     [ecx], al
;     ret