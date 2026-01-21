#include "csv.h"
#include "unit.h"

#include <cstdint>
#include <sstream>
#include <string>

using namespace tempura;

auto main() -> int {
  "CsvReader basic parsing"_test = [] {
    std::istringstream input{"Alice,30,1.75\nBob,25,1.80"};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.numFields(), 3UZ);
    expectEq(reader.get<std::string>(0), std::string{"Alice"});
    expectEq(reader.get<int64_t>(1), 30);
    expectNear(reader.get<double>(2), 1.75, 1e-6);

    expectTrue(reader.nextRow());
    expectEq(reader.get<std::string>(0), std::string{"Bob"});
    expectEq(reader.get<int64_t>(1), 25);
    expectNear(reader.get<double>(2), 1.80, 1e-6);

    expectFalse(reader.nextRow());
  };

  "CsvReader skip header"_test = [] {
    std::istringstream input{"name,age,height\nAlice,30,1.75\nBob,25,1.80"};
    CsvReader reader{input};
    reader.skipRows(1);

    expectTrue(reader.nextRow());
    expectEq(reader.get<std::string>(0), std::string{"Alice"});

    expectTrue(reader.nextRow());
    expectEq(reader.get<std::string>(0), std::string{"Bob"});

    expectFalse(reader.nextRow());
  };

  "CsvReader custom delimiter"_test = [] {
    std::istringstream input{"Alice;30;1.75\nBob;25;1.80"};
    CsvReader reader{input, ';'};

    expectTrue(reader.nextRow());
    expectEq(reader.get<std::string>(0), std::string{"Alice"});
    expectEq(reader.get<int64_t>(1), 30);
  };

  "CsvReader quoted fields"_test = [] {
    std::istringstream input{R"("Hello, World",42,"Test")"};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.numFields(), 3UZ);
    expectEq(reader.get<std::string>(0), std::string{"Hello, World"});
    expectEq(reader.get<int64_t>(1), 42);
    expectEq(reader.get<std::string>(2), std::string{"Test"});
  };

  "CsvReader escaped quotes"_test = [] {
    std::istringstream input{R"("He said ""Hello""",123)"};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.get<std::string>(0), std::string{R"(He said "Hello")"});
    expectEq(reader.get<int64_t>(1), 123);
  };

  "CsvReader empty fields"_test = [] {
    std::istringstream input{"Alice,,30"};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.numFields(), 3UZ);
    expectEq(reader.get<std::string>(0), std::string{"Alice"});
    expectEq(reader.getString(1), std::string_view{""});
    expectEq(reader.get<int64_t>(2), 30);
  };

  "CsvReader tryGet success"_test = [] {
    std::istringstream input{"Alice,30,1.75"};
    CsvReader reader{input};
    reader.nextRow();

    auto name = reader.tryGet<std::string>(0);
    expectTrue(name.has_value());
    expectEq(*name, std::string{"Alice"});

    auto age = reader.tryGet<int64_t>(1);
    expectTrue(age.has_value());
    expectEq(*age, 30);

    auto height = reader.tryGet<double>(2);
    expectTrue(height.has_value());
    expectNear(*height, 1.75, 1e-6);
  };

  "CsvReader tryGet out of bounds"_test = [] {
    std::istringstream input{"Alice,30"};
    CsvReader reader{input};
    reader.nextRow();

    auto result = reader.tryGet<std::string>(99);
    expectFalse(result.has_value());
  };

  "CsvReader tryGet parse failure"_test = [] {
    std::istringstream input{"Alice,not_a_number"};
    CsvReader reader{input};
    reader.nextRow();

    auto result = reader.tryGet<int64_t>(1);
    expectFalse(result.has_value());
  };

  "CsvReader negative and floating-point values"_test = [] {
    std::istringstream input{"-42,-3.14,1e-5"};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.get<int64_t>(0), -42);
    expectNear(reader.get<double>(1), -3.14, 1e-6);
    expectNear(reader.get<double>(2), 1e-5, 1e-10);
  };

  "CsvReader whitespace handling"_test = [] {
    std::istringstream input{"  42  ,  3.14  "};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.get<int64_t>(0), 42);
    expectNear(reader.get<double>(1), 3.14, 1e-6);
  };

  "CsvReader empty input"_test = [] {
    std::istringstream input{""};
    CsvReader reader{input};

    expectFalse(reader.nextRow());
  };

  "CsvReader single field"_test = [] {
    std::istringstream input{"42"};
    CsvReader reader{input};

    expectTrue(reader.nextRow());
    expectEq(reader.numFields(), 1UZ);
    expectEq(reader.get<int64_t>(0), 42);
  };

  return TestRegistry::result();
}
