#include <stdio.h>
#include <mex.h>
#include <windows.h>
#define MAX_SHMEM_NAME_LENGTH 64

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    // SHARED MEMORY NAME CHECK
    char shmem_name[MAX_SHMEM_NAME_LENGTH];
    if (0 != mxGetString(prhs[0], shmem_name, MAX_SHMEM_NAME_LENGTH))
        mexErrMsgIdAndTxt("SharedMatrix:InvalidInput", "Could not get input arg [1]: shared memory name %d", 5);
    printf("%s\n", shmem_name);
    
    TCHAR* tchar_name = (TCHAR*)malloc(sizeof(TCHAR) * (MAX_SHMEM_NAME_LENGTH + 1));
    if (tchar_name == NULL) {
        mexErrMsgIdAndTxt("SharedMatrix:MemoryError", "Failed to allocate new memory");
    }
    swprintf(tchar_name, MAX_SHMEM_NAME_LENGTH, "%hs", shmem_name);
    printf("%d", sizeof(TCHAR));
    free(tchar_name);
}