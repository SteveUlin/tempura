#include "optimization/downhill_simplex.h"

#include "special/rosnbrock_function.h"
#include "optimization/average_dissent.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::optimization;

auto main() -> int {
  "makeSimplex"_test = [] {
    constexpr std::tuple input{1., 2., 3.};

    constexpr auto simplex = makeSimplex(1., input);
    static_assert(std::tuple{1., 2., 3.} == simplex[0]);
    static_assert(std::tuple{2., 2., 3.} == simplex[1]);
    static_assert(std::tuple{1., 3., 3.} == simplex[2]);
    static_assert(std::tuple{1., 2., 4.} == simplex[3]);
  };

  "reflection"_test = [] {
    constexpr auto simplex = std::array{
        std::tuple{0., 0., 0.},
        std::tuple{1., 0., 0.},
        std::tuple{0., 1., 0.},
        std::tuple{0., 0., 1.},
    };
    constexpr std::tuple sum{1., 1., 1.};

    constexpr auto flip = scaleAgainstFace(-1., sum, simplex[0]);
    static_assert(std::abs(std::get<0>(flip) - .666666666666667) < 1e-10);
    static_assert(std::abs(std::get<1>(flip) - .666666666666667) < 1e-10);
    static_assert(std::abs(std::get<2>(flip) - .666666666666667) < 1e-10);

    constexpr auto id = scaleAgainstFace(1., sum, simplex[0]);
    static_assert(std::abs(std::get<0>(id)) < 1e-10);
    static_assert(std::abs(std::get<1>(id)) < 1e-10);
    static_assert(std::abs(std::get<2>(id)) < 1e-10);

    constexpr auto stretch = scaleAgainstFace(2., sum, simplex[0]);
    static_assert(std::abs(std::get<0>(stretch) + .333333333333333) < 1e-10);
    static_assert(std::abs(std::get<1>(stretch) + .333333333333333) < 1e-10);
    static_assert(std::abs(std::get<2>(stretch) + .333333333333333) < 1e-10);
  };

  "simple optimization"_test = [] {
    auto func = [](double x, double y, double z) {
      return std::pow(x + 30, 2) + std::pow(y - 40, 2) - 1 / (z * z + 1);
    };
    // start some distance away from the minimum
    auto simplex = makeSimplex(1., std::tuple{-350., 400., 100.});
    auto val = downhillSimplex(simplex, func);
    double next;
    while (true) {
      std::cout << "Final value: " << val << "\n";
      // Print the simplex
      for (const auto& vec : simplex) {
        std::print("[{:.4} {:.4} {:.4}]", std::get<0>(vec), std::get<1>(vec),
                   std::get<2>(vec));
        std::cout << "\t value:";
        std::cout << std::apply(func, vec);
        std::cout << "\n";
      }
      simplex = makeSimplex(1., simplex[0]);
      next = downhillSimplex(simplex, func);
      if (std::abs(next - val) < 1e-6) {
        break;
      }
      val = next;
    }
    std::cout << "Final value: " << val << "\n";
    // Print the simplex
    for (const auto& vec : simplex) {
      std::print("[{:.4} {:.4} {:.4}]", std::get<0>(vec), std::get<1>(vec),
                 std::get<2>(vec));
      std::cout << "\t value:";
      std::cout << std::apply(func, vec);
      std::cout << "\n";
    }
  };

  "bananna"_test = [] {
    auto simplex = makeSimplex(1., std::tuple{10., 10.});
    auto val = downhillSimplex(
        simplex, [](double x, double y) { return special::rosnbrockFn(x, y); });
    while (true) {
      simplex = makeSimplex(1., simplex[0]);
      auto next = downhillSimplex(simplex, [](double x, double y) {
        return special::rosnbrockFn(x, y);
      });
      if (std::abs(next - val) < 1e-6) {
        break;
      }
    }
    for (const auto& vec : simplex) {
      std::print("[{:.4} {:.4}]", std::get<0>(vec), std::get<1>(vec));
      std::cout << "\t value:";
      std::cout << special::rosnbrockFn(std::get<0>(vec), std::get<1>(vec));
      std::cout << "\n";
    }
  };
  "bananna average dissent"_test = [] {
    auto start = std::tuple{10., 10.};
    auto end = averageDissent(start, [](double x, double y) {
      return special::rosnbrockFn(x, y);
    });
    std::cout << "Final value: " << special::rosnbrockFn(std::get<0>(end),
                                                          std::get<1>(end))
              << "\n";
    std::cout << "Final point: " << std::get<0>(end) << " "
              << std::get<1>(end) << "\n";
  };
  return 0;
}
