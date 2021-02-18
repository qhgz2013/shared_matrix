#include "compiler_def.h"

// input arg [1]: shared memory name
// output arg [1]: matlab array (data pointer is attached to shared memory)
// output arg [2]: opened handle
// output arg [3]: base pointer referenced to the entry address
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    MATLAB_PRHS_PTR_CHECK_STRICT(1);

    // SHARED MEMORY NAME CHECK
    char shmem_name[MAX_SHMEM_NAME_LENGTH];
    if (!mxIsChar(prhs[0]) || mxGetString(prhs[0], shmem_name, MAX_SHMEM_NAME_LENGTH))
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Could not get input arg [1]: shared memory name");
    if (strlen(shmem_name) == 0)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Empty shared memory name");
    SHMEM_DEBUG_OUTPUT("Shared memory name: %s\n", shmem_name);

    // OUTPUT ARGUMENT CHECK
    if (nlhs != 3)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidOutput", "read_shared_matrix returns two values: data array, handle");
    unsigned long long* output_handle = NULL;
    unsigned long long* output_pointer = NULL;
    MATLAB_CREATE_UINT64_RETURN_MATRIX(1, output_handle, unsigned long long);
    MATLAB_CREATE_UINT64_RETURN_MATRIX(2, output_pointer, unsigned long long);
    
    // COMMON DEFINES FOR DIFFERENT API
#if SHMEM_API == SHMEM_WIN_API
#    define SHMEM_EXC_CLEANUP_HANDLE { SHMEM_DEBUG_OUTPUT("API call: CloseHandle\n"); CloseHandle(shmem); }
#    define SHMEM_EXC_CLEANUP_PTR(ptr_name,size) { SHMEM_DEBUG_OUTPUT("API call: UnmapViewOfFile\n"); UnmapViewOfFile(ptr_name); }
#    define SHMEM_ATTACH_PTR_FUNC MapViewOfFile
#    define SHMEM_ATTACH_PTR_ARG(size) shmem, FILE_MAP_ALL_ACCESS, 0, 0, size
//#    define SHMEM_ATTACH_PTR_NAME "WIN API MapViewOfFile"
#    define SHMEM_ATTACH_PTR_ERRNO GetLastError()
#    define SHMEM_API_FAILURE_COND NULL
#    define SHMEM_API_STR  "WIN"
#elif SHMEM_API == SHMEM_POSIX_API
#    define SHMEM_EXC_CLEANUP_HANDLE { SHMEM_DEBUG_OUTPUT("API call: close\n"); close(shmem); SHMEM_DEBUG_OUTPUT("API call: shm_unlink\n"); shm_unlink(shmem_name); }
#    define SHMEM_EXC_CLEANUP_PTR(ptr_name,size) { SHMEM_DEBUG_OUTPUT("API call: munmap\n"); munmap(ptr_name, size); }
#    define SHMEM_ATTACH_PTR_FUNC mmap
#    define SHMEM_ATTACH_PTR_ARG(size) 0, size, PROT_READ|PROT_WRITE, MAP_SHARED, shmem, 0
//#    define SHMEM_ATTACH_PTR_NAME "POSIX API mmap"
#    define SHMEM_ATTACH_PTR_ERRNO errno
#    define SHMEM_API_FAILURE_COND MAP_FAILED
#    define SHMEM_API_STR "POSIX"
#endif
#define SHMEM_ATTACH_PTR_FUNC_STR2(x) #x
#define SHMEM_ATTACH_PTR_FUNC_STR(x) SHMEM_ATTACH_PTR_FUNC_STR2(x)
#define SHMEM_EXC_CLEANUP_HANDLE_PTR(ptr_name,size) { SHMEM_EXC_CLEANUP_PTR(ptr_name,size); SHMEM_EXC_CLEANUP_HANDLE; }
#define SHMEM_ATTACH_PTR(ptr_name, size) { \
    SHMEM_DEBUG_OUTPUT("API call: " SHMEM_ATTACH_PTR_FUNC_STR(SHMEM_ATTACH_PTR_FUNC) "\n"); \
    ptr_name = SHMEM_ATTACH_PTR_FUNC(SHMEM_ATTACH_PTR_ARG(size)); \
    if (ptr_name == SHMEM_API_FAILURE_COND) { \
        int attach_ptr_errno = SHMEM_ATTACH_PTR_ERRNO; \
        SHMEM_EXC_CLEANUP_HANDLE; \
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", SHMEM_API_STR " API " SHMEM_ATTACH_PTR_FUNC_STR(SHMEM_ATTACH_PTR_FUNC) " call failed: %d", attach_ptr_errno); \
    } \
    SHMEM_DEBUG_OUTPUT("Shared memory pointer: %p\n", ptr_name); \
}

    // HEADER VALIDATION
    void* header_ptr = NULL;
#if SHMEM_API == SHMEM_WIN_API
    SHMEM_DEBUG_OUTPUT("API call: OpenFileMappingA\n");
    HANDLE shmem = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shmem_name);
    if (shmem == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "WIN API OpenFileMappingA failed: %d", GetLastError());
#elif SHMEM_API == SHMEM_POSIX_API
    SHMEM_DEBUG_OUTPUT("API call: shm_open\n");
    int shmem = shm_open(shmem_name, O_RDWR, 0666);
    if (shmem == -1) {
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "POSIX API shm_open failed: %d", errno);
    }
#endif
    SHMEM_ATTACH_PTR(header_ptr, 8);
    if (SHMEM_READ_CAST(unsigned int, header_ptr, 0) != SHMEM_MEMORY_LAYOUT_VERSION) {
        SHMEM_EXC_CLEANUP_HANDLE_PTR(header_ptr, 8);
        mexErrMsgIdAndTxt("SharedMatrix:CorruptMemory", "Read invalid memory layout version");
    }
    unsigned int header_size = SHMEM_READ_CAST(unsigned int, header_ptr, 4);
    SHMEM_DEBUG_OUTPUT("Header size: %d\n", header_size);
    SHMEM_EXC_CLEANUP_PTR(header_ptr, 8);
    if (header_size == 0) {
        SHMEM_EXC_CLEANUP_HANDLE;
        mexErrMsgIdAndTxt("SharedMatrix:CorruptMemory", "Read invalid header size");
    }

    // READ FULL HEADER
    SHMEM_ATTACH_PTR(header_ptr, header_size);
    unsigned long long matrix_type = SHMEM_READ_CAST(unsigned long long, header_ptr, 8);
    SHMEM_DEBUG_OUTPUT("Matrix type: %lld\n", matrix_type);
    unsigned long long array_attribute = SHMEM_READ_CAST(unsigned long long, header_ptr, 16);
    SHMEM_DEBUG_OUTPUT("Matrix flag: %lld\n", array_attribute);
    unsigned long long payload_size = SHMEM_READ_CAST(unsigned long long, header_ptr, 24);
    SHMEM_DEBUG_OUTPUT("Matrix payload size: %lld\n", payload_size);
    unsigned int n_dims = SHMEM_READ_CAST(unsigned int, header_ptr, 32);
    SHMEM_DEBUG_OUTPUT("Matrix dimension: %d\n", n_dims);

    int data_size = 0;
    if (matrix_type == mxINT8_CLASS || matrix_type == mxUINT8_CLASS) data_size = 1;
    else if (matrix_type == mxINT16_CLASS || matrix_type == mxUINT16_CLASS) data_size = 2;
    else if (matrix_type == mxINT32_CLASS || matrix_type == mxUINT32_CLASS || matrix_type == mxSINGLE_CLASS) data_size = 4;
    else if (matrix_type == mxINT64_CLASS || matrix_type == mxUINT64_CLASS || matrix_type == mxDOUBLE_CLASS) data_size = 8;
    if (data_size == 0) {
        SHMEM_EXC_CLEANUP_HANDLE_PTR(header_ptr, header_size);
        mexErrMsgIdAndTxt("SharedMatrix:CorruptMemory", "Read invalid matrix type");
    }

    if (array_attribute & ARRAY_COMPLEX) {
#ifndef SHMEM_COMPLEX_SUPPORTED
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Complex array is not supported before R2018a");
#else
        data_size *= 2;
#endif
    }
    SHMEM_DEBUG_OUTPUT("Data size: %d\n", data_size);
    
    mwSize static_dims[MAX_STATIC_ALLOCATED_DIMS]; // pre-allocated stack space
    mwSize* dims = (n_dims <= MAX_STATIC_ALLOCATED_DIMS) ? static_dims : (mwSize*)malloc(sizeof(mwSize)*n_dims);
    if (dims == NULL) {
        SHMEM_EXC_CLEANUP_HANDLE_PTR(header_ptr, header_size);
        mexErrMsgIdAndTxt("SharedMatrix:OutOfMemory", "Malloc failed to allocate new memory");
    }
    for (int i = 0; i < n_dims; i++)
        dims[i] = (mwSize)SHMEM_READ_CAST(unsigned long long, header_ptr, 36+i*8);
    unsigned long long total_size = header_size + payload_size;
    unsigned long long nzmax = 0;
    if (array_attribute & ARRAY_SPARSE) {
        nzmax = SHMEM_READ_CAST(unsigned long long, header_ptr, 36+n_dims*8);
        SHMEM_DEBUG_OUTPUT("Nzmax: %lld\n", nzmax);
    }
    SHMEM_EXC_CLEANUP_PTR(header_ptr, header_size);

    // CREATE RETURN MATLAB ARRAY
    mxArray* output_array = NULL;
    int complex_flag = (array_attribute & ARRAY_COMPLEX) ? mxCOMPLEX : mxREAL;
    if (array_attribute & ARRAY_SPARSE) {
        // sparse array
        if (n_dims != 2) {
            SHMEM_EXC_CLEANUP_HANDLE;
            if (n_dims > MAX_STATIC_ALLOCATED_DIMS) free(dims);
            mexErrMsgIdAndTxt("SharedMatrix:DimensionError", "Sparse matrix only supports 2 dimensions");
        }
        output_array = mxCreateSparse(dims[0], dims[1], nzmax, complex_flag);
    }
    else {
        output_array = mxCreateNumericArray((mwSize)n_dims, dims, (int)matrix_type, complex_flag);
    }
    if (output_array == NULL) {
        SHMEM_EXC_CLEANUP_HANDLE;
        if (n_dims > MAX_STATIC_ALLOCATED_DIMS) free(dims);
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Failed to call Matlab mex API: mxCreateNumericArray");
    }
    SHMEM_DEBUG_OUTPUT("Output mxArray created: %p\n", output_array);

    // READ FULL SHARED MEMORY
    void* ptr = NULL;
    SHMEM_ATTACH_PTR(ptr, total_size);
    char* ptr_pr = ((char*)ptr) + header_size;

    // FREE ORIGINAL ARRAY
    void* original_pr = NULL;
    
    // ATTACH PTR
    SHMEM_DEBUG_OUTPUT("Set pr: %p\n", ptr_pr + ARRAY_HEADER_SIZE);
    if (array_attribute & ARRAY_COMPLEX) {
#ifdef SHMEM_COMPLEX_SUPPORTED
        original_pr = get_ic_ptr(output_array, (int)matrix_type);
        set_ic_ptr(output_array, (int)matrix_type, ptr_pr + ARRAY_HEADER_SIZE);
#endif
    }
    else {
        original_pr = mxGetPr(output_array);
        mxSetPr(output_array, (double*)(ptr_pr + ARRAY_HEADER_SIZE));
    }
    SHMEM_DEBUG_OUTPUT("CALL mxSetPr done\n");

    SHMEM_DEBUG_OUTPUT("Original pr: %p\n", original_pr);
    if (original_pr) {
        mxFree(original_pr);
        SHMEM_DEBUG_OUTPUT("CALL mxFree done\n");
    }
    if (array_attribute & ARRAY_SPARSE) {
        unsigned long long ofs_ir = nzmax * data_size + ARRAY_HEADER_SIZE;
        ofs_ir = INT_CEIL(ofs_ir, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
        unsigned long long ofs_jc = ofs_ir + nzmax * sizeof(mwIndex) + ARRAY_HEADER_SIZE;
        ofs_jc = INT_CEIL(ofs_jc, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
        char* ptr_ir = ptr_pr + ofs_ir;
        char* ptr_jc = ptr_pr + ofs_jc;
        void* original_ir = mxGetIr(output_array);
        void* original_jc = mxGetJc(output_array);
        SHMEM_DEBUG_OUTPUT("Original ir: %p\n", original_ir);
        if (original_ir) {
            mxFree(original_ir);
            SHMEM_DEBUG_OUTPUT("CALL mxFree done\n");
        }
        SHMEM_DEBUG_OUTPUT("Original jc: %p\n", original_jc);
        if (original_jc) {
            mxFree(original_jc);
            SHMEM_DEBUG_OUTPUT("CALL mxFree done\n");
        }
        SHMEM_DEBUG_OUTPUT("Set ir: %p\n", ptr_ir + ARRAY_HEADER_SIZE);
        mxSetIr(output_array, (mwIndex*)(ptr_ir + ARRAY_HEADER_SIZE));
        SHMEM_DEBUG_OUTPUT("CALL mxSetIr done\n");
        SHMEM_DEBUG_OUTPUT("Set jc: %p\n", ptr_jc + ARRAY_HEADER_SIZE);
        mxSetJc(output_array, (mwIndex*)(ptr_jc + ARRAY_HEADER_SIZE));
        SHMEM_DEBUG_OUTPUT("CALL mxSetJc done\n");
    }

    plhs[0] = output_array;
    *output_handle = (unsigned long long)shmem;
    *output_pointer = (unsigned long long)ptr;
}
