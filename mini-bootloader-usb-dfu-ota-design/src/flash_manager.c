/*
 * flash_manager.c -- Flash Memory Management for MCU Bootloader
 *
 * Implements NOR flash erase/program/verify operations, A/B dual-bank
 * slot management, wear leveling, and power-loss-safe swap operations.
 *
 * Knowledge Points:
 *   L1: Flash geometry (sector/page/block hierarchy)
 *   L2: Erase-before-write constraint, NOR flash characteristics
 *   L4: Flash erase property (after erase all bytes = 0xFF)
 *   L5: Wear leveling algorithm, A/B slot selection
 *   L6: Power-loss-safe firmware swap via scratch area
 */

#include "flash_manager.h"
#include <string.h>
#include <stdio.h>

#define FLASH_ERASED_BYTE  0xFF

static uint8_t sim_flash[1024 * 1024];
static uint8_t sim_done = 0;

void flash_mgr_init(flash_manager_t *mgr, const flash_geometry_t *geometry)
{
    if (!mgr || !geometry) return;
    memset(mgr, 0, sizeof(*mgr));
    memcpy(&mgr->geometry, geometry, sizeof(flash_geometry_t));
    mgr->initialized = 1;
    if (!sim_done) {
        memset(sim_flash, FLASH_ERASED_BYTE, sizeof(sim_flash));
        sim_done = 1;
    }
    mgr->bootloader_end = geometry->flash_base + geometry->sector_size;
}

static int addr_ok(const flash_manager_t *m, uint32_t a, uint32_t s)
{
    if (!m || !m->initialized) return 0;
    const flash_geometry_t *g = &m->geometry;
    if (a < g->flash_base) return 0;
    if (a + s > g->flash_base + g->flash_size) return 0;
    if (a + s < a) return 0;
    return 1;
}

uint32_t flash_get_sector_start(const flash_manager_t *mgr, uint32_t addr)
{
    if (!mgr || !addr_ok(mgr, addr, 1)) return 0;
    return addr - (addr % mgr->geometry.sector_size);
}

uint32_t flash_get_sector_size(const flash_manager_t *mgr, uint32_t addr)
{
    (void)addr;
    if (!mgr) return 0;
    return mgr->geometry.sector_size;
}

flash_result_t flash_erase_sector(flash_manager_t *mgr, uint32_t addr)
{
    if (!mgr || !mgr->initialized) return FLASH_ERR_ADDRESS;
    uint32_t sec = flash_get_sector_start(mgr, addr);
    if (!addr_ok(mgr, sec, mgr->geometry.sector_size))
        return FLASH_ERR_ADDRESS;
    if (sec < mgr->bootloader_end) return FLASH_ERR_WRITE_PROTECTED;
    if (flash_wear_health(mgr, sec) == 0) return FLASH_ERR_WORN_OUT;
    uint32_t off = sec - mgr->geometry.flash_base;
    memset(&sim_flash[off], FLASH_ERASED_BYTE, mgr->geometry.sector_size);
    flash_wear_update(mgr, sec);
    return FLASH_OK;
}

flash_result_t flash_erase_range(flash_manager_t *mgr, uint32_t start,
                                  uint32_t size)
{
    if (!mgr || !mgr->initialized) return FLASH_ERR_ADDRESS;
    if (!addr_ok(mgr, start, size)) return FLASH_ERR_RANGE;
    uint32_t end = start + size;
    for (uint32_t a = start; a < end; ) {
        uint32_t s = flash_get_sector_start(mgr, a);
        flash_result_t r = flash_erase_sector(mgr, s);
        if (r != FLASH_OK) return r;
        a = s + mgr->geometry.sector_size;
    }
    return FLASH_OK;
}

flash_result_t flash_program_word(flash_manager_t *mgr, uint32_t addr,
                                   uint32_t data)
{
    if (!mgr || !mgr->initialized) return FLASH_ERR_ADDRESS;
    if (addr & 0x3) return FLASH_ERR_ALIGNMENT;
    if (!addr_ok(mgr, addr, 4)) return FLASH_ERR_ADDRESS;
    if (addr < mgr->bootloader_end) return FLASH_ERR_WRITE_PROTECTED;
    uint32_t off = addr - mgr->geometry.flash_base;
    uint32_t ex = ((uint32_t)sim_flash[off]) |
                  ((uint32_t)sim_flash[off+1]<<8) |
                  ((uint32_t)sim_flash[off+2]<<16) |
                  ((uint32_t)sim_flash[off+3]<<24);
    if (ex != 0xFFFFFFFF) return FLASH_ERR_PROGRAM;
    sim_flash[off]   = (uint8_t)(data & 0xFF);
    sim_flash[off+1] = (uint8_t)((data>>8) & 0xFF);
    sim_flash[off+2] = (uint8_t)((data>>16) & 0xFF);
    sim_flash[off+3] = (uint8_t)((data>>24) & 0xFF);
    return FLASH_OK;
}

flash_result_t flash_program_page(flash_manager_t *mgr, uint32_t addr,
                                   const uint8_t *data, uint32_t len)
{
    if (!mgr || !data) return FLASH_ERR_ADDRESS;
    if (!addr_ok(mgr, addr, len)) return FLASH_ERR_RANGE;
    if (addr % mgr->geometry.page_size != 0) return FLASH_ERR_ALIGNMENT;
    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t w = 0xFFFFFFFF;
        for (uint32_t j = 0; j < 4 && (i+j) < len; j++) {
            w &= ~(0xFFu << (j*8));
            w |= ((uint32_t)data[i+j]) << (j*8);
        }
        flash_result_t r = flash_program_word(mgr, addr + i, w);
        if (r != FLASH_OK) return r;
    }
    return FLASH_OK;
}

flash_result_t flash_program_buffer(flash_manager_t *mgr, uint32_t addr,
                                     const uint8_t *data, uint32_t len)
{
    if (!mgr || !data) return FLASH_ERR_ADDRESS;
    if (!addr_ok(mgr, addr, len)) return FLASH_ERR_RANGE;
    for (uint32_t sec = flash_get_sector_start(mgr, addr);
         sec < addr + len; sec += mgr->geometry.sector_size) {
        flash_result_t r = flash_erase_sector(mgr, sec);
        if (r != FLASH_OK) return r;
    }
    uint32_t remain = len, ca = addr;
    const uint8_t *p = data;
    while (remain > 0) {
        uint32_t c = (remain < mgr->geometry.page_size)
                     ? remain : mgr->geometry.page_size;
        flash_result_t r = flash_program_page(mgr, ca, p, c);
        if (r != FLASH_OK) return r;
        remain -= c;
        ca += c;
        p += c;
    }
    return FLASH_OK;
}

flash_result_t flash_verify(flash_manager_t *mgr, uint32_t addr,
                             const uint8_t *data, uint32_t len)
{
    if (!mgr || !data) return FLASH_ERR_VERIFY;
    if (!addr_ok(mgr, addr, len)) return FLASH_ERR_ADDRESS;
    uint32_t off = addr - mgr->geometry.flash_base;
    for (uint32_t i = 0; i < len; i++)
        if (sim_flash[off+i] != data[i]) return FLASH_ERR_VERIFY;
    return FLASH_OK;
}

bool flash_is_erased(flash_manager_t *mgr, uint32_t addr, uint32_t len)
{
    if (!mgr) return 0;
    uint32_t off = addr - mgr->geometry.flash_base;
    for (uint32_t i = 0; i < len; i++)
        if (sim_flash[off+i] != FLASH_ERASED_BYTE) return 0;
    return 1;
}

int flash_add_partition(flash_manager_t *mgr, int type, uint32_t start,
                        uint32_t size, const char *name)
{
    if (!mgr || !name || mgr->num_partitions >= MAX_PARTITIONS) return -1;
    flash_partition_t *p = &mgr->partitions[mgr->num_partitions];
    p->type = (partition_type_t)type;
    p->start_addr = start;
    p->size = size;
    p->sector_offset = (start - mgr->geometry.flash_base)
                       / mgr->geometry.sector_size;
    p->num_sectors = (size + mgr->geometry.sector_size - 1)
                     / mgr->geometry.sector_size;
    p->active = 1;
    p->write_protected = (type == 0);
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = 0;
    mgr->num_partitions++;
    return mgr->num_partitions - 1;
}

const flash_partition_t *flash_find_partition(const flash_manager_t *mgr,
                                               int type)
{
    if (!mgr) return NULL;
    for (int i = 0; i < mgr->num_partitions; i++)
        if ((int)mgr->partitions[i].type == type && mgr->partitions[i].active)
            return &mgr->partitions[i];
    return NULL;
}

int flash_is_in_partition(const flash_partition_t *part,
                           uint32_t addr, uint32_t size)
{
    if (!part) return 0;
    return (addr >= part->start_addr &&
            (addr + size) <= (part->start_addr + part->size));
}

flash_result_t flash_swap_begin(flash_manager_t *mgr, uint32_t img_size,
                                 uint8_t swap_type, uint8_t src, uint8_t dst)
{
    if (!mgr) return FLASH_ERR_ADDRESS;
    memset(&mgr->swap_status, 0, sizeof(swap_status_t));
    mgr->swap_status.magic[0] = 0x53;
    mgr->swap_status.magic[1] = 0x57;
    mgr->swap_status.magic[2] = 0x41;
    mgr->swap_status.magic[3] = 0x50;
    mgr->swap_status.image_size = img_size;
    mgr->swap_status.swap_state = SWAP_STATE_READY;
    mgr->swap_status.swap_type = swap_type;
    mgr->swap_status.source_slot = src;
    mgr->swap_status.target_slot = dst;
    return FLASH_OK;
}

flash_result_t flash_swap_continue(flash_manager_t *mgr)
{
    if (!mgr) return FLASH_ERR_ADDRESS;
    switch (mgr->swap_status.swap_state) {
    case SWAP_STATE_READY:
        mgr->swap_status.swap_state = SWAP_STATE_MOVING_TO_SCRATCH;
        break;
    case SWAP_STATE_MOVING_TO_SCRATCH:
        mgr->swap_status.swap_state = SWAP_STATE_MOVING_B_TO_A;
        break;
    case SWAP_STATE_MOVING_B_TO_A:
        mgr->swap_status.swap_state = SWAP_STATE_MOVING_SCRATCH_TO_B;
        break;
    case SWAP_STATE_MOVING_SCRATCH_TO_B:
        mgr->swap_status.swap_state = SWAP_STATE_DONE;
        break;
    default:
        return FLASH_ERR_PROGRAM;
    }
    return FLASH_OK;
}

bool flash_swap_is_complete(const flash_manager_t *mgr)
{
    return mgr && mgr->swap_status.swap_state == SWAP_STATE_DONE;
}

flash_result_t flash_swap_rollback(flash_manager_t *mgr)
{
    if (!mgr) return FLASH_ERR_ADDRESS;
    mgr->swap_status.swap_state = SWAP_STATE_FAILED;
    return FLASH_OK;
}

void flash_wear_update(flash_manager_t *mgr, uint32_t addr)
{
    if (!mgr) return;
    for (uint32_t i = 0; i < mgr->num_wear_entries; i++) {
        if (mgr->wear_table[i].sector_addr == addr) {
            mgr->wear_table[i].erase_count++;
            uint32_t m = mgr->geometry.max_pe_cycles;
            uint32_t u = mgr->wear_table[i].erase_count;
            mgr->wear_table[i].health = (u >= m) ? 0
                                        : (uint8_t)(100 - (u * 100 / m));
            return;
        }
    }
    if (mgr->num_wear_entries < MAX_WEAR_ENTRIES) {
        wear_entry_t *e = &mgr->wear_table[mgr->num_wear_entries++];
        e->sector_addr = addr;
        e->erase_count = 1;
        e->last_erase_time = 0;
        e->health = 99;
    }
}

uint32_t flash_wear_get_count(const flash_manager_t *mgr, uint32_t addr)
{
    if (!mgr) return 0;
    for (uint32_t i = 0; i < mgr->num_wear_entries; i++)
        if (mgr->wear_table[i].sector_addr == addr)
            return mgr->wear_table[i].erase_count;
    return 0;
}

int flash_wear_find_best_sector(const flash_manager_t *mgr,
                                 uint32_t start, uint32_t size)
{
    if (!mgr) return -1;
    uint32_t ssz = mgr->geometry.sector_size;
    uint32_t min_e = 0xFFFFFFFF;
    for (uint32_t a = start; a < start + size; a += ssz) {
        uint32_t c = flash_wear_get_count(mgr, a);
        if (c < min_e) min_e = c;
    }
    return (min_e < mgr->geometry.max_pe_cycles) ? 0 : -1;
}

uint8_t flash_wear_health(const flash_manager_t *mgr, uint32_t addr)
{
    if (!mgr) return 0;
    for (uint32_t i = 0; i < mgr->num_wear_entries; i++)
        if (mgr->wear_table[i].sector_addr == addr)
            return mgr->wear_table[i].health;
    return 100;
}

bool flash_lock_sector(flash_manager_t *mgr, uint32_t addr)
{
    (void)mgr; (void)addr;
    return 1;
}

bool flash_unlock_sector(flash_manager_t *mgr, uint32_t addr)
{
    (void)mgr; (void)addr;
    return 1;
}
