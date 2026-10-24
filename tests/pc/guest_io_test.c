#include "pc/guest/guest_io.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t device_offset;
static uint16_t device_value;
static uint8_t dma_memory[0x200000];
static uint32_t dma_words[4];
static size_t dma_word_count;

static void device_write(uint32_t offset, uint16_t value, void *context)
{
    (void)context;
    device_offset = offset;
    device_value = value;
}

static void *dma_resolve(uint32_t address, size_t length, void *context)
{
    (void)context;
    if (address < 0x80000000u || address > 0x80200000u ||
        length > 0x80200000u - address) return NULL;
    return dma_memory + (address - 0x80000000u);
}

static int dma_target(const uint32_t *words, size_t count, void *context)
{
    (void)context;
    assert(count <= sizeof(dma_words) / sizeof(dma_words[0]));
    memcpy(dma_words, words, count * sizeof(*words));
    dma_word_count = count;
    return 0;
}

int main(void)
{
    Memories_GuestIoReset();

    assert(Memories_GuestIoContains(MEMORIES_GUEST_IO_BASE, 1));
    assert(Memories_GuestIoContains(0x1F801D4Cu, 2));
    assert(!Memories_GuestIoContains(MEMORIES_GUEST_IO_END, 1));
    assert(!Memories_GuestIoContains(MEMORIES_GUEST_IO_END - 1u, 2));

    Memories_GuestIoWrite8(0x1F801D4Cu, 0x34);
    assert(Memories_GuestIoRead8(0x1F801D4Cu) == 0x34);

    Memories_GuestIoWrite16(0x1F801D4Cu, 0xABCDu);
    assert(Memories_GuestIoRead8(0x1F801D4Cu) == 0xCD);
    assert(Memories_GuestIoRead16(0x1F801D4Cu) == 0xABCDu);
    assert(Memories_GuestIoRead16(0x9F801D4Cu) == 0xABCDu);
    assert(Memories_GuestIoRead16(0xBF801D4Cu) == 0xABCDu);

    Memories_GuestIoWrite32(0x1F801D48u, 0x12345678u);
    assert(Memories_GuestIoRead32(0x1F801D48u) == 0x12345678u);
    assert(Memories_GuestIoRead16(0x1F801D4Au) == 0x1234u);

    Memories_GuestIoReset();
    assert(Memories_GuestIoRead32(0x1F801D48u) == 0);
    Memories_GuestIoWrite16(0x1F801118u, 10);
    Memories_GuestIoWrite16(0x1F801114u, 0x0018u); /* repeat + IRQ */
    Memories_GuestIoAdvance(10);
    assert(Memories_GuestIoRead16(0x1F801110u) == 0);
    assert(Memories_GuestIoRead16(0x1F801070u) == 0x0020u);
    Memories_GuestIoWrite16(0x1F801070u, 0x0020u);
    assert(Memories_GuestIoRead16(0x1F801070u) == 0);
    Memories_GuestIoWrite16(0x1F801110u, 7);
    Memories_GuestIoWrite16(0x1F801114u, 0x0002u); /* reset counter */
    assert(Memories_GuestIoRead16(0x1F801110u) == 0);
    assert(Memories_GuestIoRegisterDevice(0xC00u, 0x200u, NULL, device_write, NULL) == 0);
    Memories_GuestIoWrite16(0x1F801C04u, 0x3456u);
    assert(device_offset == 0xC04u && device_value == 0x3456u);
    Memories_GuestIoSetMemoryResolver(dma_resolve, NULL);
    {
        uint32_t word = 0x11223344u;
        memcpy(dma_memory + 0x200, &word, sizeof(word));
        word = 0x55667788u;
        memcpy(dma_memory + 0x204, &word, sizeof(word));
    }
    assert(Memories_GuestIoRegisterDmaTarget(2, dma_target, NULL) == 0);
    Memories_GuestIoWrite32(0x1F8010A0u, 0x80000200u);
    Memories_GuestIoWrite32(0x1F8010A4u, 2);
    Memories_GuestIoWrite32(0x1F8010A8u, 0x01000001u);
    assert(dma_word_count == 2 && dma_words[0] == 0x11223344u && dma_words[1] == 0x55667788u);
    Memories_GuestIoWrite32(0x1F8010E0u, 0x80000100u);
    Memories_GuestIoWrite32(0x1F8010E4u, 4);
    Memories_GuestIoWrite32(0x1F8010E8u, 0x01000000u);
    {
        uint32_t word;
        memcpy(&word, dma_memory + 0x100, sizeof(word));
        assert(word == 0x800000FCu);
        memcpy(&word, dma_memory + 0x0F4, sizeof(word));
        assert(word == 0x00FFFFFFu);
    }
    assert(Memories_GuestIoRead32(0x1F8010E8u) == 0);
    Memories_GuestIoSetMemoryResolver(NULL, NULL);
    return 0;
}
