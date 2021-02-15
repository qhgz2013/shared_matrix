#define SUPPRESS_NOT_SUPPORTED_ERROR
#include "compiler_def.h"

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    if (nlhs > 1)
        mexErrMsgIdAndTxt("SharedMatrix:TooManyOutput", "test_platform only supports 0 or 1 return value");
    if (nrhs != 0)
        mexErrMsgIdAndTxt("SharedMatrix:TooManyInput", "test_platform does not accept any arguments");
    if (nlhs == 0)
        return;
    mxArray* new_arr = mxCreateNumericMatrix(1, 1, mxINT32_CLASS, mxREAL);
    int* value = (int*)mxGetPr(new_arr);
#ifndef SHMEM_API
    *value = 0;
#else
    *value = (int)(SHMEM_API);
#endif
    plhs[0] = new_arr;
}
