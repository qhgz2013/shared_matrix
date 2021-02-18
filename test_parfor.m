for i = 1:100
    disp(['Iteration ' num2str(i)]);
    v = randn(4096, 5);
    v2 = randn(4096, 5);
    host = shared_matrix_host(v);
    host2 = shared_matrix_host(v2);
    sum_v = sum(v(:) + v2(:));
    sum_b = zeros(5,1);
    parfor j = 1:5
        dev = host.attach();
        dev2 = host2.attach();
        b = dev.get_data();
        b2 = dev2.get_data();
        sum_b(j) = sum(b(:,j) + b2(:,j));
        dev.detach();
        dev2.detach();
%         disp(dev);
%         disp(dev2);
    end
    sum_b = sum(sum_b);
    if abs(sum_v - sum_b) > 1e-5
        error('Data corrupt');
    end
    host.detach();
    host2.detach();
end