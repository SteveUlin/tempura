#include <iostream>
#include <print>
#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/simplify.h"
#include "symbolic4/infer.h"

using namespace tempura::symbolic4;

int main() {
  // Test infer() auto-discovery
  auto mu = normal(lit(0.0), lit(10.0));
  auto sigma = halfNormal(lit(5.0));
  auto y = normal(mu, sigma);

  // infer(y) should discover mu and sigma?
  auto posterior = infer(y);

  // posterior.logProb(...) expects arguments matching params.
  // If infer(y) only puts y in params, it takes 1 arg.
  // If it discovers mu, sigma, it takes 3 args.
  
  std::cout << "Number of parameters: " << posterior.numParams() << std::endl;
  
  // Try to evaluate
  try {
      // If it only has 1 param (y), this will compile but evaluate will fail due to unbound mu/sigma
      // If it has 3 params, we need to know the order.
      // But SimplePosterior uses a tuple of params.
      
      // Let's just print the number of params. That determines if discovery happened.
      if (posterior.numParams() == 1) {
          std::cout << "FAILURE: infer(y) only has 1 parameter (y). Parents not discovered as parameters." << std::endl;
      } else {
          std::cout << "SUCCESS: infer(y) has " << posterior.numParams() << " parameters." << std::endl;
      }
  } catch (...) {
      std::cout << "Exception occurred." << std::endl;
  }

  return 0;
}