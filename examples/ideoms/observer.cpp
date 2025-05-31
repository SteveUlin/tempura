#include <algorithm>
#include <functional>
#include <print>
#include <string>
#include <vector>

// Observer
//
// The observer pattern lets you define a one-to-many dependency between objects
// so that when one object changes state, all its dependents are notified and
// updated automatically.

template <typename Subject, typename StateTag>
class Observer {
 public:
  using OnUpdate = std::function<void(const Subject&, StateTag)>;

  explicit Observer(OnUpdate update_fn) : update_fn_(std::move(update_fn)) {}

  void update(const Subject& subject, StateTag state) const {
    update_fn_(subject, state);
  }

 private:
  OnUpdate update_fn_;
};

class Person {
 public:
  enum class StateChanged : std::uint8_t {
    FirstNameChanged,
    LastNameChanged,
    EmailChanged
  };

  using PersonObserver = Observer<Person, StateChanged>;

  explicit Person(std::string first_name, std::string last_name,
                  std::string email)
      : first_name_(std::move(first_name)),
        last_name_(std::move(last_name)),
        email_(std::move(email)) {}

  void addObserver(PersonObserver* observer) { observers_.push_back(observer); }

  void removeObserver(PersonObserver* observer) {
    std::erase(observers_, observer);
  }

  void setFirstName(std::string first_name) {
    first_name_ = std::move(first_name);
    notifyObservers(StateChanged::FirstNameChanged);
  }

  void setLastName(std::string last_name) {
    last_name_ = std::move(last_name);
    notifyObservers(StateChanged::LastNameChanged);
  }

  void setEmail(std::string email) {
    email_ = std::move(email);
    notifyObservers(StateChanged::EmailChanged);
  }

  auto getFirstName() const -> const std::string& { return first_name_; }

  auto getLastName() const -> const std::string& { return last_name_; }

  auto getEmail() const -> const std::string& { return email_; }

 private:
  void notifyObservers(StateChanged state) const {
    for (const auto& observer : observers_) {
      observer->update(*this, state);
    }
  }

  std::string first_name_;
  std::string last_name_;
  std::string email_;

  std::vector<PersonObserver*> observers_;
};

auto main() -> int {
  Person::PersonObserver observer(
      [](const Person& person, Person::StateChanged state) {
        if (state == Person::StateChanged::FirstNameChanged) {
          std::println("First name changed to: {}", person.getFirstName());
        } else if (state == Person::StateChanged::LastNameChanged) {
          std::println("Last name changed to: {}", person.getLastName());
        } else if (state == Person::StateChanged::EmailChanged) {
          std::println("Email changed to: {}", person.getEmail());
        }
      });

  Person john("John", "Doe", "john@test.com");

  john.addObserver(&observer);
  john.setFirstName("Jonathan");
  john.setLastName("Smith");
  john.setEmail("johnsmith@test.com");

  john.removeObserver(&observer);
  john.setFirstName("Johnny");  // No output, observer removed
}
