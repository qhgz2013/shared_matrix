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
#    define SHMEM_EXC_CLEANUP_HANDLE CloseHandle(shmem)
#    define SHMEM_EXC_CLEANUP_PTR(ptr_name,size) UnmapViewOfFile(ptr_name)
#    define SHMEM_ATTACH_PTR_STMT(size) MapViewOfFile(shmem, FILE_MAP_ALL_ACCESS, 0, 0, size)
#    define SHMEM_ATTACH_PTR_NAME "WIN API MapViewOfFile"
#    define SHMEM_ATTACH_PTR_ERRNO GetLastError()
#    define SHMEM_API_FAILURE_COND NULL
#elif SHMEM_API == SHMEM_POSIX_API
#    define SHMEM_EXC_CLEANUP_HANDLE shm_unlink(shmem_name)
#    define SHMEM_EXC_CLEANUP_PTR(ptr_name,size) munmap(ptr_name, size)
#    define SHMEM_ATTACH_PTR_STMT(size) mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, shmem, 0)
#    define SHMEM_ATTACH_PTR_NAME "POSIX API mmap"
#    define SHMEM_ATTACH_PTR_ERRNO errno
#    define SHMEM_API_FAILURE_COND MAP_FAILED
#endif
#define SHMEM_EXC_CLEANUP_HANDLE_PTR(ptr_name,size) SHMEM_EXC_CLEANUP_PTR(ptr_name,size); SHMEM_EXC_CLEANUP_HANDLE
#define SHMEM_ATTACH_PTR(ptr_name, size) { \
    ptr_name = SHMEM_ATTACH_PTR_STMT(size); \
    if (ptr_name == SHMEM_API_FAILURE_COND) { \
        int attach_ptr_errno = SHMEM_ATTACH_PTR_ERRNO; \
        SHMEM_EXC_CLEANUP_HANDLE; \
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", SHMEM_ATTACH_PTR_NAME "failed: %d", attach_ptr_errno); \
    } \
}

    // HEADER VALIDATION
    void* header_ptr = NULL;
#if SHMEM_API == SHMEM_WIN_API
    HANDLE shmem = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shmem_name);
    if (shmem == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "WIN API OpenFileMappingA failed: %d", GetLastError());
#elif SHMEM_API == SHMEM_POSIX_API
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
    SHMEM_DEBUG_OUTPUT("Data size: %d\n", data_size);

    if (array_attribute & ARRAY_COMPLEX) {
#ifndef SHMEM_COMPLEX_SUPPORTED
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Complex array is not supported before R2018a");
#endif
    }
    
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
    SHMEM_DEBUG_OUTPUT("Got shared memory pointer: %p\n", ptr);
    char* ptr_pr = ((char*)ptr) + header_size;

    // FREE ORIGINAL ARRAY
    void* original_ptr = mxGetPr(output_array);
    
    // ATTACH PTR
    if (array_attribute & ARRAY_COMPLEX) {
        SHMEM_EXC_CLEANUP_HANDLE_PTR(ptr, total_size);
        mexErrMsgIdAndTxt("SharedMatrix:NotImplemented", "Complex data is not implemented yet");
    }
    else {
        mxSetPr(output_array, (double*)(ptr_pr + ARRAY_HEADER_SIZE));
        SHMEM_DEBUG_OUTPUT("CALL mxSetPr done\n");
    }
    if (original_ptr) {
        mxFree(original_ptr);
        SHMEM_DEBUG_OUTPUT("CALL mxFree done\n");
    }
    if (array_attribute & ARRAY_SPARSE) {
        unsigned long long ofs_ir = nzmax * data_size + ARRAY_HEADER_SIZE;
        ofs_ir = INT_CEIL(ofs_ir, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
        unsigned long long ofs_jc = ofs_ir + nzmax * sizeof(mwIndex) + ARRAY_HEADER_SIZE;
        ofs_jc = INT_CEIL(ofs_jc, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
            printf("%d %d\n", ofs_ir, ofs_jc);
        char* ptr_ir = ptr_pr + ofs_ir;
        char* ptr_jc = ptr_pr + ofs_jc;
        mxSetIr(output_array, (mwIndex*)(ptr_ir + ARRAY_HEADER_SIZE));
        SHMEM_DEBUG_OUTPUT("CALL mxSetIr done\n");
        mxSetJc(output_array, (mwIndex*)(ptr_jc + ARRAY_HEADER_SIZE));
        SHMEM_DEBUG_OUTPUT("CALL mxSetJc done\n");
    }

    plhs[0] = output_array;
    *output_handle = (unsigned long long)shmem;
    *output_pointer = (unsigned long long)ptr;
}
