#include 

/* Port I/O primitives for x86_64, GCC/Clang. */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Read 32-bit PCI config dword via 0xCF8/0xCFC */
uint32_t pci_config_read_dword_io(uint8_t bus, uint8_t device,
                                 uint8_t function, uint8_t offset)
{
    uint32_t addr = (1u << 31) |
                    ((uint32_t)bus << 16) |
                    ((uint32_t)device << 11) |
                    ((uint32_t)function << 8) |
                    (offset & 0xFCu);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/* Read 16-bit and 8-bit helpers */
static inline uint16_t pci_config_read_word_io(uint8_t b, uint8_t d, uint8_t f, uint8_t o){
    uint32_t v = pci_config_read_dword_io(b,d,f,o & ~0x2);
    return (uint16_t)((v >> ((o & 2) * 8)) & 0xFFFF);
}
static inline uint8_t pci_config_read_byte_io(uint8_t b, uint8_t d, uint8_t f, uint8_t o){
    uint32_t v = pci_config_read_dword_io(b,d,f,o & ~0x3);
    return (uint8_t)((v >> ((o & 3) * 8)) & 0xFF);
}

/* Example: probe vendor/device at bus 0 device 0 function 0 */
void example_probe(void){
    uint16_t vendor = pci_config_read_word_io(0,0,0,0x00);
    uint16_t device = pci_config_read_word_io(0,0,0,0x02);
    /* Use vendor==0xFFFF to detect absent device */
    (void)device; /* further parsing omitted for brevity */
}