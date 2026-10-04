# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Ahmed ARIF <arif.ing@outlook.com>
#
# Toolchain for the nested Mesa build against the ReactOS SDK headers, the
# ReactOS CRT and import libraries, and the tree's matching libc++ runtime.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR "${MESA_REACTOS_ARCH}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    MESA_REACTOS_ARCH MESA_REACTOS_TRIPLE MESA_REACTOS_ARCH_FLAGS MESA_REACTOS_SOURCE_DIR
    MESA_REACTOS_BUILD_DIR MESA_REACTOS_LIBDIR MESA_REACTOS_CXX_RUNTIME REACTOS_CLANG_LLVM_MINGW_ROOT)

set(CMAKE_C_COMPILER "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/clang")
set(CMAKE_CXX_COMPILER "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/clang++")
set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
set(CMAKE_C_COMPILER_TARGET "${MESA_REACTOS_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "${MESA_REACTOS_TRIPLE}")
set(CMAKE_ASM_COMPILER_TARGET "${MESA_REACTOS_TRIPLE}")
set(CMAKE_AR "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/llvm-ar")
set(CMAKE_RANLIB "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/llvm-ranlib")
set(CMAKE_LINKER "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/ld.lld")
set(CMAKE_RC_COMPILER "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/llvm-windres")
set(CMAKE_RC_FLAGS_INIT "--target=${MESA_REACTOS_TRIPLE}")
set(CMAKE_NM "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/llvm-nm")
set(CMAKE_OBJCOPY "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/llvm-objcopy")
set(CMAKE_STRIP "${REACTOS_CLANG_LLVM_MINGW_ROOT}/bin/llvm-strip")

execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
    OUTPUT_VARIABLE _mesa_clang_resource_dir OUTPUT_STRIP_TRAILING_WHITESPACE)
set(_mesa_sdk "${MESA_REACTOS_SOURCE_DIR}/sdk/include")
set(_mesa_sdk_build "${MESA_REACTOS_BUILD_DIR}/sdk/include")
set(_mesa_sdk_includes
    "-isystem ${_mesa_clang_resource_dir}/include"
    "-isystem ${_mesa_sdk}/ucrt"
    "-isystem ${_mesa_sdk}/vcruntime"
    "-isystem ${_mesa_sdk}/crt"
    "-isystem ${_mesa_sdk}/psdk"
    "-isystem ${_mesa_sdk}"
    "-isystem ${_mesa_sdk}/ddk"
    "-isystem ${_mesa_sdk}/reactos"
    "-isystem ${_mesa_sdk_build}"
    "-isystem ${_mesa_sdk_build}/psdk"
    "-isystem ${_mesa_sdk_build}/ddk")
string(JOIN " " _mesa_sdk_includes ${_mesa_sdk_includes})
set(_mesa_sdk_defines
    "-D_DLL -D_UCRT -D__USE_CRTIMP -D__REACTOS__ -D_CRT_DECLARE_NONSTDC_NAMES=1 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00")
set(_mesa_sdk_codegen
    "${MESA_REACTOS_ARCH_FLAGS} -fsigned-char -funwind-tables -Xclang -fstack-clash-protection")

set(CMAKE_C_FLAGS "${_mesa_sdk_codegen} -nostdlibinc ${_mesa_sdk_defines} ${_mesa_sdk_includes}"
    CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS
    "${_mesa_sdk_codegen} -nostdlibinc -nostdinc++ -isystem ${MESA_REACTOS_CXX_RUNTIME}/include/c++/v1 -D__LARGE_MBSTATE_T ${_mesa_sdk_defines} ${_mesa_sdk_includes}"
    CACHE STRING "C++ flags" FORCE)
set(CMAKE_ASM_FLAGS_INIT "${MESA_REACTOS_ARCH_FLAGS}")

set(_mesa_sdk_crt
    "${MESA_REACTOS_CXX_RUNTIME}/lib/windows/libclang_rt.builtins-${MESA_REACTOS_ARCH}.a"
    "${MESA_REACTOS_LIBDIR}/libucrtbase.a"
    "${MESA_REACTOS_LIBDIR}/libmsvcrtex.a"
    "${MESA_REACTOS_LIBDIR}/libmsvcrt.a"
    "${MESA_REACTOS_LIBDIR}/libkernel32.a"
    "${MESA_REACTOS_LIBDIR}/libntdll.a"
    "${MESA_REACTOS_LIBDIR}/ucrtoldnames.a"
    "${MESA_REACTOS_CXX_RUNTIME}/lib/windows/libclang_rt.builtins-${MESA_REACTOS_ARCH}.a")
string(JOIN " " _mesa_sdk_crt ${_mesa_sdk_crt})
set(CMAKE_C_STANDARD_LIBRARIES "${_mesa_sdk_crt}" CACHE STRING "Standard C libraries" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES
    "${MESA_REACTOS_CXX_RUNTIME}/lib/libc++.a ${MESA_REACTOS_CXX_RUNTIME}/lib/libc++abi.a ${MESA_REACTOS_CXX_RUNTIME}/lib/libunwind.a ${_mesa_sdk_crt}"
    CACHE STRING "Standard C++ libraries" FORCE)

set(_mesa_sdk_link "-nostdlib -fuse-ld=lld -L${MESA_REACTOS_LIBDIR} -Wl,--disable-auto-import,--disable-stdcall-fixup")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_mesa_sdk_link} -Wl,-entry,DllMainCRTStartup")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_mesa_sdk_link} -Wl,-entry,DllMainCRTStartup")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_mesa_sdk_link} -Wl,-entry,mainCRTStartup")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
