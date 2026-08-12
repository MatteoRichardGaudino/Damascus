#ifndef __khrplatform_h_
#define __khrplatform_h_

/*
** Copyright (c) 2008-2018 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
*/

#if defined(_WIN32) && !defined(__SCITECH_SNAP__)
#   define KHRONOS_APICALL __declspec(dllimport)
#   define KHRONOS_APIENTRY __stdcall
#   define KHRONOS_APIATTRIBUTES
#elif defined(__ANDROID__)
#   define KHRONOS_APICALL
#   define KHRONOS_APIENTRY
#   define KHRONOS_APIATTRIBUTES __attribute__((visibility("default")))
#else
#   define KHRONOS_APICALL
#   define KHRONOS_APIENTRY
#   define KHRONOS_APIATTRIBUTES
#endif

#include <stdint.h>
#include <stddef.h>

typedef int32_t                 khronos_int32_t;
typedef uint32_t                khronos_uint32_t;
typedef int64_t                 khronos_int64_t;
typedef uint64_t                khronos_uint64_t;
typedef int8_t                  khronos_int8_t;
typedef uint8_t                 khronos_uint8_t;
typedef int16_t                 khronos_int16_t;
typedef uint16_t                khronos_uint16_t;

typedef intptr_t                khronos_intptr_t;
typedef uintptr_t               khronos_uintptr_t;
typedef khronos_intptr_t        khronos_ssize_t;
typedef khronos_uintptr_t       khronos_usize_t;

typedef enum {
    KHRONOS_FALSE = 0,
    KHRONOS_TRUE  = 1,
    KHRONOS_BOOLEAN_ENUM_FORCE_SIZE = 0x7FFFFFFF
} khronos_boolean_enum;

#endif /* __khrplatform_h_ */
