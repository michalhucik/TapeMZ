/**
 * @file   memory_driver.c
 * @brief  Standalone implementace memory driveru pro TMZ projekt.
 *
 * Adaptace z mz800emu - poskytuje globalni driver instance
 * pro pametovy handler (staticka a realloc varianta).
 *
 * @par Licence:
 * GNU General Public License v3 (GPLv3)
 *
 * Copyright (C) 2017-2026 Michal Hucik <hucik@ordoz.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "memory_driver.h"
#include "../baseui_compat.h"

st_DRIVER g_memory_driver_static;
st_DRIVER g_memory_driver_realloc;


static int memory_driver_prepare_static_cb ( st_HANDLER *h, uint32_t offset, void **buffer, uint32_t count_bytes ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    if ( EXIT_SUCCESS != generic_driver_memory_operation_internal_bootstrap ( h ) ) return EXIT_FAILURE;
    uint32_t need_size = offset + count_bytes;
    *buffer = NULL;
    if ( offset > memspec->size ) { d->err = GENERIC_DRIVER_ERROR_SEEK; return EXIT_FAILURE; }
    if ( need_size > memspec->size ) { d->err = GENERIC_DRIVER_ERROR_SIZE; return EXIT_FAILURE; }
    *buffer = &memspec->ptr[offset];
    return EXIT_SUCCESS;
}


static int memory_driver_prepare_realloc_cb ( st_HANDLER *h, uint32_t offset, void **buffer, uint32_t count_bytes ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    if ( EXIT_SUCCESS != generic_driver_memory_operation_internal_bootstrap ( h ) ) return EXIT_FAILURE;
    uint32_t need_size = offset + count_bytes;
    *buffer = NULL;
    if ( ( offset > memspec->size ) || ( need_size > memspec->size ) ) {
        if ( h->status & HANDLER_STATUS_READ_ONLY ) { h->err = HANDLER_ERROR_WRITE_PROTECTED; return EXIT_FAILURE; }
        uint8_t *new_ptr = realloc ( memspec->ptr, need_size );
        if ( !new_ptr ) { d->err = GENERIC_DRIVER_ERROR_REALLOC; return EXIT_FAILURE; }
        memspec->ptr = new_ptr;
        memspec->size = need_size;
    }
    *buffer = &memspec->ptr[offset];
    return EXIT_SUCCESS;
}


static int memory_driver_read_cb ( st_HANDLER *h, uint32_t offset, void *buffer, uint32_t count_bytes, uint32_t *readlen ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    *readlen = 0;
    if ( EXIT_SUCCESS != generic_driver_memory_operation_internal_bootstrap ( h ) ) return EXIT_FAILURE;
    if ( offset > memspec->size ) { d->err = GENERIC_DRIVER_ERROR_SEEK; return EXIT_FAILURE; }
    if ( offset + count_bytes > memspec->size ) { d->err = GENERIC_DRIVER_ERROR_SIZE; return EXIT_FAILURE; }
    *readlen = count_bytes;
    if ( &memspec->ptr[offset] != buffer ) memmove ( buffer, &memspec->ptr[offset], count_bytes );
    return EXIT_SUCCESS;
}


static int memory_driver_write_cb ( st_HANDLER *h, uint32_t offset, void *buffer, uint32_t count_bytes, uint32_t *writelen ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    *writelen = 0;
    if ( EXIT_SUCCESS != generic_driver_memory_operation_internal_bootstrap ( h ) ) return EXIT_FAILURE;
    if ( h->status & HANDLER_STATUS_READ_ONLY ) { h->err = HANDLER_ERROR_WRITE_PROTECTED; return EXIT_FAILURE; }
    if ( offset > memspec->size ) { d->err = GENERIC_DRIVER_ERROR_SEEK; return EXIT_FAILURE; }
    if ( offset + count_bytes > memspec->size ) { d->err = GENERIC_DRIVER_ERROR_SIZE; return EXIT_FAILURE; }
    *writelen = count_bytes;
    if ( &memspec->ptr[offset] != buffer ) { memspec->updated = 1; memmove ( &memspec->ptr[offset], buffer, count_bytes ); }
    return EXIT_SUCCESS;
}


static int memory_driver_truncate_cb ( st_HANDLER *h, uint32_t size ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    if ( EXIT_SUCCESS != generic_driver_memory_operation_internal_bootstrap ( h ) ) return EXIT_FAILURE;
    if ( h->status & HANDLER_STATUS_READ_ONLY ) { h->err = HANDLER_ERROR_WRITE_PROTECTED; return EXIT_FAILURE; }
    if ( size < 1 ) { d->err = GENERIC_DRIVER_ERROR_SIZE; return EXIT_FAILURE; }
    uint8_t *new_ptr = realloc ( memspec->ptr, size );
    if ( !new_ptr ) { d->err = GENERIC_DRIVER_ERROR_REALLOC; return EXIT_FAILURE; }
    memspec->ptr = new_ptr;
    memspec->size = size;
    return EXIT_SUCCESS;
}


static int memory_driver_open_cb ( st_HANDLER *h ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    h->err = HANDLER_ERROR_NONE;
    d->err = GENERIC_DRIVER_ERROR_NONE;
    if ( h->status & HANDLER_STATUS_READY ) { d->err = GENERIC_DRIVER_ERROR_HANDLER_IS_BUSY; return EXIT_FAILURE; }
    if ( h->type != HANDLER_TYPE_MEMORY ) { d->err = GENERIC_DRIVER_ERROR_HANDLER_TYPE; return EXIT_FAILURE; }
    if ( memspec->ptr != NULL ) { d->err = GENERIC_DRIVER_ERROR_HANDLER_IS_BUSY; return EXIT_FAILURE; }
    if ( memspec->open_size < 1 ) { d->err = GENERIC_DRIVER_ERROR_SIZE; return EXIT_FAILURE; }
    h->status = HANDLER_STATUS_NOT_READY;
    uint8_t *new_ptr = calloc ( 1, memspec->open_size );
    if ( !new_ptr ) { d->err = GENERIC_DRIVER_ERROR_MALLOC; return EXIT_FAILURE; }
    memspec->ptr = new_ptr;
    memspec->size = memspec->open_size;
    h->status = HANDLER_STATUS_READY;
    return EXIT_SUCCESS;
}


static int memory_driver_close_cb ( st_HANDLER *h ) {
    st_DRIVER *d = (st_DRIVER *) h->driver;
    st_HANDLER_MEMSPC *memspec = &h->spec.memspec;
    if ( EXIT_SUCCESS != generic_driver_memory_operation_internal_bootstrap ( h ) ) return EXIT_FAILURE;
    free ( memspec->ptr );
    memspec->ptr = NULL;
    memspec->size = 0;
    h->err = HANDLER_ERROR_NONE;
    d->err = GENERIC_DRIVER_ERROR_NONE;
    h->status = HANDLER_STATUS_NOT_READY;
    return EXIT_SUCCESS;
}


void memory_driver_init ( void ) {
    generic_driver_setup ( &g_memory_driver_static, memory_driver_open_cb, memory_driver_close_cb, memory_driver_read_cb, memory_driver_write_cb, memory_driver_prepare_static_cb, memory_driver_truncate_cb );
    generic_driver_setup ( &g_memory_driver_realloc, memory_driver_open_cb, memory_driver_close_cb, memory_driver_read_cb, memory_driver_write_cb, memory_driver_prepare_realloc_cb, memory_driver_truncate_cb );
}
