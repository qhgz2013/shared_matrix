/*
 * Shared matrix in Matlab
 * Author: Xuebin Zhou
 * License: GNU GPLv3
 */

/*
 * MEMORY LAYOUT documentation V1
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
 * 
 * (unused memory padded to SHMEM_HEADER_PADDED_BYTES bytes)
 * 
 * [ P A Y L O A D ]
 * (byte*16) (optional) ARRAY_HEADER, Matlab matrix header (required in Linux R2017b)
 * (byte*N) ARRAY_DATA, real data payload
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
// Header will be padded to multiple of ? bytes, 16 bytes at least, value must be 2^n
#define SHMEM_HEADER_PADDED_BYTES 16
// Comment the following line to enable runtime output (requires re-compile)
#define NO_DEBUG_OUTPUT
#define SHMEM_MEMORY_LAYOUT_VERSION 1

#ifdef _MSC_VER
// MSVC compiler
#    include <Windows.h>
#    pragma comment(lib, "user32.lib")
#    define SHMEM_API SHMEM_WIN_API
#    define ARRAY_HEADER_SIZE 0
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
#        define ARRAY_HEADER_SIZE 16
#    elif !defined(SUPPRESS_NOT_SUPPORTED_ERROR)
#        error "POSIX feature is unavailable in current GNU C compiler"
#    endif
#else
#    ifndef SUPPRESS_NOT_SUPPORTED_ERROR
#        error "No supported shared memory API"
#    endif
#endif

// MX_HAS_INTERLEAVED_COMPLEX

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
