function [dev_struct, data_struct] = attach_shmat(host_struct)
% Attach host memory and restore all variables stored in host struct (via shared memory based matrix)
% Example:
% a = randn(5);
% b = a + eye(5);
% host = create_shmat(a, b);
% clear('a', 'b'); % remove the existed variable
% [dev, data] = attach_shmat(host); % usually executed in parfor, etc.
% disp(data.a); disp(data.b);
if ~isstruct(host_struct)
    error('Parameter host_struct must be an instance of struct');
end
fields = fieldnames(host_struct);
for i = 1:length(fields)
    arg_dev = host_struct.(fields{i}).attach();
    arg_val = arg_dev.get_data();
    dev_struct.(fields{i}) = arg_dev;
    data_struct.(fields{i}) = arg_val;
end
end
