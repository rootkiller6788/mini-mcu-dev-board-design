/*
 * flash_manager.h -- Flash Memory Management for MCU Bootloader
 */

#ifndef FLASH_MANAGER_H
#define FLASH_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t flash_base;
    uint32_t flash_size;
    uint32_t sector_size;
    uint32_t page_size;
    uint32_t block_size;
    uint32_t word_size;
    uint32_t max_pe_cycles;
} flash_geometry_t;

typedef enum {
    FLASH_OK                   = 0,
    FLASH_ERR_ADDRESS          = -1,
    FLASH_ERR_ALIGNMENT        = -2,
    FLASH_ERR_RANGE            = -3,
    FLASH_ERR_ERASE            = -4,
    FLASH_ERR_PROGRAM          = -5,
    FLASH_ERR_VERIFY           = -6,
    FLASH_ERR_LOCKED           = -7,
    FLASH_ERR_BUSY             = -8,
    FLASH_ERR_WORN_OUT         = -9,
    FLASH_ERR_WRITE_PROTECTED  = -10
} flash_result_t;

typedef enum {
    PARTITION_BOOTLOADER       = 0,
    PARTITION_APP_PRIMARY      = 1,
    PARTITION_APP_SECONDARY    = 2,
    PARTITION_SCRATCH          = 3,
    PARTITION_SWAP_STATUS      = 4,
    PARTITION_NV_STORAGE       = 5,
    PARTITION_FACTORY_RESET    = 6,
    PARTITION_USER_DATA        = 7
} partition_type_t;

typedef struct {
    partition_type_t type;
    uint32_t         start_addr;
    uint32_t         size;
    uint32_t         sector_offset;
    uint32_t         num_sectors;
    char             name[16];
    uint8_t          active;
    uint8_t          write_protected;
} flash_partition_t;

typedef enum {
    SWAP_STATE_NONE            = 0,
    SWAP_STATE_READY           = 1,
    SWAP_STATE_MOVING_TO_SCRATCH = 2,
    SWAP_STATE_MOVING_B_TO_A   = 3,
    SWAP_STATE_MOVING_SCRATCH_TO_B = 4,
    SWAP_STATE_DONE            = 5,
    SWAP_STATE_FAILED          = 6
} swap_state_t;

typedef struct {
    uint8_t      magic[4];
    uint32_t     image_size;
    uint8_t      swap_state;
    uint8_t      swap_type;
    uint8_t      source_slot;
    uint8_t      target_slot;
    uint32_t     image_crc32;
    uint32_t     sector_index;
    uint32_t     offset;
} swap_status_t;

typedef struct {
    uint32_t sector_addr;
    uint32_t erase_count;
    uint32_t last_erase_time;
    uint8_t  health;
} wear_entry_t;

#define MAX_PARTITIONS   16
#define MAX_WEAR_ENTRIES 256

typedef struct {
    flash_geometry_t   geometry;
    flash_partition_t  partitions[MAX_PARTITIONS];
    uint8_t            num_partitions;
    wear_entry_t       wear_table[MAX_WEAR_ENTRIES];
    uint32_t           num_wear_entries;
    swap_status_t      swap_status;
    uint32_t           bootloader_end;
    uint8_t            initialized;
} flash_manager_t;

void flash_mgr_init(flash_manager_t *mgr, const flash_geometry_t *geometry);
flash_result_t flash_erase_sector(flash_manager_t *mgr, uint32_t addr);
flash_result_t flash_erase_range(flash_manager_t *mgr, uint32_t start, uint32_t size);
flash_result_t flash_program_word(flash_manager_t *mgr, uint32_t addr, uint32_t data);
flash_result_t flash_program_page(flash_manager_t *mgr, uint32_t addr, const uint8_t *data, uint32_t len);
flash_result_t flash_program_buffer(flash_manager_t *mgr, uint32_t addr, const uint8_t *data, uint32_t len);
flash_result_t flash_verify(flash_manager_t *mgr, uint32_t addr, const uint8_t *data, uint32_t len);
bool flash_is_erased(flash_manager_t *mgr, uint32_t addr, uint32_t len);
int flash_add_partition(flash_manager_t *mgr, int type, uint32_t start, uint32_t size, const char *name);
const flash_partition_t *flash_find_partition(const flash_manager_t *mgr, int type);
int flash_is_in_partition(const flash_partition_t *part, uint32_t addr, uint32_t size);
flash_result_t flash_swap_begin(flash_manager_t *mgr, uint32_t img_size, uint8_t swap_type, uint8_t src, uint8_t dst);
flash_result_t flash_swap_continue(flash_manager_t *mgr);
bool flash_swap_is_complete(const flash_manager_t *mgr);
flash_result_t flash_swap_rollback(flash_manager_t *mgr);
void flash_wear_update(flash_manager_t *mgr, uint32_t addr);
uint32_t flash_wear_get_count(const flash_manager_t *mgr, uint32_t addr);
int flash_wear_find_best_sector(const flash_manager_t *mgr, uint32_t start, uint32_t size);
uint8_t flash_wear_health(const flash_manager_t *mgr, uint32_t addr);
bool flash_lock_sector(flash_manager_t *mgr, uint32_t addr);
bool flash_unlock_sector(flash_manager_t *mgr, uint32_t addr);

#endif /* FLASH_MANAGER_H */
