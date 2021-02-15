#include "compiler_def.h"

// input arg [1]: shared memory name
// input arg [2]: input array
// output arg [1]: base pointer of shared memory
// output arg [2]: shared memory handle (optional, required in win api)
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    MATLAB_PRHS_PTR_CHECK_STRICT(2);

    // SHARED MEMORY NAME CHECK
    char shmem_name[MAX_SHMEM_NAME_LENGTH];
    if (!mxIsChar(prhs[0]) || mxGetString(prhs[0], shmem_name, MAX_SHMEM_NAME_LENGTH))
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Could not get input arg [1]: shared memory name");
    if (strlen(shmem_name) == 0)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Empty shared memory name");
    SHMEM_DEBUG_OUTPUT("Shared memory name: %s\n", shmem_name);
    
    // OUTPUT ARGUMENT CHECK
    unsigned long long* base_pointer = NULL;
    unsigned long long* output_value = NULL;
    if (nlhs == 1 || nlhs == 2) {
#if SHMEM_API == SHMEM_WIN_API
        if (nlhs == 1)
            mexErrMsgIdAndTxt("SharedMatrix:NotEnoughOutput", "Win API based shared matrix needs to return a handle of the memory");
#endif
        MATLAB_CREATE_UINT64_RETURN_MATRIX(0, base_pointer, unsigned long long);
        if (nlhs == 2)
            MATLAB_CREATE_UINT64_RETURN_MATRIX(1, output_value, unsigned long long);
    }
    else {
        mexErrMsgIdAndTxt("SharedMatrix:InvalidOutput", "Too many output, max output: 2");
    }

    // ARRAY ATTRIBUTE CHECK
    unsigned long long array_attribute = 0;
    if (mxIsSparse(prhs[1])) {
        array_attribute |= ARRAY_SPARSE;
        SHMEM_DEBUG_OUTPUT("Attribute flag: ARRAY_SPARSE\n");
        mexErrMsgIdAndTxt("SharedMatrix:NotImplemented", "Sparse matrix is not implemented yet");
    }
    if (mxIsComplex(prhs[1])) {
        array_attribute |= ARRAY_COMPLEX;
        SHMEM_DEBUG_OUTPUT("Attribute flag: ARRAY_COMPLEX\n");
        mexErrMsgIdAndTxt("SharedMatrix:NotImplemented", "Complex data is not implemented yet");
    }
    if (!mxIsNumeric(prhs[1])) {
        // TODO [prior: normal]: support logical type
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Only supports numeric data");
    }
    
    // DATA TYPE CHECK
    int data_size = 0;
    int data_class = mxUNKNOWN_CLASS;
    if (mxIsInt8(prhs[1])) { data_size = 1; data_class = mxINT8_CLASS; }
    else if (mxIsInt16(prhs[1])) { data_size = 2; data_class = mxINT16_CLASS; }
    else if (mxIsInt32(prhs[1])) { data_size = 4; data_class = mxINT32_CLASS; }
    else if (mxIsInt64(prhs[1])) { data_size = 8; data_class = mxINT64_CLASS; }
    else if (mxIsUint8(prhs[1])) { data_size = 1; data_class = mxUINT8_CLASS; }
    else if (mxIsUint16(prhs[1])) { data_size = 2; data_class = mxUINT16_CLASS; }
    else if (mxIsUint32(prhs[1])) { data_size = 4; data_class = mxUINT32_CLASS; }
    else if (mxIsUint64(prhs[1])) { data_size = 8; data_class = mxUINT64_CLASS; }
    else if (mxIsSingle(prhs[1])) { data_size = 4; data_class = mxSINGLE_CLASS; }
    else if (mxIsDouble(prhs[1])) { data_size = 8; data_class = mxDOUBLE_CLASS; }
    else { mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Unsupported data type"); };
    SHMEM_DEBUG_OUTPUT("Data size: %d\n", data_size);
    
    // DIMENSION CHECK
    mwSize n_dims = mxGetNumberOfDimensions(prhs[1]);
    SHMEM_DEBUG_OUTPUT("Numbers of dimensions: %d\n", n_dims);
    if (n_dims == 0)
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Zero dimension is unsupported");
    const mwSize* dims = mxGetDimensions(prhs[1]);
    SHMEM_DEBUG_OUTPUT("Dimensions: %d", dims[0]); for (int i = 1; i < n_dims; i++) SHMEM_DEBUG_OUTPUT(" * %d", dims[i]); SHMEM_DEBUG_OUTPUT("\n");
    
    // COMPUTE REQUIRED BYTES
    unsigned long long payload_size = data_size;
    for (int i = 0; i < n_dims; i++)
        payload_size *= dims[i];
    payload_size += ARRAY_HEADER_SIZE;
    unsigned long long header_size = 36; // LAYOUT_VERSION, HEADER_SIZE, MATRIX_TYPE, MATRIX_FLAG, PAYLOAD_SIZE, N_MATRIX_DIMENSION
    header_size += n_dims * 8; // uint64 * N_MATRIX_DIMENSION
    header_size = INT_CEIL(header_size, SHMEM_HEADER_PADDED_BYTES) * SHMEM_HEADER_PADDED_BYTES; // padded
    SHMEM_DEBUG_OUTPUT("Header size: %d\n", header_size);
    SHMEM_DEBUG_OUTPUT("Payload size: %lld\n", payload_size);
    unsigned long long total_size = header_size + payload_size;
    
    // CREATE SHARED MEMORY
#if SHMEM_API == SHMEM_WIN_API
    HANDLE shmem = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)((total_size >> 32) & 0xffffffff), (DWORD)(total_size & 0xffffffff), shmem_name);
    if (shmem == NULL) {
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "WIN API CreateFileMappingA failed: %d", GetLastError());
    }
    void* ptr = MapViewOfFile(shmem, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
    if (ptr == NULL) {
        int map_vof_err = GetLastError();
        CloseHandle(shmem);
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "WIN API MapViewOfFile failed: %d", map_vof_err);
    }
#elif SHMEM_API == SHMEM_POSIX_API
    int shmem = shm_open(shmem_name, O_CREAT | O_RDWR, 0666);
    if (shmem == -1) {
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "POSIX API shm_open failed: %d", errno);
    }
    int trunc_result = ftruncate(shmem, total_size);
    if (trunc_result == -1) {
        int trunc_errno = errno;
        shm_unlink(shmem_name);
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "POSIX API ftruncate failed: %d", trunc_errno);
    }
    void* ptr = mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmem, 0);
    if (ptr == MAP_FAILED) {
        int map_errno = errno;
        shm_unlink(shmem_name);
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", "POSIX API mmap failed: %d", map_errno);
    }
#endif
    
    // MEMORY COPY
    // HEADER
    SHMEM_WRITE_CAST(unsigned int, ptr, 0, SHMEM_MEMORY_LAYOUT_VERSION); // LAYOUT_VERSION
    SHMEM_WRITE_CAST(unsigned int, ptr, 4, header_size); // HEADER_SIZE
    SHMEM_WRITE_CAST(unsigned long long, ptr, 8, data_class); // MATRIX_TYPE
    SHMEM_WRITE_CAST(unsigned long long, ptr, 16, 0); // MATRIX_FLAG
    SHMEM_WRITE_CAST(unsigned long long, ptr, 24, payload_size); // PAYLOAD_SIZE
    SHMEM_WRITE_CAST(unsigned int, ptr, 32, n_dims); // N_MATRIX_DIMENSION
    for (int i = 0; i < n_dims; i++)
        SHMEM_WRITE_CAST(unsigned long long, ptr, 36+i*8, dims[i]);
    // PAYLOAD
    const char* src_ptr = (const char*)mxGetPr(prhs[1]);
    char* dst_ptr = ((char*)ptr) + header_size;
    if (src_ptr == NULL) {
#if SHMEM_API == SHMEM_WIN_API
        UnmapViewOfFile(ptr);
        CloseHandle(shmem);
#elif SHMEM_API == SHMEM_POSIX_API
        munmap(ptr, total_size);
        shm_unlink(shmem_name);
#endif
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null pointer from non-empty array");
    }
    src_ptr = src_ptr - ARRAY_HEADER_SIZE;
    memcpy(dst_ptr, src_ptr, total_size);

    *base_pointer = (unsigned long long)ptr;
    if (output_value)
        *output_value = (unsigned long long)shmem;
}
