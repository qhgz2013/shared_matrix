% test small matrix
a = randn(5) * 10;
abs_a = abs(a);
sum_a = sum(abs_a(:));
host = shared_matrix_host(abs_a);
% test direct memory access
dev = host.attach();
b = dev.get_data();
if any(size(b) ~= [5, 5])
    error('Size incorrect');
end
sum_b = sum(b(:));
dev.detach();
if any(size(b) ~= [0, 0])
    error('Memory detach failed');
end
host.detach();
if abs(sum_a - sum_b) > 1e-5
    error('Data incorrect');
end
sparse_a = sparse(abs_a);
host = shared_matrix_host(sparse_a);
dev = host.attach();
b = dev.get_data();
if any(size(b) ~= [5, 5])
    error('Size incorrect');
end
sum_b = full(sum(b(:)));
dev.detach();
if any(size(b) ~= [0, 0])
    error('Memory detach failed');
end
host.detach();
if abs(sum_a - sum_b) > 1e-5
    error('Data incorrect');
end
clear
