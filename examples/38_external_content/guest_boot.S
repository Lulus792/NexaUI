/* Tiny real-mode guest for the private QEMU console workflow. */
.code16
.text
boot:
    cli
    xorw %ax, %ax
    movw %ax, %ss
    movw $0x7c00, %sp
    movw %ax, %ds
    movw $0x0013, %ax
    int $0x10
    cli
    cld
    movw $0x07c0, %ax
    movw %ax, %ds
    movw $0xa000, %ax
    movw %ax, %es

    /* Background key state and white/yellow pointer state. */
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
    outb %al, %dx
    outb %al, %dx
    outb %al, %dx
    outb %al, %dx
    outb %al, %dx
    xorb %al, %al
    outb %al, %dx

    call controller_ready
    movb $0xa8, %al
    outb %al, $0x64
    call controller_ready
    movb $0xd4, %al
    outb %al, $0x64
    call controller_ready
    movb $0xf4, %al
    outb %al, $0x60
mouse_ack:
    inb $0x64, %al
    testb $1, %al
    jz mouse_ack
    movb %al, %ah
    inb $0x60, %al
    testb $0x20, %ah
    jz mouse_ack
    cmpb $0xfa, %al
    jne mouse_ack

    xorw %bx, %bx
    movb $1, %al
    call paint
    movb $'R', %al
    outb %al, $0xe9
poll:
    inb $0x64, %al
    testb $1, %al
    jz poll
    movb %al, %ah
    inb $0x60, %al
    testb $0x20, %ah
    jnz mouse_byte
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
    movb %al, %bh
    xorw %di, %di
    movw $64000, %cx
    rep stosb
    jmp draw_cursor
controller_ready:
    inb $0x64, %al
    testb $2, %al
    jnz controller_ready
    ret

/* The auxiliary port supplies a status byte followed by X and Y deltas. */
mouse_byte:
    cmpb $0, packet_count-boot
    jne mouse_delta
    testb $8, %al
    jz poll
    movb %al, mouse_status-boot
    incb packet_count-boot
    jmp poll
mouse_delta:
    cmpb $1, packet_count-boot
    jne mouse_packet
    movb %al, mouse_dx-boot
    incb packet_count-boot
    jmp poll
mouse_packet:
    movb $0, packet_count-boot
    movb %al, mouse_dy-boot
    movb %bh, %al
    call cursor
    movb mouse_dx-boot, %al
    cbw
    addw cursor_x-boot, %ax
    xorw %dx, %dx
    movw $314, %cx
    call clamp
    movw %ax, cursor_x-boot
    movb mouse_dy-boot, %al
    cbw
    negw %ax
    addw cursor_y-boot, %ax
    movw $194, %cx
    call clamp
    movw %ax, cursor_y-boot
    movb mouse_status-boot, %al
    andb $7, %al
    movb %al, %dl
    xorb mouse_buttons-boot, %al
    movb %dl, mouse_buttons-boot
    movb %al, %dh
    call draw_cursor
    movb $'L', %cl
button_edge:
    shrb $1, %dh
    jnc next_button
    movb %cl, %al
    testb $1, %dl
    jnz trace_button
    orb $32, %al
trace_button:
    outb %al, $0xe9
next_button:
    shrb $1, %dl
    incb %cl
    cmpb $'O', %cl
    jne button_edge
    movb mouse_dx-boot, %al
    orb mouse_dy-boot, %al
    jz poll
    movb $'P', %al
    outb %al, $0xe9
    jmp poll
clamp:
    testw %ax, %ax
    cmovs %dx, %ax
    cmpw %cx, %ax
    cmovg %cx, %ax
    ret
draw_cursor:
    movb $4, %al
    cmpb $0, mouse_buttons-boot
    je cursor
    incb %al
cursor:
    imulw $320, cursor_y-boot, %di
    addw cursor_x-boot, %di
    movw $6, %cx
cursor_row:
    pushw %cx
    movw $6, %cx
    rep stosb
    addw $314, %di
    popw %cx
    loop cursor_row
    ret
cursor_x: .word 30
cursor_y: .word 30
packet_count: .byte 0
mouse_status: .byte 0
mouse_dx: .byte 0
mouse_dy: .byte 0
mouse_buttons: .byte 0
.org 510
.word 0xaa55
