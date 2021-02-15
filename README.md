# Shared Matrix in Matlab

A MEX-C library for creating or reading matrix using shared memory (parpool supports) to reduce unnecessary copies of memory.

(Be care for all the computations, because it may crash Matlab if any improper accesses are made)

# Supported compiler

Windows: MSVC (using WIN API)  
Linux: GCC (using POSIX API)

# Tested environments

Windows: R2018b + VS 2019  
Linux: R2017b + GCC 7.5.0

# Usage

## Compile

running `compile` in Matlab shell

## Example

A full example showing how this works on parallel computing
```matlab
a = randn(32*4096, 4096);  % creating a large (4GB) matrix
host = shared_matrix_host(a);  % put it into shared matrix
sum_a = sum(a(:));  % just for simple verification
clear a  % the original variable can be removed now

sum_vector = zeros(4096, 1);
parfor i = 1:4096
    accessor = host.attach();  % attach this memory to other processes created by parpool
    data_matrix = accessor.get_data();  % gets the real matrix
    % do parallel computation using big data matrix
    % NOTICE: ONLY SUPPORTS READ
    sum_vector(i) = sum(data_matrix(:, i));
    % don't forget to clean it up
    accessor.detach();
end

% detach host memory
host.detach();
sum_b = sum(sum_vector);

% have a comparison
sum_a
sum_b
```

In general, this example only uses 4GB memory (8GB peak when do copying). The traditional method (Matlab provided) requires 4*(n_workers+1) GB memory, where n_workers is the number of CPU cores:

```matlab
a = randn(32*4096, 4096);  % creating a large (4GB) matrix
sum_a = sum(a(:));  % just for simple verification

sum_vector = zeros(4096, 1);
parfor i = 1:4096
    % accessing large matrix within parfor will cost massive memory usage, because every worker keeps a copy of this large matrix
    sum_vector(i) = sum(a(:, i));
end

sum_b = sum(sum_vector);

% have a comparison
sum_a
sum_b
```

## Notice

For Linux users, make sure the usable size of `/dev/shm` is capable for the matrix.

# Citation and license

This repository is licensed under GNU GPLv3, all rights reserved.

If this code is helpful for your research, please consider citing this repository.

```bibtex
@misc{shmem_matlab21,
  author = {Xuebin Zhou},
  title = {Shared Matrix in Matlab},
  url = {https://github.com/qhgz2013/shared_matrix},
  year = {2021}
}
```
