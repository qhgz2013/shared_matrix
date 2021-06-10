function detach_shmat(host_or_dev_struct)
% Detach and free host or device memory, all attached variables will set to <missing> (it must be called after executed, otherwise it will cause memory leakage)
% Example:
% a = randn(5);
% b = a + eye(5);
% host = create_shmat(a, b);
% clear('a', 'b'); % remove the existed variable
% dev = attach_shmat(host); % usually executed in parfor, etc.
% disp(data.a); disp(data.b);
% detach_shmat(dev);
% detach_shmat(host);
if ~isstruct(host_or_dev_struct)
    error('Parameter host_or_dev_struct must be an instance of struct');
end
fields = fieldnames(host_or_dev_struct);
for i = 1:length(fields)
    shmat_obj = host_or_dev_struct.(fields{i});
%     if isequal(class(shmat_obj), 'shared_matrix')
%         % remove device-side variables from caller (if exists)
%         if evalin('caller', sprintf('exist(''%s'', ''var'');', fields{i}))
%             % assignin('caller', fields{i}, missing);
%             evalin('caller', sprintf('clear(''%s'');', fields{i}));
%         end
%     end
    shmat_obj.detach();
end
end
