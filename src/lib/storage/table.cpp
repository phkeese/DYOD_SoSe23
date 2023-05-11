#include "table.hpp"

#include "resolve_type.hpp"
#include "utils/assert.hpp"
#include "value_segment.hpp"

namespace opossum {

Table::Table(const ChunkOffset target_chunk_size) : _max_chunk_size{target_chunk_size} {
  create_new_chunk();
}

void Table::add_column_definition(const std::string& name, const std::string& type, const bool nullable) {
  Assert(row_count() == 0, "Table already has values.");
  _column_names.push_back(name);
  _column_types.push_back(type);
  _column_nullable.push_back(nullable);
}

void Table::add_column(const std::string& name, const std::string& type, const bool nullable) {
  add_column_definition(name, type, nullable);
  resolve_data_type(type, [&nullable, this](const auto data_type_t) {
    using ColumnDataType = typename decltype(data_type_t)::type;
    const auto value_segment = std::make_shared<ValueSegment<ColumnDataType>>(nullable);
    // We only have one chunk since there are no values in the table.
    _chunks[0]->add_segment(std::move(value_segment));
  });
}

void Table::create_new_chunk() {
  const auto chunk = std::make_shared<Chunk>();
  const auto column_name_size = _column_names.size();
  for (auto index = size_t{0}; index < column_name_size; ++index) {
    const auto is_nullable = _column_nullable[index];
    const auto column_type = _column_types[index];
    resolve_data_type(column_type, [&](const auto data_type_t) {
      using ColumnDataType = typename decltype(data_type_t)::type;
      const auto value_segment = std::make_shared<ValueSegment<ColumnDataType>>(is_nullable);
      chunk->add_segment(std::move(value_segment));
    });
  }
  _chunks.push_back(chunk);
}

void Table::append(const std::vector<AllTypeVariant>& values) {
  if (_chunks.back()->size() >= _max_chunk_size) {
    create_new_chunk();
  }
  const auto& chunk = _chunks.back();
  chunk->append(values);
}

ColumnCount Table::column_count() const {
  return ColumnCount(_column_names.size());
}

uint64_t Table::row_count() const {
  auto chunk_size = size_t{0};
  for (const auto& chunk : _chunks) {
    chunk_size += chunk->size();
  }
  return chunk_size;
}

ChunkID Table::chunk_count() const {
  return ChunkID(_chunks.size());
}

ColumnID Table::column_id_by_name(const std::string& column_name) const {
  const auto search_iterator = std::find(_column_names.begin(), _column_names.end(), column_name);
  Assert(search_iterator != _column_names.end(), "Column with name '" + column_name + "' not found.");
  return ColumnID(std::distance(_column_names.begin(), search_iterator));
}

ChunkOffset Table::target_chunk_size() const {
  return _max_chunk_size;
}

const std::vector<std::string>& Table::column_names() const {
  return _column_names;
}

const std::string& Table::column_name(const ColumnID column_id) const {
  Assert(column_id < _column_names.size(), "Column with ID " + std::to_string(column_id) + " does not exist.");
  return _column_names[column_id];
}

const std::string& Table::column_type(const ColumnID column_id) const {
  Assert(column_id < _column_types.size(), "Column with ID " + std::to_string(column_id) + " does not exist.");
  return _column_types[column_id];
}

bool Table::column_nullable(const ColumnID column_id) const {
  Assert(column_id < _column_nullable.size(), "Column with ID " + std::to_string(column_id) + " does not exist.");
  return _column_nullable[column_id];
}

std::shared_ptr<Chunk> Table::get_chunk(ChunkID chunk_id) {
  Assert(chunk_id < _chunks.size(), "Chunk with ID " + std::to_string(chunk_id) + " does not exist.");
  return _chunks[chunk_id];
}

std::shared_ptr<const Chunk> Table::get_chunk(ChunkID chunk_id) const {
  Assert(chunk_id < _chunks.size(), "Chunk with ID " + std::to_string(chunk_id) + " does not exist.");
  return _chunks[chunk_id];
}

void Table::compress_chunk(const ChunkID chunk_id) {
  // Implementation goes here
  Fail("Implementation is missing.");
}

}  // namespace opossum
