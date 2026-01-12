// Demonstration of Swiss Table tombstone reuse behavior
//
// This program shows the difference between the standard Swiss Table
// and the reuse variant when handling deleted slots.

#include <iostream>

#include "containers/maps/swiss_table.h"
#include "containers/maps/swiss_table_reuse.h"

using namespace tempura;

auto main() -> int {
  std::cout << "Swiss Table Tombstone Reuse Demo\n";
  std::cout << "=================================\n\n";

  // Standard Swiss Table
  {
    std::cout << "Standard SwissTable:\n";
    SwissTable<int, int> map;

    // Insert some elements
    for (int i = 0; i < 10; ++i) {
      map.insert(i, i * 100);
    }
    std::cout << "  Inserted 10 elements, size = " << map.size() << "\n";

    // Erase some elements (creates tombstones)
    for (int i = 0; i < 5; ++i) {
      map.erase(i);
    }
    std::cout << "  Erased 5 elements, size = " << map.size() << "\n";

    // Insert new elements
    for (int i = 100; i < 105; ++i) {
      map.insert(i, i * 100);
    }
    std::cout << "  Inserted 5 new elements, size = " << map.size() << "\n";

    // Verify all expected keys exist
    int count = 0;
    for (int i = 5; i < 10; ++i) {
      if (map.contains(i)) ++count;
    }
    for (int i = 100; i < 105; ++i) {
      if (map.contains(i)) ++count;
    }
    std::cout << "  Valid keys accessible = " << count << "\n\n";
  }

  // Swiss Table with Reuse
  {
    std::cout << "SwissTableReuse (with tombstone reuse):\n";
    SwissTableReuse<int, int> map;

    // Insert some elements
    for (int i = 0; i < 10; ++i) {
      map.insert(i, i * 100);
    }
    std::cout << "  Inserted 10 elements, size = " << map.size() << "\n";

    // Erase some elements (creates tombstones)
    for (int i = 0; i < 5; ++i) {
      map.erase(i);
    }
    std::cout << "  Erased 5 elements, size = " << map.size() << "\n";

    // Insert new elements - should reuse tombstones
    for (int i = 100; i < 105; ++i) {
      map.insert(i, i * 100);
    }
    std::cout << "  Inserted 5 new elements, size = " << map.size() << "\n";

    // Verify all expected keys exist
    int count = 0;
    for (int i = 5; i < 10; ++i) {
      if (map.contains(i)) ++count;
    }
    for (int i = 100; i < 105; ++i) {
      if (map.contains(i)) ++count;
    }
    std::cout << "  Valid keys accessible = " << count << "\n\n";
  }

  std::cout << "Key Difference:\n";
  std::cout << "---------------\n";
  std::cout << "SwissTable: Uses first available slot (empty OR deleted)\n";
  std::cout << "SwissTableReuse: Remembers first deleted slot, prefers it over empty\n";
  std::cout << "\nBenefit: Better memory locality - keeps occupied region compact\n";
  std::cout << "Tradeoff: Slightly slower insert (must probe until empty, not just available)\n";

  return 0;
}
