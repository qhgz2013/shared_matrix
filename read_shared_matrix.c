#include "compiler_def.h"

// input arg [1]: shared memory name
// output arg [1]: matlab array (data pointer is attached to shared memory)
// output arg [2]: opened handle
// output arg [3]: base pointer referenced to the entry address
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    MATLAB_PRHS_PTR_CHECK_STRICT(1);

    // SHARED MEMORY NAME CHECK
    char shmem_name[MAX_SHMEM_NAME_LENGTH];
    if (0 != mxGetString(prhs[0], shmem_name, MAX_SHMEM_NAME_LENGTH))
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Could not get input arg [1]: shared memory name");
    if (strlen(shmem_name) == 0)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Empty shared memory name");
    SHMEM_DEBUG_OUTPUT("Shared memory name: %s\n", shmem_name);

    // OUTPUT ARGUMENT CHECK
    if (nlhs != 3)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidOutput", "read_shared_matrix returns two values: data array, handle");
    plhs[1] = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
    if (plhs[1] == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Failed to call Matlab mex API: mxCreateNumericArray");
    unsigned long long* output_handle = (unsigned long long*)mxGetPr(plhs[1]);
    if (output_handle == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null pointer from non-empty array");
    plhs[2] = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
    if (plhs[2] == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Failed to call Matlab mex API: mxCreateNumericArray");
    unsigned long long* output_pointer = (unsigned long long*)mxGetPr(plhs[2]);
    if (output_pointer == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null pointer from non-empty array");
    
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
#define SHMEM_ATTACH_PTR(ptr_name, size) \
    ptr_name = SHMEM_ATTACH_PTR_STMT(size); \
    if (ptr_name == SHMEM_API_FAILURE_COND) { \
        int attach_ptr_errno = SHMEM_ATTACH_PTR_ERRNO; \
        SHMEM_EXC_CLEANUP_HANDLE; \
        mexErrMsgIdAndTxt("SharedMatrix:NativeAPICallFailed", SHMEM_ATTACH_PTR_NAME "failed: %d", attach_ptr_errno); \
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
        CloseHandle(shmem);
        mexErrMsgIdAndTxt("SharedMatrix:CorruptMemory", "Read invalid header size");
    }

    // READ FULL HEADER
    SHMEM_ATTACH_PTR(header_ptr, header_size);
    unsigned long long matrix_type = SHMEM_READ_CAST(unsigned long long, header_ptr, 8);
    SHMEM_DEBUG_OUTPUT("Matrix type: %lld\n", matrix_type);
    unsigned long long matrix_flag = SHMEM_READ_CAST(unsigned long long, header_ptr, 16);
    SHMEM_DEBUG_OUTPUT("Matrix flag: %lld\n", matrix_flag);
    unsigned long long payload_size = SHMEM_READ_CAST(unsigned long long, header_ptr, 24);
    SHMEM_DEBUG_OUTPUT("Matrix payload size: %lld\n", payload_size);
    unsigned int n_dims = SHMEM_READ_CAST(unsigned int, header_ptr, 32);
    SHMEM_DEBUG_OUTPUT("Matrix dimension: %d\n", n_dims);
    
    mwSize* dims = (mwSize*)malloc(sizeof(mwSize)*n_dims);
    if (dims == NULL) {
        SHMEM_EXC_CLEANUP_HANDLE_PTR(header_ptr, header_size);
        mexErrMsgIdAndTxt("SharedMatrix:OutOfMemory", "Malloc failed to allocate new memory");
    }
    for (int i = 0; i < n_dims; i++)
        dims[i] = (mwSize)SHMEM_READ_CAST(unsigned long long, header_ptr, 36+i*8);
    unsigned long long total_size = header_size + payload_size;
    SHMEM_EXC_CLEANUP_PTR(header_ptr, header_size);

    // CREATE RETURN MATLAB ARRAY
    mxArray* output_array = mxCreateNumericArray((mwSize)n_dims, dims, (int)matrix_type, mxREAL); // only supports real
    if (output_array == NULL) {
        SHMEM_EXC_CLEANUP_HANDLE;
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Failed to call Matlab mex API: mxCreateNumericArray");
    }

    // READ FULL SHARED MEMORY
    // register cleanup variable "dims"
#define SHMEM_EXC_CLEANUP_HANDLE_PTR2(ptr_name,size) SHMEM_EXC_CLEANUP_HANDLE_PTR(ptr_name,size); free(dims)
    void* ptr = NULL;
    SHMEM_ATTACH_PTR(ptr, total_size);
    char* payload_ptr = ((char*)ptr) + header_size + ARRAY_HEADER_SIZE;

    // FREE ORIGINAL ARRAY
    void* original_ptr = mxGetPr(output_array);
    if (original_ptr)
        mxFree(original_ptr);
    
    // ATTACH PTR
    mxSetPr(output_array, (double*)payload_ptr);

    plhs[0] = output_array;
    *output_handle = (unsigned long long)shmem;
    *output_pointer = (unsigned long long)ptr;
}
