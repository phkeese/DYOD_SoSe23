#include "chunk.hpp"
#include <memory>
#include "abstract_segment.hpp"
#include "utils/assert.hpp"
#include "value_segment.hpp"

namespace opossum {

void Chunk::add_segment(const std::shared_ptr<AbstractSegment> segment) {
  _chunk_segments.push_back(segment);
}

void Chunk::append(const std::vector<AllTypeVariant>& values) {
  DebugAssert(values.size() == column_count(), "incorrect number of values");

  for (size_t i = 0; i < values.size(); i++) {
    auto column = _chunk_segments[i];
    auto value = values[i];
    append_to_segment(value, column);
  }
}

void Chunk::append_to_segment(const AllTypeVariant& value, const std::shared_ptr<AbstractSegment>& segment) {
  if (auto concrete_i32 = std::dynamic_pointer_cast<ValueSegment<int32_t>>(segment)) {
    concrete_i32->append(value);
  } else if (auto concrete_i64 = std::dynamic_pointer_cast<ValueSegment<int64_t>>(segment)) {
    concrete_i64->append(value);
  } else if (auto concrete_float = std::dynamic_pointer_cast<ValueSegment<float>>(segment)) {
    concrete_float->append(value);
  } else if (auto concrete_double = std::dynamic_pointer_cast<ValueSegment<double>>(segment)) {
    concrete_double->append(value);
  } else if (auto concrete_string = std::dynamic_pointer_cast<ValueSegment<std::string>>(segment)) {
    concrete_string->append(value);
  } else {
    throw std::logic_error{"not implemented"};
  }
}

std::shared_ptr<AbstractSegment> Chunk::get_segment(const ColumnID column_id) const {
  return _chunk_segments[column_id];
}

ColumnCount Chunk::column_count() const {
  return ColumnCount(_chunk_segments.size());
}

ChunkOffset Chunk::size() const {
  if (_chunk_segments.size() != 0) {
    return _chunk_segments[0]->size();
  } else {
    return 0;
  }
}

}  // namespace opossum
