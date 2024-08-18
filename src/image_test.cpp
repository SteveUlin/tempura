#include "image.h"

#include "unit.h"

#include <fstream>

using namespace tempura;

auto main() -> int {
  "encode image"_test = [] {
    const std::string image =
        encodePgm({.width = 2, .height = 2},
                  std::vector<GreyScalePixel>{{0.0}, {1.0}, {0.0}, {1.0}});

    for (const auto& byte : image) {
      std::cout << static_cast<int>(byte) << " ";
    }
  };

  "write file"_test = [] {
    std::vector<GreyScalePixel> data;
    for (size_t i = 0; i < 1024; ++i) {
      for (size_t j = 0; j < 1024; ++j) {
        data.push_back({static_cast<double>(i) / 1024.0});
      }
    }
    std::string image =
        encodePgm({.width = 1024, .height = 1024}, data);

    std::ofstream file("image.pgm", std::ios::binary);
    file << image;
    file.close();
  };

  return 0;
}
