# math — own implementations of elementary functions (the ones with NO hardware
# instruction: exp, log, sin, …; sqrt/fma/round are hardware). Toolkit: float_bits.h
# (IEEE-754 bit surgery), poly.h (polynomial evaluation), ulp_sweep.h (ULP oracle).
# tempura_test() is defined in the parent src/CMakeLists.txt.
tempura_test(float_bits LABEL math)
tempura_test(poly LABEL math)
tempura_test(ulp_sweep LABEL math)
tempura_test(exp LABEL math)
tempura_test(sqrt_approx LABEL math)

# Benchmarks (opt-in: -DTEMPURA_BUILD_BENCHMARKS=ON).
tempura_benchmark(sqrt)
