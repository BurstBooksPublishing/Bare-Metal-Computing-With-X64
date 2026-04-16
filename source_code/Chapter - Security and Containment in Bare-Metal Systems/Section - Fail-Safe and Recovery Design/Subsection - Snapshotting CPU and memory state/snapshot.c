#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ... helpers and structs unchanged ... */

int produce_snapshot(void *snapshot_buffer, size_t snapshot_buffer_size,
                     void *phys_mem_src, size_t mem_size, uint64_t cpu_id) {
    struct cpu_snapshot_header *hdr = snapshot_buffer;
    uint8_t *p = (uint8_t*)snapshot_buffer + sizeof(*hdr);

    hdr->signature = 0x53E4A5C7B1ULL;
    hdr->cpu_id = cpu_id;
    hdr->timestamp = 0;
    hdr->mem_size = mem_size;

    struct cpu_regs *regs = (struct cpu_regs*)p;

    asm volatile(
        "mov %%rax, %0\n\t" "mov %%rbx, %1\n\t" "mov %%rcx, %2\n\t" "mov %%rdx, %3\n\t"
        "mov %%rsi, %4\n\t" "mov %%rdi, %5\n\t" "mov %%rbp, %6\n\t" "mov %%rsp, %7\n\t"
        "mov %%r8,  %8\n\t" "mov %%r9,  %9\n\t" "mov %%r10, %10\n\t" "mov %%r11, %11\n\t"
        "mov %%r12, %12\n\t" "mov %%r13, %13\n\t" "mov %%r14, %14\n\t" "mov %%r15, %15\n\t"
        : "=m"(regs->rax), "=m"(regs->rbx), "=m"(regs->rcx), "=m"(regs->rdx),
          "=m"(regs->rsi), "=m"(regs->rdi), "=m"(regs->rbp), "=m"(regs->rsp),
          "=m"(regs->r8),  "=m"(regs->r9),  "=m"(regs->r10), "=m"(regs->r11),
          "=m"(regs->r12), "=m"(regs->r13), "=m"(regs->r14), "=m"(regs->r15)
        : : "memory"
    );

    asm volatile("leaq (%%rip), %0" : "=r"(regs->rip));
    asm volatile("pushfq; popq %0" : "=r"(regs->rflags));
    asm volatile("mov %%cr0, %0" : "=r"(regs->cr0));
    asm volatile("mov %%cr3, %0" : "=r"(regs->cr3));
    asm volatile("mov %%cr4, %0" : "=r"(regs->cr4));
    regs->msr_tsc = rdmsr(0x10);

    p += sizeof(*regs);
    hdr->saved_regs_size = sizeof(*regs);

    uint32_t eax, ebx, ecx, edx;
    cpuid(0x0D, 0, &eax, &ebx, &ecx, &edx);
    uint32_t xsave_size = eax;
    hdr->xsave_size = xsave_size;

    if ((size_t)(p - (uint8_t*)snapshot_buffer) + xsave_size + mem_size > snapshot_buffer_size)
        return -1;

    wbinvd();

    uint64_t xcr0 = xgetbv(0);
    void *xsave_area = p;
    asm volatile("xsave64 (%0)"
                 :
                 : "r"(xsave_area),
                   "a"((uint32_t)xcr0),
                   "d"((uint32_t)(xcr0>>32))
                 : "memory");
    p += xsave_size;

    /* 6) copy requested physical memory region */
    memcpy(p, phys_mem_src, mem_size);
    p += mem_size;

    return 0;
}