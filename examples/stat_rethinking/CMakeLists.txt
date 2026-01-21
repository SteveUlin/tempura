# Statistical Rethinking 2025 homework solutions
# https://github.com/rmcelreath/stat_rethinking_2025/tree/main/homework

# Random Allocation Game: Detecting dishonesty via Bayesian inference
add_executable(random_allocation_game random_allocation_game.cpp)
target_link_libraries(random_allocation_game PRIVATE bayes bayes2 plot)

# BMJ Weekend Submissions: No-pooling vs partial-pooling (hierarchical models)
add_executable(bmj_weekend_submissions bmj_weekend_submissions.cpp)
target_link_libraries(bmj_weekend_submissions PRIVATE bayes2 plot)

# Week 2: Geocentric Models / Categories and Curves
# Week 3: Elemental Confounds / Good and Bad Controls
# Week 4: Overfitting / MCMC
# Week 5: Modeling Events / Counts and Confounds
# Week 6: Ordered Categories / Multilevel Models
# Week 7: Multilevel Adventures / Correlated Features
# Week 8: Social Networks / Gaussian Processes
# Week 9: Measurement / Missing Data
