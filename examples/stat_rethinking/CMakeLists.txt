# Statistical Rethinking 2025 homework solutions
# https://github.com/rmcelreath/stat_rethinking_2025/tree/main/homework

# Random Allocation Game: Detecting dishonesty via Bayesian inference
add_executable(random_allocation_game random_allocation_game.cpp)
target_link_libraries(random_allocation_game PRIVATE bayes symbolic4 plot)

# BMJ Weekend Submissions: No-pooling vs partial-pooling (hierarchical models)
add_executable(bmj_weekend_submissions bmj_weekend_submissions.cpp)
target_link_libraries(bmj_weekend_submissions PRIVATE bayes symbolic4 plot)

# Linear Regression: Bayesian inference with HMC
add_executable(linear_regression linear_regression.cpp)
target_link_libraries(linear_regression PRIVATE bayes symbolic4)
target_compile_options(linear_regression PRIVATE -fconstexpr-steps=10000000)

# Logistic Regression: Binary classification with HMC
add_executable(logistic_regression logistic_regression.cpp)
target_link_libraries(logistic_regression PRIVATE bayes symbolic4)
target_compile_options(logistic_regression PRIVATE -fconstexpr-steps=10000000)

# Bangladesh Contraception: Multilevel model with varying slopes (B02)
add_executable(bangladesh_contraception bangladesh_contraception.cpp)
target_link_libraries(bangladesh_contraception PRIVATE bayes plot)

# BMJ Symbolic: Auto-diff demonstration using SumOver and diff()
add_executable(bmj_symbolic bmj_symbolic.cpp)
target_link_libraries(bmj_symbolic PRIVATE bayes symbolic4 plot)
target_compile_options(bmj_symbolic PRIVATE -fconstexpr-steps=10000000)

# Bangladesh Gather Test: Tests cross-plate indexing with gather()
add_executable(bangladesh_gather_test bangladesh_gather_test.cpp)
target_link_libraries(bangladesh_gather_test PRIVATE symbolic4)

# Hierarchical Gather Example: Full model with gather() and sample()
add_executable(hierarchical_gather_example hierarchical_gather_example.cpp)
target_link_libraries(hierarchical_gather_example PRIVATE bayes symbolic4)
target_compile_options(hierarchical_gather_example PRIVATE -fconstexpr-steps=10000000)

# Week 2: Geocentric Models / Categories and Curves
# Week 3: Elemental Confounds / Good and Bad Controls
# Week 4: Overfitting / MCMC
# Week 5: Modeling Events / Counts and Confounds
# Week 6: Ordered Categories / Multilevel Models
# Week 7: Multilevel Adventures / Correlated Features
# Week 8: Social Networks / Gaussian Processes
# Week 9: Measurement / Missing Data
