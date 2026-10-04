# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Ahmed ARIF <arif193@gmail.com>

if(ARCH STREQUAL "riscv64")
    option(SPACEMIT_K1_SUPPORT "Build SpacemiT K1 SD card images" OFF)

    if(SPACEMIT_K1_SUPPORT)
        set(SPACEMIT_K1_BOOT_DIR "${CMAKE_CURRENT_LIST_DIR}/spacemit-k1")

        set(SPACEMIT_K1_BOOTINFO "${SPACEMIT_K1_BOOT_DIR}/bootinfo_sd.bin")
        set(SPACEMIT_K1_FSBL "${SPACEMIT_K1_BOOT_DIR}/FSBL.bin")
        set(SPACEMIT_K1_OPENSBI "${SPACEMIT_K1_BOOT_DIR}/fw_dynamic.itb")
        set(SPACEMIT_K1_UBOOT "${SPACEMIT_K1_BOOT_DIR}/u-boot.itb")
        set(SPACEMIT_K1_UBOOT_ENV "${SPACEMIT_K1_BOOT_DIR}/env_k1-x.txt")
        foreach(_k1_file BOOTINFO FSBL OPENSBI UBOOT UBOOT_ENV)
            if(NOT EXISTS "${SPACEMIT_K1_${_k1_file}}")
                message(FATAL_ERROR "SpacemiT K1: missing boot file ${SPACEMIT_K1_${_k1_file}}")
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SPACEMIT_K1_${_k1_file}}")
        endforeach()

        set(SPACEMIT_K1_FSBL_SECTOR 256)
        set(SPACEMIT_K1_FSBL_BACKUP_SECTOR 1024)
        set(SPACEMIT_K1_OPENSBI_START 2048)
        set(SPACEMIT_K1_OPENSBI_SECTORS 2048)
        set(SPACEMIT_K1_UBOOT_START 4096)
        set(SPACEMIT_K1_UBOOT_SECTORS 8192)
        set(SPACEMIT_K1_BOOT_PARTITION_START_MB 8)

        message(STATUS "RISC-V64: SpacemiT K1 SD card layout enabled")
    endif()
endif()
