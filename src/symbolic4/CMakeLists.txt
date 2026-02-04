# ============================================================================
# Symbolic4: AST-as-types with pluggable interpreters
# ============================================================================
#
# Dependencies:
#   - meta: Type metaprogramming for type lists and utilities
#   - tempura_flags: Compiler settings
#
# Key Design: Everything is an Atom<Id, Effect>. Types ARE the representation.
# Strategy-based rewriting and direct recursion for all transforms.
# ============================================================================

add_library(symbolic4 INTERFACE)
target_include_directories(symbolic4 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(symbolic4 INTERFACE tempura::meta tempura_flags)
add_library(tempura::symbolic4 ALIAS symbolic4)

# ============================================================================
# Tests
# ============================================================================

if(NOT TEMPURA_BUILD_TESTS)
  return()
endif()

# compressed.h tests
add_executable(symbolic4_compressed_test compressed_test.cc)
target_link_libraries(symbolic4_compressed_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_compressed_test COMMAND symbolic4_compressed_test)
set_tests_properties(symbolic4_compressed_test PROPERTIES LABELS "symbolic4;compressed")

# core.h tests
add_executable(symbolic4_core_test core_test.cc)
target_link_libraries(symbolic4_core_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_core_test COMMAND symbolic4_core_test)
set_tests_properties(symbolic4_core_test PROPERTIES LABELS "symbolic4;core")

# constants.h tests
add_executable(symbolic4_constants_test constants_test.cc)
target_link_libraries(symbolic4_constants_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_constants_test COMMAND symbolic4_constants_test)
set_tests_properties(symbolic4_constants_test PROPERTIES LABELS "symbolic4;constants")

# operators.h tests
add_executable(symbolic4_operators_test operators_test.cc)
target_link_libraries(symbolic4_operators_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_operators_test COMMAND symbolic4_operators_test)
set_tests_properties(symbolic4_operators_test PROPERTIES LABELS "symbolic4;operators")


# interpreter/eval.h tests
add_executable(symbolic4_eval_test interpreter/eval_test.cc)
target_link_libraries(symbolic4_eval_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_eval_test COMMAND symbolic4_eval_test)
set_tests_properties(symbolic4_eval_test PROPERTIES LABELS "symbolic4;interpreter;eval")

# interpreter/to_string.h tests
add_executable(symbolic4_to_string_test interpreter/to_string_test.cc)
target_link_libraries(symbolic4_to_string_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_to_string_test COMMAND symbolic4_to_string_test)
set_tests_properties(symbolic4_to_string_test PROPERTIES LABELS "symbolic4;interpreter;to_string")

# let.h tests
add_executable(symbolic4_let_test let_test.cc)
target_link_libraries(symbolic4_let_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_let_test COMMAND symbolic4_let_test)
set_tests_properties(symbolic4_let_test PROPERTIES LABELS "symbolic4;let")

# interpreter/simplify.h tests
add_executable(symbolic4_simplify_test interpreter/simplify_test.cc)
target_link_libraries(symbolic4_simplify_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_simplify_test COMMAND symbolic4_simplify_test)
set_tests_properties(symbolic4_simplify_test PROPERTIES LABELS "symbolic4;interpreter;simplify")

# interpreter/partial_eval.h tests
add_executable(symbolic4_partial_eval_test interpreter/partial_eval_test.cc)
target_link_libraries(symbolic4_partial_eval_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_partial_eval_test COMMAND symbolic4_partial_eval_test)
set_tests_properties(symbolic4_partial_eval_test PROPERTIES LABELS "symbolic4;interpreter;partial_eval")

# interpreter/partial_eval_exact.h tests
add_executable(symbolic4_partial_eval_exact_test interpreter/partial_eval_exact_test.cc)
target_link_libraries(symbolic4_partial_eval_exact_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_partial_eval_exact_test COMMAND symbolic4_partial_eval_exact_test)
set_tests_properties(symbolic4_partial_eval_exact_test PROPERTIES LABELS "symbolic4;interpreter;partial_eval_exact")

# distributions/ tests
add_executable(symbolic4_distributions_test distributions_test.cc)
target_link_libraries(symbolic4_distributions_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_distributions_test COMMAND symbolic4_distributions_test)
set_tests_properties(symbolic4_distributions_test PROPERTIES LABELS "symbolic4;distributions")

# indexed/ tests
add_executable(symbolic4_indexed_test indexed_test.cc)
target_link_libraries(symbolic4_indexed_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_indexed_test COMMAND symbolic4_indexed_test)
set_tests_properties(symbolic4_indexed_test PROPERTIES LABELS "symbolic4;indexed")

# observed/ tests
add_executable(symbolic4_observed_test observed_test.cc)
target_link_libraries(symbolic4_observed_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_observed_test COMMAND symbolic4_observed_test)
set_tests_properties(symbolic4_observed_test PROPERTIES LABELS "symbolic4;observed")

# multivariate/ tests
add_executable(symbolic4_multivariate_test multivariate_test.cc)
target_link_libraries(symbolic4_multivariate_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_multivariate_test COMMAND symbolic4_multivariate_test)
set_tests_properties(symbolic4_multivariate_test PROPERTIES LABELS "symbolic4;multivariate")

# distributions/log_prob.h tests
add_executable(symbolic4_log_prob_test distributions/log_prob_test.cc)
target_link_libraries(symbolic4_log_prob_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_log_prob_test COMMAND symbolic4_log_prob_test)
set_tests_properties(symbolic4_log_prob_test PROPERTIES LABELS "symbolic4;distributions;log_prob")

# distributions/wrappers.h tests
add_executable(symbolic4_wrappers_test distributions/wrappers_test.cc)
target_link_libraries(symbolic4_wrappers_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_wrappers_test COMMAND symbolic4_wrappers_test)
set_tests_properties(symbolic4_wrappers_test PROPERTIES LABELS "symbolic4;distributions;wrappers")

# distributions/discrete.h tests
add_executable(symbolic4_discrete_test distributions/discrete_test.cc)
target_link_libraries(symbolic4_discrete_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_discrete_test COMMAND symbolic4_discrete_test)
set_tests_properties(symbolic4_discrete_test PROPERTIES LABELS "symbolic4;distributions;discrete")

# distributions/random_var.h tests
add_executable(symbolic4_random_var_test distributions/random_var_test.cc)
target_link_libraries(symbolic4_random_var_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_random_var_test COMMAND symbolic4_random_var_test)
set_tests_properties(symbolic4_random_var_test PROPERTIES LABELS "symbolic4;distributions;random_var")

# distributions/dirichlet_simplex.h tests
add_executable(symbolic4_dirichlet_simplex_test distributions/dirichlet_simplex_test.cc)
target_link_libraries(symbolic4_dirichlet_simplex_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_dirichlet_simplex_test COMMAND symbolic4_dirichlet_simplex_test)
set_tests_properties(symbolic4_dirichlet_simplex_test PROPERTIES LABELS "symbolic4;distributions;dirichlet_simplex")

# distributions/joint.h tests
add_executable(symbolic4_joint_test distributions/joint_test.cc)
target_link_libraries(symbolic4_joint_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_joint_test COMMAND symbolic4_joint_test)
set_tests_properties(symbolic4_joint_test PROPERTIES LABELS "symbolic4;distributions;joint")

# distributions/sample.h tests
add_executable(symbolic4_sample_test distributions/sample_test.cc)
target_link_libraries(symbolic4_sample_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_sample_test COMMAND symbolic4_sample_test)
set_tests_properties(symbolic4_sample_test PROPERTIES LABELS "symbolic4;distributions;sample")

# distributions/indexed_node.h tests
add_executable(symbolic4_indexed_node_test distributions/indexed_node_test.cc)
target_link_libraries(symbolic4_indexed_node_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_indexed_node_test COMMAND symbolic4_indexed_node_test)
set_tests_properties(symbolic4_indexed_node_test PROPERTIES LABELS "symbolic4;distributions;indexed_node")

# indexed/dim.h tests
add_executable(symbolic4_dim_test indexed/dim_test.cc)
target_link_libraries(symbolic4_dim_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_dim_test COMMAND symbolic4_dim_test)
set_tests_properties(symbolic4_dim_test PROPERTIES LABELS "symbolic4;indexed;dim")

# indexed/sum_over.h tests
add_executable(symbolic4_sum_over_test indexed/sum_over_test.cc)
target_link_libraries(symbolic4_sum_over_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_sum_over_test COMMAND symbolic4_sum_over_test)
set_tests_properties(symbolic4_sum_over_test PROPERTIES LABELS "symbolic4;indexed;sum_over")

# indexed/indexed_eval.h tests
add_executable(symbolic4_indexed_eval_test indexed/indexed_eval_test.cc)
target_link_libraries(symbolic4_indexed_eval_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_indexed_eval_test COMMAND symbolic4_indexed_eval_test)
set_tests_properties(symbolic4_indexed_eval_test PROPERTIES LABELS "symbolic4;indexed;indexed_eval")

# indexed/gather.h tests
add_executable(symbolic4_gather_test indexed/gather_test.cc)
target_link_libraries(symbolic4_gather_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_gather_test COMMAND symbolic4_gather_test)
set_tests_properties(symbolic4_gather_test PROPERTIES LABELS "symbolic4;indexed;gather")

# indexed/gather.h integration tests
add_executable(symbolic4_gather_infer_test indexed/gather_infer_test.cc)
target_link_libraries(symbolic4_gather_infer_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_gather_infer_test COMMAND symbolic4_gather_infer_test)
set_tests_properties(symbolic4_gather_infer_test PROPERTIES LABELS "symbolic4;indexed;gather;infer")

# Minimal gather+infer test for debugging
add_executable(symbolic4_gather_infer_minimal_test indexed/gather_infer_minimal_test.cc)
target_link_libraries(symbolic4_gather_infer_minimal_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_gather_infer_minimal_test COMMAND symbolic4_gather_infer_minimal_test)
set_tests_properties(symbolic4_gather_infer_minimal_test PROPERTIES LABELS "symbolic4;indexed;gather;infer")


# mcmc/posterior.h tests
add_executable(symbolic4_posterior_test mcmc/posterior_test.cc)
target_link_libraries(symbolic4_posterior_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_posterior_test COMMAND symbolic4_posterior_test)
set_tests_properties(symbolic4_posterior_test PROPERTIES LABELS "symbolic4;mcmc;posterior")

# mcmc/transforms.h tests
add_executable(symbolic4_transforms_test mcmc/transforms_test.cc)
target_link_libraries(symbolic4_transforms_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_transforms_test COMMAND symbolic4_transforms_test)
set_tests_properties(symbolic4_transforms_test PROPERTIES LABELS "symbolic4;mcmc;transforms")

# mcmc/simplex_transform tests
add_executable(symbolic4_simplex_transform_test mcmc/simplex_transform_test.cc)
target_link_libraries(symbolic4_simplex_transform_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_simplex_transform_test COMMAND symbolic4_simplex_transform_test)
set_tests_properties(symbolic4_simplex_transform_test PROPERTIES LABELS "symbolic4;mcmc;simplex_transform")

# mcmc/support.h tests
add_executable(symbolic4_support_test mcmc/support_test.cc)
target_link_libraries(symbolic4_support_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_support_test COMMAND symbolic4_support_test)
set_tests_properties(symbolic4_support_test PROPERTIES LABELS "symbolic4;mcmc;support")

# mcmc/plate_transforms.h tests
add_executable(symbolic4_plate_transforms_test mcmc/plate_transforms_test.cc)
target_link_libraries(symbolic4_plate_transforms_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_plate_transforms_test COMMAND symbolic4_plate_transforms_test)
set_tests_properties(symbolic4_plate_transforms_test PROPERTIES LABELS "symbolic4;mcmc;plate_transforms")

# mcmc/hmc_adapter_test: HMC integration with PlateTransformedPosterior
add_executable(symbolic4_hmc_adapter_test mcmc/hmc_adapter_test.cc)
target_link_libraries(symbolic4_hmc_adapter_test PRIVATE symbolic4 tempura::bayes tempura::prob unit)
add_test(NAME symbolic4_hmc_adapter_test COMMAND symbolic4_hmc_adapter_test)
set_tests_properties(symbolic4_hmc_adapter_test PROPERTIES LABELS "symbolic4;mcmc;hmc_adapter")

# mcmc/likelihoods.h tests
add_executable(symbolic4_likelihoods_test mcmc/likelihoods_test.cc)
target_link_libraries(symbolic4_likelihoods_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_likelihoods_test COMMAND symbolic4_likelihoods_test)
set_tests_properties(symbolic4_likelihoods_test PROPERTIES LABELS "symbolic4;mcmc;likelihoods")

# mcmc/ordered_effect.h tests
add_executable(symbolic4_ordered_effect_test mcmc/ordered_effect_test.cc)
target_link_libraries(symbolic4_ordered_effect_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_ordered_effect_test COMMAND symbolic4_ordered_effect_test)
set_tests_properties(symbolic4_ordered_effect_test PROPERTIES LABELS "symbolic4;mcmc;ordered_effect")

# simplify/ordering.h tests
add_executable(symbolic4_ordering_test simplify/ordering_test.cc)
target_link_libraries(symbolic4_ordering_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_ordering_test COMMAND symbolic4_ordering_test)
set_tests_properties(symbolic4_ordering_test PROPERTIES LABELS "symbolic4;simplify;ordering")

# simplify/canonicalize.h tests
add_executable(symbolic4_canonicalize_test simplify/canonicalize_test.cc)
target_link_libraries(symbolic4_canonicalize_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_canonicalize_test COMMAND symbolic4_canonicalize_test)
set_tests_properties(symbolic4_canonicalize_test PROPERTIES LABELS "symbolic4;simplify;canonicalize")

# model.h tests
add_executable(symbolic4_model_test model_test.cc)
target_link_libraries(symbolic4_model_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_model_test COMMAND symbolic4_model_test)
set_tests_properties(symbolic4_model_test PROPERTIES LABELS "symbolic4;model")

# strategy/pattern.h tests
add_executable(symbolic4_pattern_test strategy/pattern_test.cc)
target_link_libraries(symbolic4_pattern_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_pattern_test COMMAND symbolic4_pattern_test)
set_tests_properties(symbolic4_pattern_test PROPERTIES LABELS "symbolic4;strategy;pattern")

# strategy/diff.h tests
add_executable(symbolic4_strategy_diff_test strategy/diff_test.cc)
target_link_libraries(symbolic4_strategy_diff_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_strategy_diff_test COMMAND symbolic4_strategy_diff_test)
set_tests_properties(symbolic4_strategy_diff_test PROPERTIES LABELS "symbolic4;strategy;diff")

# strategy/reduce_over.h + strategy integration tests
add_executable(symbolic4_reduce_over_test strategy/reduce_over_test.cc)
target_link_libraries(symbolic4_reduce_over_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_reduce_over_test COMMAND symbolic4_reduce_over_test)
set_tests_properties(symbolic4_reduce_over_test PROPERTIES LABELS "symbolic4;strategy;reduce_over")

# strategy/recursive.h tests
add_executable(symbolic4_recursive_test strategy/recursive_test.cc)
target_link_libraries(symbolic4_recursive_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_recursive_test COMMAND symbolic4_recursive_test)
set_tests_properties(symbolic4_recursive_test PROPERTIES LABELS "symbolic4;strategy;recursive")

# distributions/collect_log_prob.h tests
add_executable(symbolic4_collect_log_prob_test collect_log_prob_test.cc)
target_link_libraries(symbolic4_collect_log_prob_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_collect_log_prob_test COMMAND symbolic4_collect_log_prob_test)
set_tests_properties(symbolic4_collect_log_prob_test PROPERTIES LABELS "symbolic4;distributions;collect_log_prob")

# infer.h tests
add_executable(symbolic4_infer_test infer_test.cc)
target_link_libraries(symbolic4_infer_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME symbolic4_infer_test COMMAND symbolic4_infer_test)
set_tests_properties(symbolic4_infer_test PROPERTIES LABELS "symbolic4;infer")

# matrix/matrix.h tests
add_executable(symbolic4_matrix_test matrix/matrix_test.cc)
target_link_libraries(symbolic4_matrix_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_matrix_test COMMAND symbolic4_matrix_test)
set_tests_properties(symbolic4_matrix_test PROPERTIES LABELS "symbolic4;matrix")

# mcmc/matrix_mcmc_test.cc tests
add_executable(symbolic4_matrix_mcmc_test mcmc/matrix_mcmc_test.cc)
target_link_libraries(symbolic4_matrix_mcmc_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_matrix_mcmc_test COMMAND symbolic4_matrix_mcmc_test)
set_tests_properties(symbolic4_matrix_mcmc_test PROPERTIES LABELS "symbolic4;mcmc;matrix")

# mcmc/non_centered.h tests
add_executable(symbolic4_non_centered_test mcmc/non_centered_test.cc)
target_link_libraries(symbolic4_non_centered_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_non_centered_test COMMAND symbolic4_non_centered_test)
set_tests_properties(symbolic4_non_centered_test PROPERTIES LABELS "symbolic4;mcmc;non_centered")

# mcmc/state.h and mcmc/samples.h tests
add_executable(symbolic4_state_test mcmc/state_test.cc)
target_link_libraries(symbolic4_state_test PRIVATE symbolic4 tempura::bayes tempura::prob unit)
add_test(NAME symbolic4_state_test COMMAND symbolic4_state_test)
set_tests_properties(symbolic4_state_test PROPERTIES LABELS "symbolic4;mcmc;state;samples")

# mcmc/transform_pack.h tests
add_executable(symbolic4_transform_pack_test mcmc/transform_pack_test.cc)
target_link_libraries(symbolic4_transform_pack_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_transform_pack_test COMMAND symbolic4_transform_pack_test)
set_tests_properties(symbolic4_transform_pack_test PROPERTIES LABELS "symbolic4;mcmc;transform_pack")

# symbolic_state.h tests
add_executable(symbolic4_symbolic_state_test symbolic_state_test.cc)
target_link_libraries(symbolic4_symbolic_state_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_symbolic_state_test COMMAND symbolic4_symbolic_state_test)
set_tests_properties(symbolic4_symbolic_state_test PROPERTIES LABELS "symbolic4;symbolic_state")

# plate_infer_test.cc - plate inference API tests
add_executable(plate_infer_test plate_infer_test.cc)
target_link_libraries(plate_infer_test PRIVATE symbolic4 tempura::prob unit)
add_test(NAME plate_infer_test COMMAND plate_infer_test)
set_tests_properties(plate_infer_test PROPERTIES LABELS "symbolic4;plate;infer")

# grad.h tests
add_executable(symbolic4_grad_test grad_test.cc)
target_link_libraries(symbolic4_grad_test PRIVATE symbolic4 unit)
add_test(NAME symbolic4_grad_test COMMAND symbolic4_grad_test)
set_tests_properties(symbolic4_grad_test PROPERTIES LABELS "symbolic4;grad")

# Demo executable to see toString output
add_executable(symbolic4_to_string_demo interpreter/to_string_demo.cc)
target_link_libraries(symbolic4_to_string_demo PRIVATE symbolic4)
