#define SUPPRESS_NOT_SUPPORTED_ERROR
#include "compiler_def.h"

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    printf("%p\n", mxGetIr(prhs[0]));
    printf("%p\n", mxGetJc(prhs[0]));
    printf("%p\n", mxGetPr(prhs[0]));
    //printf("%p\n", mxGetPi(prhs[0]));
}