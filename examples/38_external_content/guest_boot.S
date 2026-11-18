/* Tiny real-mode guest for the private QEMU console workflow. */
.code16
.text
    cli
    xorw %ax, %ax
    movw %ax, %ss
    movw $0x7c00, %sp
    movw %ax, %ds
    movw $0x0013, %ax
    int $0x10
    cli
    cld
    movw $0xa000, %ax
    movw %ax, %es

    /* Three exact DAC colors: blue idle, green pressed, magenta released. */
    movw $0x03c8, %dx
    movb $1, %al
    outb %al, %dx
    incw %dx
    xorb %al, %al
    outb %al, %dx
    outb %al, %dx
    movb $63, %al
    outb %al, %dx
    xorb %al, %al
    outb %al, %dx
    movb $63, %al
    outb %al, %dx
    xorb %al, %al
    outb %al, %dx
    movb $63, %al
    outb %al, %dx
    xorb %al, %al
    outb %al, %dx
    movb $63, %al
    outb %al, %dx

    xorb %bl, %bl
    movb $1, %al
    call paint
    movb $'R', %al
    outb %al, $0xe9
poll:
    inb $0x64, %al
    testb $1, %al
    jz poll
    inb $0x60, %al
    cmpb $0x1e, %al
    je key_down
    cmpb $0x9e, %al
    jne poll
    testb %bl, %bl
    jz poll
    xorb %bl, %bl
    movb $3, %al
    call paint
    movb $'U', %al
    outb %al, $0xe9
    jmp poll
key_down:
    testb %bl, %bl
    jnz poll
    incb %bl
    movb $2, %al
    call paint
    movb $'D', %al
    outb %al, $0xe9
    jmp poll
paint:
    xorw %di, %di
    movw $64000, %cx
    rep stosb
    ret
.org 510
.word 0xaa55
