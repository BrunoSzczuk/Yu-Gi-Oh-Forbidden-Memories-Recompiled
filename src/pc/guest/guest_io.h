#ifndef MEMORIES_GUEST_IO_H
#define MEMORIES_GUEST_IO_H

#include <stddef.h>
#include <stdint.h>
#include "pc/memory_map.h"

typedef uint16_t (*MemoriesGuestIoRead16)(uint32_t offset, void *context);
typedef void (*MemoriesGuestIoWrite16)(uint32_t offset, uint16_t value, void *context);
typedef void *(*MemoriesGuestIoResolveMemory)(uint32_t address, size_t length, void *context);
typedef int (*MemoriesGuestDmaWrite)(const uint32_t *words, size_t count, void *context);

/* First-stage translated guest bus. Registers are latched with PS1 little
 * endian semantics; device-specific timer, DMA, CD-ROM, GPU and SPU behavior
 * is added behind this boundary instead of allowing host-pointer faults. */
int Memories_GuestIoContains(uint32_t address, size_t length);
int Memories_GuestIoRegisterDevice(uint32_t offset, size_t size,
                                   MemoriesGuestIoRead16 read16,
                                   MemoriesGuestIoWrite16 write16,
                                   void *context);
void Memories_GuestIoSetMemoryResolver(MemoriesGuestIoResolveMemory resolve, void *context);
int Memories_GuestIoRegisterDmaTarget(unsigned channel, MemoriesGuestDmaWrite write, void *context);
void *Memories_GuestIoResolve(uint32_t address, size_t length);
int Memories_GuestIoEncodePointer(const void *pointer, uint32_t *address);
void Memories_GuestIoReset(void);
void Memories_GuestIoAdvance(uint64_t cycles);
void Memories_GuestIoRaiseIrq(uint16_t mask);
uint8_t Memories_GuestIoRead8(uint32_t address);
uint16_t Memories_GuestIoRead16(uint32_t address);
uint32_t Memories_GuestIoRead32(uint32_t address);
void Memories_GuestIoWrite8(uint32_t address, uint8_t value);
void Memories_GuestIoWrite16(uint32_t address, uint16_t value);
void Memories_GuestIoWrite32(uint32_t address, uint32_t value);

#ifdef MEMORIES_TRANSLATED
struct MemoriesState;
void Memories_GuestIoState(struct MemoriesState *state);
#endif

#endif
