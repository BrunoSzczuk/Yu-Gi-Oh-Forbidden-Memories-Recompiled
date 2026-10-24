#include "guest_io.h"
#ifdef MEMORIES_TRANSLATED
#include "state.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IRQ_STATUS_OFFSET = 0x070,
    IRQ_MASK_OFFSET = 0x072,
    TIMER_BASE_OFFSET = 0x100,
    TIMER_STRIDE = 0x10,
    TIMER_COUNT = 3,
    DEVICE_COUNT = 8,
    DMA_BASE_OFFSET = 0x080,
    DMA_STRIDE = 0x10,
    DMA_CHANNELS = 7,
    DMA_DPCR_OFFSET = 0x0f0,
    DMA_DICR_OFFSET = 0x0f4,
};

typedef struct GuestTimer {
    uint16_t counter;
    uint16_t mode;
    uint16_t target;
} GuestTimer;

typedef struct GuestIoDevice {
    uint32_t offset;
    size_t size;
    MemoriesGuestIoRead16 read16;
    MemoriesGuestIoWrite16 write16;
    void *context;
} GuestIoDevice;

typedef struct GuestDmaChannel {
    uint32_t madr;
    uint32_t bcr;
    uint32_t chcr;
} GuestDmaChannel;

typedef struct GuestDmaTarget {
    MemoriesGuestDmaWrite write;
    void *context;
} GuestDmaTarget;

static uint8_t registers[MEMORIES_GUEST_IO_END - MEMORIES_GUEST_IO_BASE];
static GuestTimer timers[TIMER_COUNT];
static uint16_t irq_status;
static uint16_t irq_mask;
static GuestIoDevice devices[DEVICE_COUNT];
static GuestDmaChannel dma[DMA_CHANNELS];
static GuestDmaTarget dma_targets[DMA_CHANNELS];
static uint32_t dma_dpcr;
static uint32_t dma_dicr;
static MemoriesGuestIoResolveMemory resolve_memory;
static void *resolve_memory_context;
static int trace_io = -1;

static int tracing(void)
{
    const char *value;
    if (trace_io >= 0) return trace_io;
    value = getenv("MEMORIES_TRACE_IO");
    trace_io = value && *value && strcmp(value, "0") != 0;
    return trace_io;
}

static int canonical(uint32_t address, size_t length, uint32_t *physical)
{
    if (address >= MEMORIES_GUEST_IO_BASE && address < MEMORIES_GUEST_IO_END) {
        *physical = address;
    } else if (address >= 0x9F801000u && address < 0x9F802000u) {
        *physical = address - 0x80000000u;
    } else if (address >= 0xBF801000u && address < 0xBF802000u) {
        *physical = address - 0xA0000000u;
    } else {
        return 0;
    }
    return length <= MEMORIES_GUEST_IO_END - *physical;
}

int Memories_GuestIoContains(uint32_t address, size_t length)
{
    uint32_t physical;
    return canonical(address, length, &physical);
}

int Memories_GuestIoRegisterDevice(uint32_t offset, size_t size,
                                   MemoriesGuestIoRead16 read16,
                                   MemoriesGuestIoWrite16 write16,
                                   void *context)
{
    if (!size || offset >= MEMORIES_GUEST_IO_END - MEMORIES_GUEST_IO_BASE ||
        size > MEMORIES_GUEST_IO_END - MEMORIES_GUEST_IO_BASE - offset ||
        (!read16 && !write16)) return -1;
    {
        unsigned i;
        GuestIoDevice *slot = NULL;
        for (i = 0; i < DEVICE_COUNT; ++i) {
            GuestIoDevice *candidate = &devices[i];
            if (!candidate->size) {
                if (!slot) slot = candidate;
                continue;
            }
            if (offset < candidate->offset + candidate->size &&
                candidate->offset < offset + size) {
                if (candidate->offset != offset || candidate->size != size) return -1;
                slot = candidate;
                break;
            }
        }
        if (!slot) return -1;
        slot->offset = offset;
        slot->size = size;
        slot->read16 = read16;
        slot->write16 = write16;
        slot->context = context;
        return 0;
    }
}

void Memories_GuestIoSetMemoryResolver(MemoriesGuestIoResolveMemory resolve, void *context)
{
    resolve_memory = resolve;
    resolve_memory_context = context;
}

int Memories_GuestIoRegisterDmaTarget(unsigned channel, MemoriesGuestDmaWrite write, void *context)
{
    if (channel >= DMA_CHANNELS) return -1;
    dma_targets[channel].write = write;
    dma_targets[channel].context = context;
    return 0;
}

static uint8_t *at(uint32_t address, size_t length, const char *operation)
{
    uint32_t physical;
    if (!canonical(address, length, &physical)) {
        fprintf(stderr, "guest I/O: invalid %s at 0x%08x (%zu bytes)\n",
                operation, address, length);
        return NULL;
    }
    if (tracing())
        fprintf(stderr, "guest I/O: %s at 0x%08x (%zu bytes)\n",
                operation, address, length);
    return registers + (physical - MEMORIES_GUEST_IO_BASE);
}

static void store16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static uint16_t load16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static void store32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int timer_field(uint32_t physical, unsigned *index, unsigned *field)
{
    uint32_t offset = physical - MEMORIES_GUEST_IO_BASE;
    unsigned relative;
    if (offset < TIMER_BASE_OFFSET || offset >= TIMER_BASE_OFFSET + TIMER_COUNT * TIMER_STRIDE)
        return 0;
    relative = offset - TIMER_BASE_OFFSET;
    *index = relative / TIMER_STRIDE;
    *field = relative % TIMER_STRIDE;
    return *field == 0 || *field == 4 || *field == 8;
}

static GuestIoDevice *find_device(uint32_t physical, size_t length)
{
    uint32_t offset = physical - MEMORIES_GUEST_IO_BASE;
    unsigned i;
    for (i = 0; i < DEVICE_COUNT; ++i) {
        GuestIoDevice *candidate = &devices[i];
        if (candidate->size && offset >= candidate->offset &&
            length <= candidate->offset + candidate->size - offset)
            return candidate;
    }
    return NULL;
}

static int dma_field(uint32_t physical, unsigned *channel, unsigned *field)
{
    uint32_t offset = physical - MEMORIES_GUEST_IO_BASE;
    unsigned relative;
    if (offset < DMA_BASE_OFFSET || offset >= DMA_BASE_OFFSET + DMA_CHANNELS * DMA_STRIDE)
        return 0;
    relative = offset - DMA_BASE_OFFSET;
    *channel = relative / DMA_STRIDE;
    *field = relative % DMA_STRIDE;
    return *field == 0 || *field == 4 || *field == 8;
}

static void sync_dma_channel(unsigned channel)
{
    uint8_t *base = registers + DMA_BASE_OFFSET + channel * DMA_STRIDE;
    store32(base, dma[channel].madr);
    store32(base + 4, dma[channel].bcr);
    store32(base + 8, dma[channel].chcr);
}

static void sync_dma_globals(void)
{
    store32(registers + DMA_DPCR_OFFSET, dma_dpcr);
    store32(registers + DMA_DICR_OFFSET, dma_dicr);
}

static void dma_complete(unsigned channel)
{
    dma[channel].chcr &= ~0x01000000u;
    sync_dma_channel(channel);
    if (dma_dicr & (1u << (16 + channel))) {
        dma_dicr |= 1u << (24 + channel);
        if (dma_dicr & 0x80000000u) Memories_GuestIoRaiseIrq(1u << 3);
        sync_dma_globals();
    }
}

static int dma_run_otc(unsigned channel)
{
    uint32_t address = dma[channel].madr & 0x001ffffcu;
    uint32_t count = dma[channel].bcr & 0xffffu;
    uint32_t i;
    if (!resolve_memory || channel != 6) return -1;
    if (!count) count = 0x10000u;
    for (i = 0; i < count; ++i) {
        uint32_t current = address - i * 4u;
        uint32_t next = i + 1 < count ? ((current - 4u) & 0x001ffffcu) | MEMORIES_GUEST_RAM : 0x00ffffffu;
        uint8_t *destination = resolve_memory(MEMORIES_GUEST_RAM | current, 4, resolve_memory_context);
        if (!destination) return -1;
        store32(destination, next);
    }
    return 0;
}

static int dma_run_target(unsigned channel)
{
    uint32_t block_size = dma[channel].bcr & 0xffffu;
    uint32_t block_count = dma[channel].bcr >> 16;
    uint64_t total;
    uint32_t address = dma[channel].madr & 0x001ffffcu;
    uint32_t chunk[256];
    uint64_t cursor = 0;
    if (!resolve_memory || !dma_targets[channel].write || !(dma[channel].chcr & 1u)) return 0;
    if (!block_size) block_size = 0x10000u;
    /* A block count of zero is one block: only the transfer length of zero
     * means the whole 65536 words. */
    if (!block_count) block_count = 1u;
    total = (uint64_t)block_size * block_count;
    if (total > 0x100000u) return -1;
    while (cursor < total) {
        size_t count = (size_t)(total - cursor > 256 ? 256 : total - cursor);
        size_t i;
        for (i = 0; i < count; ++i) {
            uint8_t *source = resolve_memory(MEMORIES_GUEST_RAM | ((address + (uint32_t)(cursor + i) * 4u) & 0x001ffffcu),
                                             4, resolve_memory_context);
            if (!source) return -1;
            chunk[i] = load32(source);
        }
        if (dma_targets[channel].write(chunk, count, dma_targets[channel].context)) return -1;
        cursor += count;
    }
    dma[channel].madr = (address + (uint32_t)total * 4u) & 0x001ffffcu;
    return 0;
}

static void dma_run(unsigned channel)
{
    int result = 0;
    if (channel == 6) result = dma_run_otc(channel);
    else if (channel == 2) result = dma_run_target(channel);
    if (result) fprintf(stderr, "guest DMA: channel %u transfer could not be completed\n", channel);
    dma_complete(channel);
}

static int dma_read32(uint32_t physical, uint32_t *value)
{
    uint32_t offset = physical - MEMORIES_GUEST_IO_BASE;
    unsigned channel, field;
    if (offset == DMA_DPCR_OFFSET) { *value = dma_dpcr; return 1; }
    if (offset == DMA_DICR_OFFSET) { *value = dma_dicr; return 1; }
    if (!dma_field(physical, &channel, &field)) return 0;
    *value = field == 0 ? dma[channel].madr : field == 4 ? dma[channel].bcr : dma[channel].chcr;
    return 1;
}

static int dma_write32(uint32_t physical, uint32_t value)
{
    uint32_t offset = physical - MEMORIES_GUEST_IO_BASE;
    unsigned channel, field;
    if (offset == DMA_DPCR_OFFSET) {
        dma_dpcr = value;
        sync_dma_globals();
        return 1;
    }
    if (offset == DMA_DICR_OFFSET) {
        dma_dicr = (dma_dicr & 0x7f000000u) | (value & 0x00ffffffu) | (value & 0x80000000u);
        dma_dicr &= ~(value & 0x7f000000u);
        sync_dma_globals();
        return 1;
    }
    if (!dma_field(physical, &channel, &field)) return 0;
    if (field == 0) dma[channel].madr = value & 0x00ffffffu;
    else if (field == 4) dma[channel].bcr = value;
    else {
        dma[channel].chcr = value;
        sync_dma_channel(channel);
        if (value & 0x01000000u) dma_run(channel);
        return 1;
    }
    sync_dma_channel(channel);
    return 1;
}

static void sync_irq_registers(void)
{
    store16(registers + IRQ_STATUS_OFFSET, irq_status);
    store16(registers + IRQ_MASK_OFFSET, irq_mask);
}

static void sync_timer_register(unsigned index)
{
    uint8_t *base = registers + TIMER_BASE_OFFSET + index * TIMER_STRIDE;
    store16(base, timers[index].counter);
    store16(base + 4, timers[index].mode);
    store16(base + 8, timers[index].target);
}

void *Memories_GuestIoResolve(uint32_t address, size_t length)
{
    return at(address, length, "resolve");
}

int Memories_GuestIoEncodePointer(const void *pointer, uint32_t *address)
{
    uintptr_t start = (uintptr_t)registers;
    uintptr_t host = (uintptr_t)pointer;
    if (!address || host < start || host - start > sizeof(registers)) return 0;
    *address = MEMORIES_GUEST_IO_BASE + (uint32_t)(host - start);
    return 1;
}

void Memories_GuestIoReset(void)
{
    memset(registers, 0, sizeof(registers));
    memset(timers, 0, sizeof(timers));
    memset(dma, 0, sizeof(dma));
    irq_status = 0;
    irq_mask = 0;
    dma_dpcr = 0;
    dma_dicr = 0;
}

uint8_t Memories_GuestIoRead8(uint32_t address)
{
    uint8_t *p = at(address, 1, "read8");
    return p ? p[0] : 0;
}

uint16_t Memories_GuestIoRead16(uint32_t address)
{
    uint8_t *p = at(address, 2, "read16");
    uint32_t physical;
    unsigned index, field;
    if (!p || !canonical(address, 2, &physical)) return 0;
    if (physical - MEMORIES_GUEST_IO_BASE == IRQ_STATUS_OFFSET) return irq_status;
    if (physical - MEMORIES_GUEST_IO_BASE == IRQ_MASK_OFFSET) return irq_mask;
    if (timer_field(physical, &index, &field)) {
        if (field == 0) return timers[index].counter;
        if (field == 4) return timers[index].mode;
        return timers[index].target;
    }
    {
        GuestIoDevice *device = find_device(physical, 2);
        if (device && device->read16) {
            uint16_t value = device->read16(physical - MEMORIES_GUEST_IO_BASE, device->context);
            store16(p, value);
            return value;
        }
    }
    return load16(p);
}

uint32_t Memories_GuestIoRead32(uint32_t address)
{
    uint32_t physical, value;
    if (!Memories_GuestIoContains(address, sizeof(uint32_t))) {
        at(address, sizeof(uint32_t), "read32");
        return 0;
    }
    if (canonical(address, sizeof(uint32_t), &physical) && dma_read32(physical, &value)) {
        store32(at(address, sizeof(uint32_t), "read32"), value);
        return value;
    }
    return (uint32_t)Memories_GuestIoRead16(address) |
           (uint32_t)Memories_GuestIoRead16(address + 2u) << 16;
}

void Memories_GuestIoWrite8(uint32_t address, uint8_t value)
{
    uint8_t *p = at(address, 1, "write8");
    if (p) p[0] = value;
}

void Memories_GuestIoWrite16(uint32_t address, uint16_t value)
{
    uint8_t *p = at(address, 2, "write16");
    uint32_t physical;
    unsigned index, field;
    uint32_t offset;
    if (!p || !canonical(address, 2, &physical)) return;
    offset = physical - MEMORIES_GUEST_IO_BASE;
    if (offset == IRQ_STATUS_OFFSET) {
        irq_status &= (uint16_t)~value;
        sync_irq_registers();
        return;
    }
    if (offset == IRQ_MASK_OFFSET) {
        irq_mask = value;
        sync_irq_registers();
        return;
    }
    if (timer_field(physical, &index, &field)) {
        if (field == 0) timers[index].counter = value;
        else if (field == 4) {
            timers[index].mode = value;
            if (value & 0x0002u) timers[index].counter = 0;
        } else timers[index].target = value;
        sync_timer_register(index);
        return;
    }
    {
        GuestIoDevice *device = find_device(physical, 2);
        if (device && device->write16)
            device->write16(physical - MEMORIES_GUEST_IO_BASE, value, device->context);
    }
    store16(p, value);
}

void Memories_GuestIoWrite32(uint32_t address, uint32_t value)
{
    uint32_t physical;
    if (!Memories_GuestIoContains(address, sizeof(uint32_t))) {
        at(address, sizeof(uint32_t), "write32");
        return;
    }
    if (canonical(address, sizeof(uint32_t), &physical) && dma_write32(physical, value)) {
        return;
    }
    Memories_GuestIoWrite16(address, (uint16_t)value);
    Memories_GuestIoWrite16(address + 2u, (uint16_t)(value >> 16));
}

void Memories_GuestIoRaiseIrq(uint16_t mask)
{
    irq_status |= mask;
    sync_irq_registers();
}

void Memories_GuestIoAdvance(uint64_t cycles)
{
    unsigned i;
    if (!cycles) return;
    for (i = 0; i < TIMER_COUNT; ++i) {
        uint64_t limit = timers[i].target ? timers[i].target : 0x10000u;
        uint64_t total = (uint64_t)timers[i].counter + cycles;
        if (timers[i].mode & 0x0002u) {
            timers[i].counter = 0;
            continue;
        }
        if (total >= limit) {
            if (timers[i].mode & 0x0008u) timers[i].counter = (uint16_t)(total % limit);
            else timers[i].counter = (uint16_t)(limit - 1u);
            if (timers[i].mode & 0x0010u)
                Memories_GuestIoRaiseIrq((uint16_t)(0x0010u << i));
        } else {
            timers[i].counter = (uint16_t)total;
        }
        sync_timer_register(i);
    }
}

#ifdef MEMORIES_TRANSLATED
void Memories_GuestIoState(struct MemoriesState *state)
{
    MemoriesStateField fields[] = {
        {registers, sizeof(registers)},
        {timers, sizeof(timers)},
        {dma, sizeof(dma)},
        {&irq_status, sizeof(irq_status)},
        {&irq_mask, sizeof(irq_mask)},
        {&dma_dpcr, sizeof(dma_dpcr)},
        {&dma_dicr, sizeof(dma_dicr)},
    };
    Memories_StateChunk(state, "guest-io", fields, sizeof(fields) / sizeof(fields[0]));
}
#endif
