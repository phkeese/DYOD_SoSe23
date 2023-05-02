#include "storage_manager.hpp"

#include "utils/assert.hpp"

namespace opossum {

static std::unique_ptr<StorageManager> STORAGE_MANAGER_SINGLETON = nullptr;

StorageManager& StorageManager::get() {
  if (!STORAGE_MANAGER_SINGLETON) {
    STORAGE_MANAGER_SINGLETON = std::unique_ptr<StorageManager>(new StorageManager());
  }
  return *STORAGE_MANAGER_SINGLETON;
}

void StorageManager::add_table(const std::string& name, std::shared_ptr<Table> table) {
  _tables.insert({name, table});
}

void StorageManager::drop_table(const std::string& name) {
  if (!_tables.erase(name)) {
    throw std::logic_error{"table does not exist"};
  }
}

std::shared_ptr<Table> StorageManager::get_table(const std::string& name) const {
  return _tables.at(name);
}

bool StorageManager::has_table(const std::string& name) const {
  return _tables.find(name) != _tables.end();
}

std::vector<std::string> StorageManager::table_names() const {
  auto keys = std::vector<std::string>{};
  keys.reserve(_tables.size());
  for (const auto& name_table : _tables) {
    keys.push_back(name_table.first);
  }
  return keys;
}

void StorageManager::print(std::ostream& out) const {
  // format: (name, #columns, #rows, #chunks)
  for (const auto& name_table: _tables) {

    const auto table = name_table.second;
    out << "("
        << name_table.first << ", "
        << table->column_count() << ", "
        << table->row_count() << ", "
        << table->chunk_count()
        << ")";
  }

}

void StorageManager::reset() {
  STORAGE_MANAGER_SINGLETON = nullptr;
}

}  // namespace opossum
