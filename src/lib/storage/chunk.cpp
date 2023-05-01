#include "chunk.hpp"

#include "abstract_segment.hpp"
#include "utils/assert.hpp"
#include "value_segment.hpp"
#include <memory>

namespace opossum {

void Chunk::add_segment(const std::shared_ptr<AbstractSegment> segment) {
  // Implementation goes here
  _chunk_segments.push_back(segment);
}

void Chunk::append(const std::vector<AllTypeVariant>& values) {
  // Implementation goes here
  DebugAssert(values.size() == column_count(), "incorrect number of values");

  int32_t index = 0;
  for (const auto &segment: _chunk_segments) {
    auto type = segment->operator[](0).which();
    const auto &value = values.operator[](index++);
    const auto value_type = value.which();
    if (type == value_type || value_type == 0) {
      switch(type) {
        case 1:
          std::dynamic_pointer_cast<ValueSegment<int32_t>>(segment)->append(value);
          break;
        case 2:
          std::dynamic_pointer_cast<ValueSegment<int64_t>>(segment)->append(value);
          break;
        case 3:
          std::dynamic_pointer_cast<ValueSegment<float>>(segment)->append(value);
          break;
        case 4:
          std::dynamic_pointer_cast<ValueSegment<double>>(segment)->append(value);
          break;
        case 5:
          std::dynamic_pointer_cast<ValueSegment<std::string>>(segment)->append(value);
          break;
        default:
          throw std::logic_error{"should not happen: " + std::to_string(type)};
      }
    } else {
      throw std::logic_error{"wrong data type"};
    }
  }

}

std::shared_ptr<AbstractSegment> Chunk::get_segment(const ColumnID column_id) const {
  // Implementation goes here
  return _chunk_segments[column_id];
}

ColumnCount Chunk::column_count() const {
  // Implementation goes here
  return ColumnCount(_chunk_segments.size());
}

ChunkOffset Chunk::size() const {
  // Implementation goes here
  if (_chunk_segments.size() != 0) {
    return _chunk_segments[0]->size();
  } else {
    return 0;
  }
}

}  // namespace opossum
