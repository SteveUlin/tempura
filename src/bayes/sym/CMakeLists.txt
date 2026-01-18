# ============================================================================
# bayes::sym - Symbolic Bayesian model DSL
# ============================================================================
#
# Provides a declarative DSL for defining probabilistic models:
#
#   auto model = Model{
#       param(mu)    = Normal(0_c, 10_c),
#       param(sigma) = HalfNormal(5_c),
#       data(y)      = Normal(mu, sigma)
#   };
#
# Dependencies:
#   - symbolic3: For symbolic expression handling
#   - prob: For log-probability functions
# ============================================================================

add_library(bayes_sym INTERFACE)
target_include_directories(bayes_sym INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(bayes_sym INTERFACE
  tempura::symbolic3
  tempura::prob
  tempura_flags
)
add_library(tempura::bayes_sym ALIAS bayes_sym)

# ============================================================================
# Tests
# ============================================================================

add_executable(constraints_test constraints_test.cc)
target_link_libraries(constraints_test PRIVATE bayes_sym)
add_test(NAME bayes_sym_constraints_test COMMAND constraints_test)

add_executable(dsl_test dsl_test.cc)
target_link_libraries(dsl_test PRIVATE bayes_sym)
add_test(NAME bayes_sym_dsl_test COMMAND dsl_test)

# ============================================================================
# Examples
# ============================================================================

add_executable(dsl_example dsl_example.cc)
target_link_libraries(dsl_example PRIVATE bayes_sym)
