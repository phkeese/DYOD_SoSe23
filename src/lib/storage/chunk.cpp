#include "chunk.hpp"
#include <memory>
#include "abstract_segment.hpp"
#include "utils/assert.hpp"
#include "value_segment.hpp"

namespace opossum {

namespace {
// Add a value to a given abstract segment.
void append_to_segment(const AllTypeVariant& value, const std::shared_ptr<AbstractSegment>& segment) {
// Test all possible ValueSegment variants.
#define TRY_APPEND_TYPE(Type)                                                         \
  if (const auto concrete = std::dynamic_pointer_cast<ValueSegment<Type>>(segment)) { \
    concrete->append(value);                                                          \
    return;                                                                           \
  }
  TRY_APPEND_TYPE(int32_t)
  TRY_APPEND_TYPE(int64_t)
  TRY_APPEND_TYPE(float)
  TRY_APPEND_TYPE(double)
  TRY_APPEND_TYPE(std::string)
#undef TRY_APPEND_TYPE
  Fail("Datatype not implemented.");
}
}  // namespace

void Chunk::add_segment(const std::shared_ptr<AbstractSegment> segment) {
  Assert(std::find(_chunk_segments.begin(), _chunk_segments.end(), segment) == _chunk_segments.end(),
         "Cannot add segment to chunk twice.");
  _chunk_segments.push_back(segment);
}

void Chunk::append(const std::vector<AllTypeVariant>& values) {
  const auto values_size = values.size();
  DebugAssert(values_size == column_count(), "incorrect number of values");

  for (auto segment_index = size_t{0}; segment_index < values_size; ++segment_index) {
    const auto column = _chunk_segments[segment_index];
    const auto& value = values[segment_index];
    append_to_segment(value, column);
  }
}

std::shared_ptr<AbstractSegment> Chunk::get_segment(const ColumnID column_id) const {
  Assert(column_id < _chunk_segments.size(), "Column with ID " + std::to_string(column_id) + " does not exist.");
  return _chunk_segments[column_id];
}

ColumnCount Chunk::column_count() const {
  return ColumnCount(_chunk_segments.size());
}

ChunkOffset Chunk::size() const {
  if (_chunk_segments.empty()) {
    return 0;
  }
  return _chunk_segments[0]->size();
}

}  // namespace opossum
