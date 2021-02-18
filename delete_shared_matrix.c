#include "compiler_def.h"

// input arg [1]: opened handle to release
// input arg [2]: base pointer of the shared memory
// input arg [3]: matlab cell containing array created from shared memory (not required for host memory)
// input arg [4]: shared memory name (required in POSIX API)
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    if (nlhs != 0)
        mexErrMsgIdAndTxt("SharedMatrix:TooManyOutput", "delete_shared_matrix does not accept any output");
    MATLAB_PRHS_PTR_CHECK_STRICT(4);
    bool throw_error_not_supported = false;

    // address containing base ptr
    unsigned long long* ptr_base = (unsigned long long*)mxGetPr(prhs[1]);
    if (ptr_base == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null data pointer from non-null array object");
    ptr_base = (unsigned long long*)*ptr_base;
    if (ptr_base == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Pointer address is assigned to zero");
    SHMEM_DEBUG_OUTPUT("Base pointer: %p\n", ptr_base);

    const mxArray* cell = prhs[2];
    if (cell == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Got null object pointer from input argument");
    if (!mxIsCell(cell) && !mxIsEmpty(cell)) // skips empty mxArray
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Input argument [3] must be a cell");
    if (mxGetM(cell) * mxGetN(cell) > 0) {
        // needs to replace matlab mxArray pointer

        // check header
        unsigned long long matrix_type = SHMEM_READ_CAST(unsigned long long, ptr_base, 8);
        unsigned long long array_attribute = SHMEM_READ_CAST(unsigned long long, ptr_base, 16);
        SHMEM_DEBUG_OUTPUT("Matrix type: %lld\n", matrix_type);
        SHMEM_DEBUG_OUTPUT("Matrix flag: %lld\n", array_attribute);

        mxArray* data_array = mxGetCell(cell, 0);
        if (data_array == NULL)
            mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null array object pointer");
        // set dimension
        const mwSize zero_dims[] = { 0, 0 };
        mxSetDimensions(data_array, zero_dims, 2);

        if (array_attribute & ARRAY_SPARSE) {
            // sparse array
            mxSetNzmax(data_array, 0);
            SHMEM_DEBUG_OUTPUT("CALL mxSetNzmax done\n");
            mxSetIr(data_array, NULL);
            SHMEM_DEBUG_OUTPUT("CALL mxSetIr done\n");
            mxSetJc(data_array, NULL);
            SHMEM_DEBUG_OUTPUT("CALL mxSetJc done\n");
        }

        // detach array
        if (array_attribute & ARRAY_COMPLEX) {
#ifndef SHMEM_COMPLEX_SUPPORTED
            // no op will be performed here, throw the exception after releasing shared memory (probably matlab will crash in the future)
            throw_error_not_supported = true;
#else
            set_ic_ptr(data_array, (int)matrix_type, NULL);
#endif
        }
        else {
            mxSetPr(data_array, NULL);
        }
        SHMEM_DEBUG_OUTPUT("CALL mxSetPr done\n");
    }


    // release shared memory
    unsigned long long* ptr_handle = (unsigned long long*)mxGetPr(prhs[0]);
    if (ptr_handle == NULL)
        mexErrMsgIdAndTxt("SharedMatrix:MatlabError", "Got null data pointer from non-null array object");
    SHMEM_DEBUG_OUTPUT("Handle: %lld\n", *ptr_handle);
#if SHMEM_API == SHMEM_WIN_API
    HANDLE handle = (HANDLE)*ptr_handle;
    SHMEM_DEBUG_OUTPUT("API call: UnmapViewOfFile\n");
    UnmapViewOfFile(ptr_base);
    SHMEM_DEBUG_OUTPUT("API call: CloseHandle\n");
    CloseHandle(handle);
#elif SHMEM_API == SHMEM_POSIX_API
    int handle = (int)*ptr_handle;
    char shmem_name[MAX_SHMEM_NAME_LENGTH] = "";
    if (!mxIsEmpty(prhs[3])) {
        if (!mxIsChar(prhs[3]) || mxGetString(prhs[3], shmem_name, MAX_SHMEM_NAME_LENGTH))
            mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Could not get input arg [4]: shared memory name");
        if (strlen(shmem_name) == 0)
            mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Empty shared memory name");
        SHMEM_DEBUG_OUTPUT("Shared memory name: %s\n", shmem_name);
    }
    unsigned int header_size = SHMEM_READ_CAST(unsigned int, ptr_base, 4);
    unsigned long long payload_size = SHMEM_READ_CAST(unsigned long long, ptr_base, 24);
    unsigned long long total_size = payload_size + header_size;
    SHMEM_DEBUG_OUTPUT("Total size: %lld\n", total_size);
    SHMEM_DEBUG_OUTPUT("API call: munmap\n");
    munmap(ptr_base, total_size);
    SHMEM_DEBUG_OUTPUT("API call: close\n");
    close(handle);
    if (*shmem_name) {
        SHMEM_DEBUG_OUTPUT("API call: shm_unlink\n");
        shm_unlink(shmem_name);
    }
#endif
    if (throw_error_not_supported)
        mexErrMsgIdAndTxt("SharedMatrix:NotSupported", "Complex array is not supported before R2018a, god bless matlab will not be crashed");
}
