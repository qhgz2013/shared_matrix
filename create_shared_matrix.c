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
    }
    if (mxIsComplex(prhs[1])) {
#ifndef SHMEM_COMPLEX_SUPPORTED
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Complex array is not supported before R2018a");
#else
        array_attribute |= ARRAY_COMPLEX;
        SHMEM_DEBUG_OUTPUT("Attribute flag: ARRAY_COMPLEX\n");
        mexErrMsgIdAndTxt("SharedMatrix:NotImplemented", "Complex data is not implemented yet");
#endif
    }
    if (!mxIsNumeric(prhs[1])) {
        // TODO [prior: normal]: support logical type
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Only supports numeric data");
    }
    
    // DATA TYPE CHECK
    int data_size = 0;
    int data_class = mxUNKNOWN_CLASS;
    unsigned long long n_elements = 0;
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
    SHMEM_DEBUG_OUTPUT("Data size: %d, data class: %d\n", data_size, data_class);
    
    // DIMENSION CHECK
    mwSize n_dims = mxGetNumberOfDimensions(prhs[1]);
    SHMEM_DEBUG_OUTPUT("Numbers of dimensions: %d\n", n_dims);
    if (n_dims == 0)
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Zero dimension is unsupported");
    const mwSize* dims = mxGetDimensions(prhs[1]);
    SHMEM_DEBUG_OUTPUT("Dimensions: %d", dims[0]); for (int i = 1; i < n_dims; i++) SHMEM_DEBUG_OUTPUT(" * %d", dims[i]); SHMEM_DEBUG_OUTPUT("\n");
    if ((array_attribute & ARRAY_SPARSE) && n_dims != 2)
        mexErrMsgIdAndTxt("SharedMatrix:DimensionError", "Sparse matrix only supports 2 dimensions");
    
    // COMPUTE REQUIRED BYTES
    unsigned long long payload_size = 0;
    unsigned int header_size = 36 + n_dims * 8;
    if (array_attribute & ARRAY_SPARSE) {
        // sparse array
        n_elements = (unsigned long long)mxGetNzmax(prhs[1]);
        SHMEM_DEBUG_OUTPUT("Nzmax: %lld\n", n_elements);
        payload_size = n_elements * data_size + ARRAY_HEADER_SIZE; // Pr, Pi
        payload_size = INT_CEIL(payload_size, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES; // padded
        payload_size += n_elements * sizeof(mwIndex) + ARRAY_HEADER_SIZE; // Ir
        payload_size = INT_CEIL(payload_size, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES; // padded
        payload_size += n_elements * sizeof(mwIndex) + ARRAY_HEADER_SIZE; // Jc
        header_size += 8; // extra header for NZ_MAX
    }
    else {
        // dense array
        n_elements = 1;
        for (int i = 0; i < n_dims; i++)
            n_elements *= dims[i];
        payload_size = ARRAY_HEADER_SIZE + n_elements * data_size;
    }
    unsigned int header_size_padded = INT_CEIL(header_size, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES; // padded
    unsigned long long payload_size_padded = INT_CEIL(payload_size, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
    SHMEM_DEBUG_OUTPUT("Header size: %d (padded: %d)\n", header_size, header_size_padded);
    SHMEM_DEBUG_OUTPUT("Payload size: %lld (padded: %lld)\n", payload_size, payload_size_padded);
    unsigned long long total_size = header_size_padded + payload_size_padded;
    SHMEM_DEBUG_OUTPUT("Total size: %lld\n", total_size);
    
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
    SHMEM_WRITE_CAST(unsigned int, ptr, 4, header_size_padded); // HEADER_SIZE
    SHMEM_WRITE_CAST(unsigned long long, ptr, 8, data_class); // MATRIX_TYPE
    SHMEM_WRITE_CAST(unsigned long long, ptr, 16, array_attribute); // MATRIX_FLAG
    SHMEM_WRITE_CAST(unsigned long long, ptr, 24, payload_size_padded); // PAYLOAD_SIZE
    SHMEM_WRITE_CAST(unsigned int, ptr, 32, n_dims); // N_MATRIX_DIMENSION
    for (int i = 0; i < n_dims; i++)
        SHMEM_WRITE_CAST(unsigned long long, ptr, 36+i*8, dims[i]);
    if (array_attribute & ARRAY_SPARSE)
        SHMEM_WRITE_CAST(unsigned long long, ptr, 36+n_dims*8, n_elements);

    // PAYLOAD
    // register exception cleanup
#if SHMEM_API == SHMEM_WIN_API
#define EXC_CLEANUP UnmapViewOfFile(ptr); CloseHandle(shmem)
#elif SHMEM_API == SHMEM_POSIX_API
#define EXC_CLEANUP munmap(ptr, total_size); shm_unlink(shmem_name)
#endif
#define CHECK_PTR(ptr) { if (ptr == NULL) { EXC_CLEANUP; mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null pointer from non-empty array"); } }

    if (array_attribute & ARRAY_COMPLEX) {
        // complex array
        // TODO [prior: normal]: implement it
        if (array_attribute & ARRAY_SPARSE) {
            // sparse complex array
        }
    }
    else {
        // non-complex array
        const char* src_pr = (const char*)mxGetPr(prhs[1]);
        CHECK_PTR(src_pr);
        char* dst_pr = ((char*)ptr) + header_size_padded;
        src_pr = src_pr - ARRAY_HEADER_SIZE;
        if (array_attribute & ARRAY_SPARSE) {
            // sparse non-complex array
            const char* src_ir = (const char*)mxGetIr(prhs[1]);
            CHECK_PTR(src_ir);
            src_ir = src_ir - ARRAY_HEADER_SIZE;
            const char* src_jc = (const char*)mxGetJc(prhs[1]);
            CHECK_PTR(src_jc);
            src_jc = src_jc - ARRAY_HEADER_SIZE;
            unsigned long long ofs_ir = n_elements * data_size + ARRAY_HEADER_SIZE;
            ofs_ir = INT_CEIL(ofs_ir, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
            unsigned long long ofs_jc = ofs_ir + n_elements * sizeof(mwIndex) + ARRAY_HEADER_SIZE;
            ofs_jc = INT_CEIL(ofs_jc, SHMEM_DATA_PADDED_BYTES) * SHMEM_DATA_PADDED_BYTES;
            printf("%d %d\n", ofs_ir, ofs_jc);
            char* dst_ir = dst_pr + ofs_ir;
            char* dst_jc = dst_pr + ofs_jc;
            memcpy(dst_pr, src_pr, n_elements * data_size + ARRAY_HEADER_SIZE);
            memcpy(dst_ir, src_ir, n_elements * sizeof(mwIndex) + ARRAY_HEADER_SIZE);
            memcpy(dst_jc, src_jc, n_elements * sizeof(mwIndex) + ARRAY_HEADER_SIZE);
        }
        else {
            memcpy(dst_pr, src_pr, payload_size);
        }
    }
    *base_pointer = (unsigned long long)ptr;
    if (output_value)
        *output_value = (unsigned long long)shmem;
}
