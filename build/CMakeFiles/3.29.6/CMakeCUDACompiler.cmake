set(CMAKE_CUDA_COMPILER "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/bin/nvcc")
set(CMAKE_CUDA_HOST_COMPILER "")
set(CMAKE_CUDA_HOST_LINK_LAUNCHER "/nix/store/d4x69qskk6312a5k1v94lqxz6r5nphmq-gcc-wrapper-12.4.0/bin/g++")
set(CMAKE_CUDA_COMPILER_ID "NVIDIA")
set(CMAKE_CUDA_COMPILER_VERSION "12.1.105")
set(CMAKE_CUDA_DEVICE_LINKER "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/bin/nvlink")
set(CMAKE_CUDA_FATBINARY "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/bin/fatbinary")
set(CMAKE_CUDA_STANDARD_COMPUTED_DEFAULT "17")
set(CMAKE_CUDA_EXTENSIONS_COMPUTED_DEFAULT "ON")
set(CMAKE_CUDA_COMPILE_FEATURES "cuda_std_03;cuda_std_11;cuda_std_14;cuda_std_17;cuda_std_20")
set(CMAKE_CUDA03_COMPILE_FEATURES "cuda_std_03")
set(CMAKE_CUDA11_COMPILE_FEATURES "cuda_std_11")
set(CMAKE_CUDA14_COMPILE_FEATURES "cuda_std_14")
set(CMAKE_CUDA17_COMPILE_FEATURES "cuda_std_17")
set(CMAKE_CUDA20_COMPILE_FEATURES "cuda_std_20")
set(CMAKE_CUDA23_COMPILE_FEATURES "")

set(CMAKE_CUDA_PLATFORM_ID "Linux")
set(CMAKE_CUDA_SIMULATE_ID "GNU")
set(CMAKE_CUDA_COMPILER_FRONTEND_VARIANT "")
set(CMAKE_CUDA_SIMULATE_VERSION "12.4")



set(CMAKE_CUDA_COMPILER_ENV_VAR "CUDACXX")
set(CMAKE_CUDA_HOST_COMPILER_ENV_VAR "CUDAHOSTCXX")

set(CMAKE_CUDA_COMPILER_LOADED 1)
set(CMAKE_CUDA_COMPILER_ID_RUN 1)
set(CMAKE_CUDA_SOURCE_FILE_EXTENSIONS cu)
set(CMAKE_CUDA_LINKER_PREFERENCE 15)
set(CMAKE_CUDA_LINKER_PREFERENCE_PROPAGATES 1)
set(CMAKE_CUDA_LINKER_DEPFILE_SUPPORTED )

set(CMAKE_CUDA_SIZEOF_DATA_PTR "8")
set(CMAKE_CUDA_COMPILER_ABI "ELF")
set(CMAKE_CUDA_BYTE_ORDER "LITTLE_ENDIAN")
set(CMAKE_CUDA_LIBRARY_ARCHITECTURE "")

if(CMAKE_CUDA_SIZEOF_DATA_PTR)
  set(CMAKE_SIZEOF_VOID_P "${CMAKE_CUDA_SIZEOF_DATA_PTR}")
endif()

if(CMAKE_CUDA_COMPILER_ABI)
  set(CMAKE_INTERNAL_PLATFORM_ABI "${CMAKE_CUDA_COMPILER_ABI}")
endif()

if(CMAKE_CUDA_LIBRARY_ARCHITECTURE)
  set(CMAKE_LIBRARY_ARCHITECTURE "")
endif()

set(CMAKE_CUDA_COMPILER_TOOLKIT_ROOT "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1")
set(CMAKE_CUDA_COMPILER_TOOLKIT_LIBRARY_ROOT "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1")
set(CMAKE_CUDA_COMPILER_TOOLKIT_VERSION "12.1.105")
set(CMAKE_CUDA_COMPILER_LIBRARY_ROOT "/nix/store/blrvxpawbf1myaday4s3zavn6ga2swg5-cuda_nvcc-12.1.105")

set(CMAKE_CUDA_ARCHITECTURES_ALL "50-real;52-real;53-real;60-real;61-real;62-real;70-real;72-real;75-real;80-real;86-real;87-real;89-real;90")
set(CMAKE_CUDA_ARCHITECTURES_ALL_MAJOR "50-real;60-real;70-real;80-real;90")
set(CMAKE_CUDA_ARCHITECTURES_NATIVE "86-real")

set(CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES "/nix/store/blrvxpawbf1myaday4s3zavn6ga2swg5-cuda_nvcc-12.1.105/include")

set(CMAKE_CUDA_HOST_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CUDA_HOST_IMPLICIT_LINK_DIRECTORIES "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/lib64/stubs;/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/lib64-L/nix/store/blrvxpawbf1myaday4s3zavn6ga2swg5-cuda_nvcc-12.1.105/nvvm/lib")
set(CMAKE_CUDA_HOST_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES "")

set(CMAKE_CUDA_IMPLICIT_INCLUDE_DIRECTORIES "/nix/store/blrvxpawbf1myaday4s3zavn6ga2swg5-cuda_nvcc-12.1.105/nvvm/include;/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/include;/nix/store/9w3yyrs3r9samcg2dyyzhny4s0c8nkfs-cuda_cudart-12.1.105-dev/include;/nix/store/4vkj3qipw2rkfrjj3s79w2w8jraf1lyp-compiler-rt-libc-18.1.8-dev/include;/nix/store/kacxs7yfq39sl3sqf3gzksipqhyg3gcp-libcxx-18.1.8-dev/include;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/include/c++/12.4.0;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/include/c++/12.4.0/x86_64-unknown-linux-gnu;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/include/c++/12.4.0/backward;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/lib/gcc/x86_64-unknown-linux-gnu/12.4.0/include;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/include;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/lib/gcc/x86_64-unknown-linux-gnu/12.4.0/include-fixed;/nix/store/09lv9r3dx6ql0lzpdv8w2b1r6b358481-glibc-2.39-52-dev/include")
set(CMAKE_CUDA_IMPLICIT_LINK_LIBRARIES "stdc++;m;gcc_s;gcc;c;gcc_s;gcc")
set(CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES "/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/lib64/stubs;/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/lib64-L/nix/store/blrvxpawbf1myaday4s3zavn6ga2swg5-cuda_nvcc-12.1.105/nvvm/lib;/nix/store/1k0mrm2nih4apz0j1g0p6kk1zcispjq5-cuda-merged-12.1/lib;/nix/store/rsryj6nzbxbxal3y1rprdh7lkk3y5v62-cuda_cudart-12.1.105-lib/lib;/nix/store/vcsjgkzji1ib69kb5p9jhjrc3rnlscln-cuda_cudart-12.1.105-static/lib;/nix/store/gqm3j7jq1ypl6d1w38jq6vidfq6wi8b1-cuda_cudart-12.1.105-stubs/lib;/nix/store/24ivk2gk133skjrnnxy0cvhx3zyjsnrp-nvidia-x11-555.58.02-6.6.41/lib;/nix/store/v61z468b5asvr5pz1q5imp310kf63ipg-libcxx-18.1.8/lib;/nix/store/m71p7f0nymb19yn1dascklyya2i96jfw-glibc-2.39-52/lib;/nix/store/acl6p8xdhzvl0rmk3rrjl5jyd81p31kz-gcc-12.4.0-lib/lib;/nix/store/d4x69qskk6312a5k1v94lqxz6r5nphmq-gcc-wrapper-12.4.0/bin;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/lib/gcc/x86_64-unknown-linux-gnu/12.4.0;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/lib64;/nix/store/8hcsbp5h11lk9wwv5b8sw9kyw1g11zwf-gcc-12.4.0/lib")
set(CMAKE_CUDA_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES "")

set(CMAKE_CUDA_RUNTIME_LIBRARY_DEFAULT "STATIC")

set(CMAKE_LINKER "/nix/store/d4x69qskk6312a5k1v94lqxz6r5nphmq-gcc-wrapper-12.4.0/bin/ld")
set(CMAKE_LINKER_LINK "")
set(CMAKE_LINKER_LLD "")
set(CMAKE_CUDA_COMPILER_LINKER "/nix/store/d4x69qskk6312a5k1v94lqxz6r5nphmq-gcc-wrapper-12.4.0/bin/ld")
set(CMAKE_CUDA_COMPILER_LINKER_ID "GNU")
set(CMAKE_CUDA_COMPILER_LINKER_VERSION 2.42)
set(CMAKE_CUDA_COMPILER_LINKER_FRONTEND_VARIANT GNU)
set(CMAKE_AR "/nix/store/d4x69qskk6312a5k1v94lqxz6r5nphmq-gcc-wrapper-12.4.0/bin/ar")
set(CMAKE_RANLIB "/nix/store/d4x69qskk6312a5k1v94lqxz6r5nphmq-gcc-wrapper-12.4.0/bin/ranlib")
set(CMAKE_MT "")
