#include "dag/ascii_dag.h"

#include <iostream>
#include <string>

#include "unit.h"

using namespace tempura;
using namespace tempura::dag;

auto main() -> int {
  "empty dag"_test = [] {
    Dag dag;
    auto result = renderAsciiDag(dag);
    expectEq(result, "");
  };

  "single node"_test = [] {
    Dag dag;
    dag.addNode("A", "Start");
    auto result = renderAsciiDag(dag);
    std::cout << "Single node:\n" << result << "\n";
    expectTrue(result.find("Start") != std::string::npos);
  };

  "two nodes chain"_test = [] {
    Dag dag;
    dag.addNode("A", "Start");
    dag.addNode("B", "End");
    dag.addEdge("A", "B");
    auto result = renderAsciiDag(dag);
    std::cout << "Two nodes:\n" << result << "\n";
    expectTrue(result.find("Start") != std::string::npos);
    expectTrue(result.find("End") != std::string::npos);
  };

  "three node chain"_test = [] {
    Dag dag;
    dag.addNode("A", "Parse");
    dag.addNode("B", "Compile");
    dag.addNode("C", "Link");
    dag.chain("A", "B", "C");
    auto result = renderAsciiDag(dag);
    std::cout << "Three node chain:\n" << result << "\n";
  };

  "diamond pattern"_test = [] {
    Dag dag;
    dag.addNode("A", "Start");
    dag.addNode("B", "Left");
    dag.addNode("C", "Right");
    dag.addNode("D", "End");
    dag.addEdge("A", "B");
    dag.addEdge("A", "C");
    dag.addEdge("B", "D");
    dag.addEdge("C", "D");
    auto result = renderAsciiDag(dag);
    std::cout << "Diamond:\n" << result << "\n";
  };

  "fork pattern"_test = [] {
    Dag dag;
    dag.addNode("A", "Root");
    dag.addNode("B", "Child1");
    dag.addNode("C", "Child2");
    dag.addNode("D", "Child3");
    dag.fork("A", {"B", "C", "D"});
    auto result = renderAsciiDag(dag);
    std::cout << "Fork:\n" << result << "\n";
  };

  "skip layer edge"_test = [] {
    Dag dag;
    dag.addNode("A", "Top");
    dag.addNode("B", "Middle");
    dag.addNode("C", "Bottom");
    dag.addEdge("A", "B");
    dag.addEdge("B", "C");
    dag.addEdge("A", "C");  // Skip layer
    auto result = renderAsciiDag(dag);
    std::cout << "Skip layer:\n" << result << "\n";
  };

  "ascii charset"_test = [] {
    Dag dag;
    dag.addNode("A", "Hello");
    dag.addNode("B", "World");
    dag.addEdge("A", "B");
    AsciiDagConfig cfg;
    cfg.chars = kAscii;
    auto result = renderAsciiDag(dag, cfg);
    std::cout << "ASCII charset:\n" << result << "\n";
    expectTrue(result.find('+') != std::string::npos);  // ASCII corners
  };

  "rounded charset"_test = [] {
    Dag dag;
    dag.addNode("A", "Hello");
    dag.addNode("B", "World");
    dag.addEdge("A", "B");
    AsciiDagConfig cfg;
    cfg.chars = kBoxRounded;
    auto result = renderAsciiDag(dag, cfg);
    std::cout << "Rounded charset:\n" << result << "\n";
  };

  "complex dag"_test = [] {
    Dag dag;
    dag.addNode("src", "Source");
    dag.addNode("lex", "Lexer");
    dag.addNode("parse", "Parser");
    dag.addNode("ast", "AST");
    dag.addNode("sema", "Semantic");
    dag.addNode("ir", "IR Gen");
    dag.addNode("opt", "Optimize");
    dag.addNode("codegen", "CodeGen");

    dag.chain("src", "lex", "parse", "ast");
    dag.addEdge("ast", "sema");
    dag.addEdge("ast", "ir");
    dag.addEdge("sema", "ir");
    dag.addEdge("ir", "opt");
    dag.addEdge("opt", "codegen");

    auto result = renderAsciiDag(dag);
    std::cout << "Complex DAG (compiler pipeline):\n" << result << "\n";
  };

  return TestRegistry::result();
}
