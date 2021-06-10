function host_struct = create_shmat(varargin)
% Create a struct which contains the host shared memory for the given arguments
% Example:
% a = randn(5);
% b = a + eye(5);
% host = create_shmat_host(a, b);
% host.a is an instance of shared_matrix_host(a) object, and
% host.b is an instance of shared_matrix_host(b) object
for i = 1:nargin
    arg_name = inputname(i);
    if isempty(arg_name)
        warning('No argument name specified for arg #%d, using "name%d" as argument name', i, i);
        arg_name = sprintf('Name%d', i);
    end
    arg = varargin{i};
    arg_host = shared_matrix_host(arg);
    host_struct.(arg_name) = arg_host;
end
end
