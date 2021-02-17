do_compile

function do_compile
try
    matlab_version = version('-release');
    disp(['Matlab version: ' matlab_version]);
    major_version = int32(str2num(matlab_version(1:end-1))); %#ok<ST2NM>
    disp('Compiling test_platform.c');
    mex 'test_platform.c'
    platform = test_platform();
    compile_files = {'create_shared_matrix.c', 'delete_shared_matrix.c', 'read_shared_matrix.c'};
    wrap_mex = @mex;
    if platform == 1
        disp('Compiler: MSVC, using WIN API');
    elseif platform == 2
        disp('Compiler: GCC, using POSIX API');
        wrap_mex = @(file, varargin) wrap_mex(file, varargin{:}, '-lrt');
    else
        error('SharedMatrix:NotSupported', 'Underlying supported API not found');
    end

    if major_version >= 2018
        wrap_mex = @(file, varargin) wrap_mex(file, varargin{:}, '-R2018a');
    else
        warning('SharedMatrix:FunctionalityUnsupported', 'Current build does not support complex matrix, to get full functionality, please upgrade to R2018a or later')
    end

    for i = 1:length(compile_files)
        disp(['Compiling ' compile_files{i}])
        wrap_mex(compile_files{i});
    end

catch ex
    disp(ex);
end
end
