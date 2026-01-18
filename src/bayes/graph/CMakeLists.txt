# ============================================================================
# bayes::graph - Expression-graph DSL for probabilistic programming
# ============================================================================
#
# A minimal DSL that produces symbolic expressions for Bayesian models:
#
#   auto mu = Normal(0, 10);
#   auto sigma = HalfNormal(5);
#   auto y = Normal(mu, sigma);
#
#   auto lp = jointLogProb(y);
#   double val = evaluate(lp, BinderPack{mu = 1.0, sigma = 2.0, y = 2.5});
#
# Dependencies:
#   - symbolic3: For symbolic expression handling
#   - prob: For log-probability functions
# ============================================================================

add_library(bayes_graph INTERFACE)
target_include_directories(bayes_graph INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(bayes_graph INTERFACE
  tempura::symbolic3
  tempura::prob
  tempura_flags
)
add_library(tempura::bayes_graph ALIAS bayes_graph)

# ============================================================================
# Tests
# ============================================================================

add_executable(graph_test graph_test.cc)
target_link_libraries(graph_test PRIVATE bayes_graph unit)
add_test(NAME bayes_graph_test COMMAND graph_test)
