#define SUPPRESS_NOT_SUPPORTED_ERROR
#include "compiler_def.h"

void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[]) {
    int a = (int)*mxGetPr(prhs[0]);
    printf("%d\n", INT_CEIL(a, 16));
    //printf("%p\n", mxGetPi(prhs[0]));
}