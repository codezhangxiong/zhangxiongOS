; haribote-os
; TAB=4

[instrset "i486p"]

BOTPAK      EQU     0X00280000
DSKCAC      EQU     0X00100000
DSKCAC0     EQU     0X00008000

; 有关BOOT_INFO（c结构体BOOTINFO类型指针）
CYLS        EQU     0x0ff0      ; 设定启动区
LEDS        EQU     0x0ff1
VMODE       EQU     0x0ff2      ; 关于颜色数目的信息
SCRNX       EQU     0x0ff4      ; 分辨率的x（screen x）
SCRNY       EQU     0X0FF6      ; 分辨率的y（screen y）
VRAM        EQU     0X0FF8      ; 图像缓冲区的开始地址

    org     0xc200          ; 这个程序将要被装载到内存的什么地方
;
    mov     bx, 0x4101      ; VGA显卡，640*480*8位彩色
    mov     ax, 0x4f02
    int     0x10
    mov     byte [VMODE], 8 ; 记录画面模式
    mov     word [SCRNX], 640
    mov     word [SCRNY], 480
    mov     dword [VRAM], 0xe0000000
; 用BIOS取得键盘上各种LED指示灯的状态
    mov     ah, 0x02
    int     0x16            ; keyboard BIOS
    mov     [LEDS], al

; PIC关闭一切中断
;   根据AT兼容机的规格，如果要初始化PIC,
;   必须在CLI之前进行，否则有时会挂起
;   随后进行PIC的初始化
    MOV     AL, 0XFF
    OUT     0X21, AL
    NOP                     ; 如果连续执行out指令，有些机种会无法正常运行
    OUT     0XA1, AL

    CLI                     ; 禁止CPU级别的中断
; 为了让CPU能够访问1MB以上的内存空间，设定A20GATE
    CALL    waitkbdout
    MOV     AL, 0XD1
    OUT     0X64, AL
    CALL    waitkbdout
    MOV     AL, 0XDF        ; enable A20
    OUT     0X60, AL
    CALL    waitkbdout

; 切换到保护模式
[INSTRSET "i486p"]          ; “想要使用486指令”的叙述
    LGDT    [GDTR0]         ; 设定临时GDT
    MOV     EAX, CR0
    AND     EAX, 0X7FFFFFFF ; 设bit31为0（为了禁止颁）
    OR      EAX, 0X00000001 ; 设bit0为1（为了切换到保护模式）
    MOV     CR0, EAX
    JMP     pipelineflush
pipelineflush:
    MOV     AX, 1*8         ; 可读写的段32bit
    MOV     DS, AX
    MOV     ES, AX
    MOV     FS, AX
    MOV     GS, AX
    MOV     SS, AX
; bootpack的转送
    MOV     ESI, bootpack   ; 转送源
    MOV     EDI, BOTPAK     ; 转送目的地
    MOV     ECX, 512*1024/4
    CALL    memcpy
; 磁盘数据最终转送到它本来的位置去
; 首先从启动扇区开始
    MOV     ESI, 0X7C00
    MOV     EDI, DSKCAC
    MOV     ECX, 512/4
    CALL    memcpy
; 所有剩下的
    MOV     ESI, DSKCAC0+512
    MOV     EDI, DSKCAC+512
    MOV     ECX, 0
    MOV     CL, BYTE [CYLS]
    IMUL    ECX, 512*18*2/4
    SUB     ECX, 512/4
    CALL    memcpy
; 必须由asmhead来完成的工作，至此全部完毕
;   以后就交由bootpack来完成
; bootpack的启动
    MOV     EBX, BOTPAK
    MOV     ECX, [EBX+16]
    ADD     ECX, 3          ; ECX +=3
    SHR     ECX, 2          ; ECX /=4
    JZ      skip            ; 没有要转送的东西时
    MOV     ESI, [EBX+20]   ; 转送源
    ADD     ESI, EBX
    MOV     EDI, [EBX+12]   ; 转送目的地
    CALL    memcpy
skip:
    MOV     ESP, [EBX+12]   ; 栈初始值
    JMP     DWORD 2*8:0x0000001b
waitkbdout:
    IN      AL, 0X64
    AND     AL, 0X02
    JNZ     waitkbdout
    RET
memcpy:
    MOV     EAX, [ESI]
    ADD     ESI, 4
    MOV     [EDI], EAX
    ADD     EDI, 4
    SUB     ECX, 1
    JNZ     memcpy
    RET
;
    ALIGNB  16
GDT0:
    RESB    8
    DW      0XFFFF, 0X0000, 0X9200, 0X00CF  ; 可读写的段32bit
    DW      0XFFFF, 0X0000, 0X9A28, 0X0047  ; 可执行的段32bit(bootpack用)

    DW      0
GDTR0:
    DW      8*3-1
    DD      GDT0

    ALIGNB  16
bootpack: