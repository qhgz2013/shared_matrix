try
    disp('Compiling test_platform.c');
    mex 'test_platform.c'
    platform = test_platform();
    if platform == 1
        disp('Compiler: MSVC, using WIN API');
        disp('Compiling create_shared_matrix.c');
        mex 'create_shared_matrix.c'
        disp('Compiling delete_shared_matrix.c');
        mex 'delete_shared_matrix.c'
        disp('Compiling read_shared_matrix.c');
        mex 'read_shared_matrix.c'
    elseif platform == 2
        disp('Compiler: GCC, using POSIX API');
        disp('Compiling create_shared_matrix.c');
        mex 'create_shared_matrix.c' -lrt
        disp('Compiling delete_shared_matrix.c');
        mex 'delete_shared_matrix.c' -lrt
        disp('Compiling read_shared_matrix.c');
        mex 'read_shared_matrix.c' -lrt
        
    else
        error('SharedMatrix:NotSupported', 'Underlying supported API not found');
    end
catch ex
    disp(ex);
end
if exist('platform', 'var')
    clear('platform');
end
