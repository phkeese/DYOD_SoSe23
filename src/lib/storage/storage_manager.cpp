#include "storage_manager.hpp"

#include "utils/assert.hpp"

namespace opossum {

StorageManager& StorageManager::get() {
  // Not a static class member due to briefing in task instructions.
  static auto singleton = StorageManager{};
  return singleton;
}

void StorageManager::add_table(const std::string& name, std::shared_ptr<Table> table) {
  _tables.insert({name, table});
}

void StorageManager::drop_table(const std::string& name) {
  Assert(_tables.erase(name), "Table does not exist.");
}

std::shared_ptr<Table> StorageManager::get_table(const std::string& name) const {
  const auto table_it = _tables.find(name);
  Assert(table_it != _tables.end(), "No such table named '" + name + "'.");
  return table_it->second;
}

bool StorageManager::has_table(const std::string& name) const {
  return _tables.contains(name);
}

std::vector<std::string> StorageManager::table_names() const {
  auto keys = std::vector<std::string>{};
  keys.reserve(_tables.size());
  for (const auto& [name, _] : _tables) {
    keys.push_back(name);
  }
  return keys;
}

void StorageManager::print(std::ostream& out) const {
  for (const auto& [name, table] : _tables) {
    out << "(" << name << ", " << table->column_count() << ", " << table->row_count() << ", " << table->chunk_count()
        << ")\n";
  }
}

void StorageManager::reset() {
  _tables.clear();
}

}  // namespace opossum
