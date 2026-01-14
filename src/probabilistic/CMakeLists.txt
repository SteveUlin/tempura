add_library(skip_list INTERFACE skip_list.h)
target_include_directories(skip_list INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(skip_list INTERFACE tempura_flags)
add_library(tempura::skip_list ALIAS skip_list)

add_library(bloom_filter INTERFACE bloom_filter.h)
target_include_directories(bloom_filter INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(bloom_filter INTERFACE tempura_flags)
add_library(tempura::bloom_filter ALIAS bloom_filter)

add_library(count_min_sketch INTERFACE count_min_sketch.h)
target_include_directories(count_min_sketch INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(count_min_sketch INTERFACE tempura_flags)
add_library(tempura::count_min_sketch ALIAS count_min_sketch)

add_library(hyperloglog INTERFACE hyperloglog.h)
target_include_directories(hyperloglog INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(hyperloglog INTERFACE tempura_flags)
add_library(tempura::hyperloglog ALIAS hyperloglog)

add_library(cuckoo_filter INTERFACE cuckoo_filter.h)
target_include_directories(cuckoo_filter INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(cuckoo_filter INTERFACE tempura_flags)
add_library(tempura::cuckoo_filter ALIAS cuckoo_filter)

add_library(t_digest INTERFACE t_digest.h)
target_include_directories(t_digest INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(t_digest INTERFACE tempura_flags)
add_library(tempura::t_digest ALIAS t_digest)

add_library(reservoir_sampling INTERFACE reservoir_sampling.h)
target_include_directories(reservoir_sampling INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(reservoir_sampling INTERFACE tempura_flags)
add_library(tempura::reservoir_sampling ALIAS reservoir_sampling)

add_library(minhash INTERFACE minhash.h)
target_include_directories(minhash INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(minhash INTERFACE tempura_flags)
add_library(tempura::minhash ALIAS minhash)

add_library(treap INTERFACE treap.h)
target_include_directories(treap INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(treap INTERFACE tempura_flags)
add_library(tempura::treap ALIAS treap)

add_library(lsh INTERFACE lsh.h)
target_include_directories(lsh INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(lsh INTERFACE tempura_flags)
add_library(tempura::lsh ALIAS lsh)

add_library(simhash INTERFACE simhash.h)
target_include_directories(simhash INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(simhash INTERFACE tempura_flags)
add_library(tempura::simhash ALIAS simhash)

add_library(one_permutation_hash INTERFACE one_permutation_hash.h)
target_include_directories(one_permutation_hash INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(one_permutation_hash INTERFACE tempura_flags)
add_library(tempura::one_permutation_hash ALIAS one_permutation_hash)

add_library(weighted_minhash INTERFACE weighted_minhash.h)
target_include_directories(weighted_minhash INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(weighted_minhash INTERFACE tempura_flags)
add_library(tempura::weighted_minhash ALIAS weighted_minhash)

add_library(xor_filter INTERFACE xor_filter.h)
target_include_directories(xor_filter INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(xor_filter INTERFACE tempura_flags)
add_library(tempura::xor_filter ALIAS xor_filter)

add_library(quotient_filter INTERFACE quotient_filter.h)
target_include_directories(quotient_filter INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(quotient_filter INTERFACE tempura_flags)
add_library(tempura::quotient_filter ALIAS quotient_filter)

add_library(bbit_minhash INTERFACE bbit_minhash.h)
target_include_directories(bbit_minhash INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(bbit_minhash INTERFACE tempura_flags)
add_library(tempura::bbit_minhash ALIAS bbit_minhash)

if(TEMPURA_BUILD_TESTS)
  add_executable(skip_list_test skip_list_test.cpp)
  target_link_libraries(skip_list_test unit skip_list)
  add_test(NAME skip_list_test COMMAND skip_list_test)
  set_property(TEST skip_list_test PROPERTY LABELS probabilistic)

  add_executable(bloom_filter_test bloom_filter_test.cpp)
  target_link_libraries(bloom_filter_test unit bloom_filter)
  add_test(NAME bloom_filter_test COMMAND bloom_filter_test)
  set_property(TEST bloom_filter_test PROPERTY LABELS probabilistic)

  add_executable(count_min_sketch_test count_min_sketch_test.cpp)
  target_link_libraries(count_min_sketch_test unit count_min_sketch)
  add_test(NAME count_min_sketch_test COMMAND count_min_sketch_test)
  set_property(TEST count_min_sketch_test PROPERTY LABELS probabilistic)

  add_executable(hyperloglog_test hyperloglog_test.cpp)
  target_link_libraries(hyperloglog_test unit hyperloglog)
  add_test(NAME hyperloglog_test COMMAND hyperloglog_test)
  set_property(TEST hyperloglog_test PROPERTY LABELS probabilistic)

  add_executable(cuckoo_filter_test cuckoo_filter_test.cpp)
  target_link_libraries(cuckoo_filter_test unit cuckoo_filter)
  add_test(NAME cuckoo_filter_test COMMAND cuckoo_filter_test)
  set_property(TEST cuckoo_filter_test PROPERTY LABELS probabilistic)

  add_executable(t_digest_test t_digest_test.cpp)
  target_link_libraries(t_digest_test unit t_digest)
  add_test(NAME t_digest_test COMMAND t_digest_test)
  set_property(TEST t_digest_test PROPERTY LABELS probabilistic)

  add_executable(reservoir_sampling_test reservoir_sampling_test.cpp)
  target_link_libraries(reservoir_sampling_test unit reservoir_sampling)
  add_test(NAME reservoir_sampling_test COMMAND reservoir_sampling_test)
  set_property(TEST reservoir_sampling_test PROPERTY LABELS probabilistic)

  add_executable(minhash_test minhash_test.cpp)
  target_link_libraries(minhash_test unit minhash)
  add_test(NAME minhash_test COMMAND minhash_test)
  set_property(TEST minhash_test PROPERTY LABELS probabilistic)

  add_executable(treap_test treap_test.cpp)
  target_link_libraries(treap_test unit treap)
  add_test(NAME treap_test COMMAND treap_test)
  set_property(TEST treap_test PROPERTY LABELS probabilistic)

  add_executable(lsh_test lsh_test.cpp)
  target_link_libraries(lsh_test unit lsh)
  add_test(NAME lsh_test COMMAND lsh_test)
  set_property(TEST lsh_test PROPERTY LABELS probabilistic)

  add_executable(simhash_test simhash_test.cpp)
  target_link_libraries(simhash_test unit simhash)
  add_test(NAME simhash_test COMMAND simhash_test)
  set_property(TEST simhash_test PROPERTY LABELS probabilistic)

  add_executable(one_permutation_hash_test one_permutation_hash_test.cpp)
  target_link_libraries(one_permutation_hash_test unit one_permutation_hash)
  add_test(NAME one_permutation_hash_test COMMAND one_permutation_hash_test)
  set_property(TEST one_permutation_hash_test PROPERTY LABELS probabilistic)

  add_executable(weighted_minhash_test weighted_minhash_test.cpp)
  target_link_libraries(weighted_minhash_test unit weighted_minhash)
  add_test(NAME weighted_minhash_test COMMAND weighted_minhash_test)
  set_property(TEST weighted_minhash_test PROPERTY LABELS probabilistic)

  add_executable(xor_filter_test xor_filter_test.cpp)
  target_link_libraries(xor_filter_test unit xor_filter bloom_filter)
  add_test(NAME xor_filter_test COMMAND xor_filter_test)
  set_property(TEST xor_filter_test PROPERTY LABELS probabilistic)

  add_executable(quotient_filter_test quotient_filter_test.cpp)
  target_link_libraries(quotient_filter_test unit quotient_filter)
  add_test(NAME quotient_filter_test COMMAND quotient_filter_test)
  set_property(TEST quotient_filter_test PROPERTY LABELS probabilistic)

  add_executable(bbit_minhash_test bbit_minhash_test.cpp)
  target_link_libraries(bbit_minhash_test unit bbit_minhash minhash)
  add_test(NAME bbit_minhash_test COMMAND bbit_minhash_test)
  set_property(TEST bbit_minhash_test PROPERTY LABELS probabilistic)
endif()
