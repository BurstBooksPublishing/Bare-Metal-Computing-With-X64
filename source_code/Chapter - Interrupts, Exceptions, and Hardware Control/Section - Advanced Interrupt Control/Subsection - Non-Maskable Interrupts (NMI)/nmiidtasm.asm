.intel_syntax noprefix
.global nmi_idt_entry
.global nmi_stub
.section .rodata
.align 16
nmi_idt_entry:
  /* 64-bit interrupt gate for vector 2:
     offset low, selector, ist/type, offset mid, offset high, zero */
  .quad 0                /* placeholder low 8 bytes */
  .quad 0                /* placeholder high 8 bytes */

/* Build the entry at runtime to fill handler address and IST reliably. */

.section .text
.global setup_nmi_idt
setup_nmi_idt:
  /* rcx = address of IDT base, rdx = address of nmi_stub */
  /* caller must provide IDT base and nmi_stub address */
  push rbp
  mov rbp, rsp

  mov rax, rdx                    ; handler address
  mov rbx, rax
  and eax, 0xffff                 ; offset15:0
  mov word ptr [rcx + 16*2 + 0], ax   ; low word
  shr rbx, 16
  mov word ptr [rcx + 16*2 + 2], bx   ; selector placeholder overwritten below

  /* Write selector (0x08) and IST/type_attr (IST=1, type_attr=0x8E) */
  mov word ptr [rcx + 16*2 + 2], 0x08 ; code segment selector
  mov byte ptr [rcx + 16*2 + 4], 1    ; IST = 1
  mov byte ptr [rcx + 16*2 + 5], 0x8E ; type_attr = 0x8E

  /* offset31:16 */
  mov eax, edx
  shr rax, 16
  mov word ptr [rcx + 16*2 + 6], ax

  /* offset63:32 */
  mov rax, rdx
  shr rax, 32
  mov dword ptr [rcx + 16*2 + 8], eax

  /* zero last dword */
  mov dword ptr [rcx + 16*2 + 12], 0

  pop rbp
  ret

/* NMI stub: minimal prologue, call C-level handler, restore, iretq */
.global nmi_stub
.type nmi_stub, @function
nmi_stub:
  /* interrupt gates automatically switch stack if IST is configured */
  push rax
  push rcx
  push rdx
  push rsi
  push rdi
  push r8
  push r9
  push r10
  push r11
  sub rsp, 8                      ; aligned stack for call
  mov rdi, rsp                    ; pass pointer to saved regs
  call nmi_handler_c              ; C handler for diagnostics/recovery
  add rsp, 8
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop rax
  iretq
.size nmi_stub, .-nmi_stub