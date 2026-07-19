#include "weighted_dag.h"

#include <concepts>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "insertion returns dense indices in topological order"_test = [] {
    WeightedDag<double> dag;
    auto a = dag.addNode({});
    auto b = dag.addNode({});
    auto c = dag.addNode({{a, 2.0}, {b, 3.0}});
    expectEq(a, std::size_t{0});
    expectEq(b, std::size_t{1});
    expectEq(c, std::size_t{2});
    expectEq(dag.nodeCount(), std::size_t{3});
  };

  "edges reads back each node's (dep, weight) row"_test = [] {
    WeightedDag<double> dag;
    auto a = dag.addNode({});
    auto b = dag.addNode({{a, 2.0}});
    auto c = dag.addNode({{a, 3.0}, {b, 5.0}});
    expectEq(dag.edges(a).size(), std::size_t{0});
    expectEq(dag.edges(b)[0].dep, a);
    expectEq(dag.edges(b)[0].weight, 2.0);
    expectEq(dag.edges(c).size(), std::size_t{2});
    expectEq(dag.edges(c)[0].dep, a);
    expectEq(dag.edges(c)[0].weight, 3.0);
    expectEq(dag.edges(c)[1].dep, b);
    expectEq(dag.edges(c)[1].weight, 5.0);
  };

  "indices align an external per-node vector"_test = [] {
    WeightedDag<double> dag;
    auto a = dag.addNode({});
    auto b = dag.addNode({{a, 2.0}});
    std::vector<double> per_node(dag.nodeCount(), 0.0);
    per_node[a] = 10.0;
    per_node[b] = 20.0;
    expectRangeEq(per_node, std::vector{10.0, 20.0});
  };

  "edges are mutable in place; const access is span<const Edge>"_test = [] {
    WeightedDag<double> dag;
    auto a = dag.addNode({});
    auto b = dag.addNode({});
    auto c = dag.addNode({{a, 2.0}, {b, 3.0}});
    for (auto& e : dag.edges(c)) e.weight *= 10.0;
    expectEq(dag.edges(c)[0].weight, 20.0);
    expectEq(dag.edges(c)[1].weight, 30.0);
    static_assert(std::same_as<decltype(std::as_const(dag).edges(c)),
                               std::span<const WeightedDag<double>::Edge>>);
  };

  "clear empties the dag and re-recording is correct"_test = [] {
    WeightedDag<double> dag;
    auto a = dag.addNode({});
    dag.addNode({{a, 2.0}});
    dag.clear();
    expectEq(dag.nodeCount(), std::size_t{0});
    auto x = dag.addNode({});
    auto y = dag.addNode({{x, 7.0}});
    expectEq(dag.edges(y)[0].dep, x);
    expectEq(dag.edges(y)[0].weight, 7.0);
  };

  return TestRegistry::result();
}
