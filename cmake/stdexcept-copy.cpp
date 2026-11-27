// Link probe for GCC PR125151. Both copies must work without optimisation.
#include <cstring>
#include <stdexcept>

std::runtime_error copy_runtime(std::runtime_error const &value) { return value; }
std::logic_error copy_logic(std::logic_error const &value) { return value; }

int main() {
  std::runtime_error runtime{"runtime copy"};
  std::logic_error logic{"logic copy"};
  return std::strcmp(copy_runtime(runtime).what(), "runtime copy")
    || std::strcmp(copy_logic(logic).what(), "logic copy");
}
