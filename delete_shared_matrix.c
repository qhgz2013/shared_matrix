#include "compiler_def.h"

// input arg [1]: opened handle to release (windows) / name of shared memory (posix)
// input arg [2]: base pointer of the shared memory
// input arg [3]: matlab cell containing array created from shared memory (not required for host memory)
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    if (nlhs != 0)
        mexErrMsgIdAndTxt("SharedMatrix:TooManyOutput", "delete_shared_matrix does not accept any output");
    if (nrhs < 2 || nrhs > 3)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Expected 2 or 3 input arguments, but got %d", nrhs);
    MATLAB_PRHS_PTR_CHECK(2);
    if (nrhs == 3) {
        const mxArray* cell = prhs[2];
        if (cell == NULL)
            mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Got null object pointer from input argument");
        if (!mxIsCell(cell))
            mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Input argument [3] must be a cell");
        if (mxGetM(cell) * mxGetN(cell) > 0) {
            mxArray* data_array = mxGetCell(cell, 0);
            if (data_array == NULL)
                mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null array object pointer");
            // set dimension
            const mwSize zero_dims[] = { 0, 0 };
            mxSetDimensions(data_array, zero_dims, 2);
            // detach array
            mxSetPr(data_array, NULL);
        }
    }

    unsigned long long* ptr_base = (unsigned long long*)mxGetPr(prhs[1]);
    if (ptr_base == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null data pointer from non-null array object");
    if ((*ptr_base) == 0)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Pointer address is assigned to zero");
    SHMEM_DEBUG_OUTPUT("Base pointer: %p\n", *ptr_base);
    // release shared memory
#if SHMEM_API == SHMEM_WIN_API
    unsigned long long* ptr_handle = (unsigned long long*)mxGetPr(prhs[0]);
    if (ptr_handle == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null data pointer from non-null array object");
    HANDLE handle = (HANDLE)*ptr_handle;
    SHMEM_DEBUG_OUTPUT("handle: %d\n", handle);
    UnmapViewOfFile((void*)*ptr_base);
    CloseHandle(handle);
#elif SHMEM_API == SHMEM_POSIX_API
    char shmem_name[MAX_SHMEM_NAME_LENGTH];
    if (!mxIsChar(prhs[0]) || mxGetString(prhs[0], shmem_name, MAX_SHMEM_NAME_LENGTH))
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Could not get input arg [1]: shared memory name");
    if (strlen(shmem_name) == 0)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Empty shared memory name");
    unsigned int header_size = SHMEM_READ_CAST(unsigned int, ptr_base, 4);
    unsigned long long payload_size = SHMEM_READ_CAST(unsigned long long, ptr_base, 24);
    unsigned long long total_size = payload_size + header_size;
    munmap(ptr_base, total_size);
    shm_unlink(shmem_name);
#endif
}
