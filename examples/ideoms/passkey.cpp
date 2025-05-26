#include <memory>
#include <print>

// The passkey idiom grants specific limited access to a class's members to
// a limited set of designated "friend" classes or functions.
//
// The core idea is to have a private nested class, "the passkey", within the
// class you want to control access to. The class then has methods requiring
// this passkey to be called, effectively limiting access to those who have
// the passkey.

// Example:
//   Grant the Resource Manager access to restricted methods of the "Resource".

class ResourceManager;

class Resource {
 public:
  struct Passkey {
    friend class ResourceManager;
    Passkey(const Passkey&) = delete;
    auto operator=(const Passkey&) -> Passkey& = delete;

   private:
    Passkey() = default;
  };

  Resource(std::string data) : secret_data_(std::move(data)) {}

  void publicOperation() { std::println("Public operation on Resource"); }

  void sensitiveOperation(Passkey) {
    std::println("Sensitive operation on Resource with passkey");
    std::println("Secret data: {}", secret_data_);
  }

 private:
  std::string secret_data_;
};

class ResourceManager {
 public:
  ResourceManager() : resource_("Top Secret Data") {}

  void manageResource() {
    resource_.publicOperation();
    resource_.sensitiveOperation(Resource::Passkey{});
  }

  auto getResource() -> Resource& {
    return resource_;
  }

 private:
  Resource resource_;
};

class UnrelatedClass {
 public:
  void tryAccess(Resource& resource) {
    // This will not compile because UnrelatedClass does not have access to
    // Resource::Passkey.
    // resource.sensitiveOperation(Resource::Passkey{});
    std::println(
        "UnrelatedClass cannot access sensitive operations of Resource");
  }
};

int main() {
  ResourceManager manager;
  manager.manageResource();

  UnrelatedClass unrelated;
  unrelated.tryAccess(manager.getResource());

  return 0;
}
