/*
 * Shared matrix in Matlab
 * Author: Xuebin Zhou
 * License: GNU GPLv3
 */

/*
 * MEMORY LAYOUT documentation V1.0.3
 *
 * <<< SHARED MEMORY POINTER STARTS HERE
 * 
 * [ M A T R I X   H E A D E R ]
 * uint32 LAYOUT_VERSION default: SHMEM_MEMORY_LAYOUT_VERSION, for future compatibility usage
 * uint32 HEADER_SIZE, size (in byte) of the header
 * uint64 MATRIX_TYPE, indicating the matrix type (double, single, int, uint, etc)
 * uint64 MATRIX_FLAG, indicating the matrix is sparse, complex, etc
 * uint64 PAYLOAD_SIZE, size (in byte) of payload
 * uint32 N_MATRIX_DIMENSION, number of dimensions of shared matrix
 * (uint64*N_MATRIX_DIMENSION) MATRIX_DIMENSIONS, size of each dimension
 * uint64 (optional) NZ_MAX, max allocated size for sparse matrix, optional, required when sparse flag is set
 * 
 * (unused memory padded to SHMEM_DATA_PADDED_BYTES bytes)
 * 
 * [ P A Y L O A D ]
 * (byte*16) (optional) ARRAY_HEADER, Matlab matrix header (required in Linux R2017b)
 * (byte*N) ARRAY_DATA, data payload (real / interleaved complex)
 * (unused memory padded to SHMEM_DATA_PADDED_BYTES bytes)
 * 
 * (byte*(NZMAX*sizeof(mwIndex))) (optional) SPARSE_MATRIX_IR, Ir value for sparse matrix
 * (unused memory padded to SHMEM_DATA_PADDED_BYTES bytes)
 * 
 * (byte*(NZMAX*sizeof(mwIndex))) (optional) SPARSE_MATRIX_JC, Jc value for sparse matrix
 * (unused memory padded to SHMEM_DATA_PADDED_BYTES bytes)
 * 
 * >>> END OF SHARED MEMORY
 */
#pragma once
#ifndef _SHARED_MATRIX_COMPILER_DEF_H_
#define _SHARED_MATRIX_COMPILER_DEF_H_

// Matlab MEX-C API Includes
#include <mex.h>
#include <matrix.h>

// STD C Includes
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Supported shared memory APIs
#define SHMEM_WIN_API   0x1
#define SHMEM_POSIX_API 0x2

// Array attributes
#define ARRAY_SPARSE  0x1
#define ARRAY_COMPLEX 0x2

// MODIFIABLE defines
// Maximum string length of shared memory
#define MAX_SHMEM_NAME_LENGTH 64
// All data will be padded to multiple of ? bytes, 16 bytes at least, value must be 2^n
#define SHMEM_DATA_PADDED_BYTES 16
// Comment the following line to enable runtime output (requires re-compile)
#define NO_DEBUG_OUTPUT
// Maximum pre-allocated size for storing MATRIX_DIMENSIONS array, if N_MATRIX_DIMENSION is larger than this value, then a dynamic memory allocation is made
#define MAX_STATIC_ALLOCATED_DIMS 4
// First integer for memory integrity test
#define SHMEM_MEMORY_LAYOUT_VERSION 0x01000300

// Matlab architecture, pass it by -D option
#ifdef ARCH_WIN64
#define ARRAY_HEADER_SIZE 0
#elif defined ARCH_GLNXA64
#define ARRAY_HEADER_SIZE 16
#endif

#ifdef _MSC_VER
// MSVC compiler
#    include <Windows.h>
#    pragma comment(lib, "user32.lib")
#    define SHMEM_API SHMEM_WIN_API
// assumes ARCH_WIN64
#    ifndef ARRAY_HEADER_SIZE
#        define ARRAY_HEADER_SIZE 0
#    endif
#elif defined __GNUC__
// GCC compiler
#    ifdef _POSIX_C_SOURCE
#        include <fcntl.h>
#        include <sys/shm.h>
#        include <sys/stat.h>
#        include <sys/mman.h>
#        include <unistd.h>
#        include <errno.h>
#        define SHMEM_API SHMEM_POSIX_API
// assumes ARCH_GLNXA64
#        ifndef ARRAY_HEADER_SIZE
#            define ARRAY_HEADER_SIZE 16
#        endif
#    elif !defined(SUPPRESS_NOT_SUPPORTED_ERROR)
#        error "POSIX feature is unavailable in current GNU C compiler"
#    endif
#elif defined(__MINGW32__) || defined(__MINGW64__)
// MinGW compiler
#    ifndef SUPPRESS_NOT_SUPPORTED_ERROR
#        error "MinGW compiler is not supported yet"
#    endif
#else
#    ifndef SUPPRESS_NOT_SUPPORTED_ERROR
#        error "No supported shared memory API"
#    endif
#endif

// Interleaved complex functionality (required when accessing complex data)
#if defined(MX_HAS_INTERLEAVED_COMPLEX) && (MX_HAS_INTERLEAVED_COMPLEX == 1)
#define SHMEM_COMPLEX_SUPPORTED
inline void* get_ic_ptr(const mxArray* arr, int cls) {
#define SHMEM_GET_COMPLEX(cls_upper, func) if (cls == mx##cls_upper##_CLASS) return mxGetComplex##func##s(arr)
    SHMEM_GET_COMPLEX(DOUBLE, Double);
    SHMEM_GET_COMPLEX(SINGLE, Single);
    SHMEM_GET_COMPLEX(INT32, Int32);
    SHMEM_GET_COMPLEX(UINT32, Uint32);
    SHMEM_GET_COMPLEX(INT64, Int64);
    SHMEM_GET_COMPLEX(UINT64, Uint64);
    SHMEM_GET_COMPLEX(INT8, Int8);
    SHMEM_GET_COMPLEX(UINT8, Uint8);
    SHMEM_GET_COMPLEX(INT16, Int16);
    SHMEM_GET_COMPLEX(UINT16, Uint16);
    return NULL;
#undef SHMEM_GET_COMPLEX
}
inline void set_ic_ptr(mxArray* arr, int cls, void* ptr) {
#define SHMEM_SET_COMPLEX(cls_upper, func) if (cls == mx##cls_upper##_CLASS) { mxSetComplex##func##s(arr, ( mxComplex##func *)ptr); return; }
    SHMEM_SET_COMPLEX(DOUBLE, Double);
    SHMEM_SET_COMPLEX(SINGLE, Single);
    SHMEM_SET_COMPLEX(INT32, Int32);
    SHMEM_SET_COMPLEX(UINT32, Uint32);
    SHMEM_SET_COMPLEX(INT64, Int64);
    SHMEM_SET_COMPLEX(UINT64, Uint64);
    SHMEM_SET_COMPLEX(INT8, Int8);
    SHMEM_SET_COMPLEX(UINT8, Uint8);
    SHMEM_SET_COMPLEX(INT16, Int16);
    SHMEM_SET_COMPLEX(UINT16, Uint16);
#undef SHMEM_SET_COMPLEX
}

#endif // MX_HAS_INTERLEAVED_COMPLEX

// INT_CEIL(a,b) = (int) ceil( ((double)a) / ((double)b) ), where a and b are positive integers
#define INT_CEIL(a,b) (1+((a)-1)/(b))

#ifndef NO_DEBUG_OUTPUT
#define SHMEM_DEBUG_OUTPUT printf
#else
inline int _empty_printf(const char* fmt, ...) { return 0; }
#define SHMEM_DEBUG_OUTPUT _empty_printf
#endif // NO_DEBUG_OUTPUT

// make read and write cast more elegant (maybe)
#define SHMEM_READ_CAST(dtype,ptr,ofs) (*(dtype*)(((char*)ptr)+(ofs)))
#define SHMEM_WRITE_CAST(dtype,ptr,ofs,value) *(dtype*)(((char*)ptr)+(ofs)) = (dtype)value
// check first n prhs argument is valid or not
#define MATLAB_PRHS_PTR_CHECK(n) { \
    for (int __prhs_chk_i = 0; __prhs_chk_i < (n); __prhs_chk_i++) \
        if (prhs[__prhs_chk_i] == NULL) \
            mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Got null object pointer from input argument"); \
}
// check first n prhs argument is valid or not, plus n must be nrhs
#define MATLAB_PRHS_PTR_CHECK_STRICT(n) { \
    if ((n) != nrhs) \
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Expected "#n " input arguments, but got %d", nrhs); \
    MATLAB_PRHS_PTR_CHECK(n) \
}
// create a 1x1 uint64 matrix to plhs[i], and ptr ref. to the created matrix for reading and writing
#define MATLAB_CREATE_UINT64_RETURN_MATRIX(i,ptr,dtype) { \
    plhs[i] = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL); \
    if (plhs[i] == NULL) \
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Failed to call Matlab mex API: mxCreateNumericArray"); \
    ptr = (dtype*)mxGetPr(plhs[i]); \
    if (ptr == NULL) \
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null pointer from non-empty array"); \
}

#endif
