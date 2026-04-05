/**
 * @file   memory_driver.h
 * @brief  Standalone verze memory driveru pro TMZ projekt (bez mz800emu).
 *
 * Poskytuje globalni driver instance pro pametovy handler
 * (staticka a realloc varianta).
 */

#ifndef MEMORY_DRIVER_H
#define MEMORY_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generic_driver.h"

    extern st_DRIVER g_memory_driver_static;
    extern st_DRIVER g_memory_driver_realloc;

    extern void memory_driver_init ( void );

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_DRIVER_H */
