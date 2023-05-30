#include "reference_segment.hpp"

#include "storage/table.hpp"
#include "storage/value_segment.hpp"
#include "storage/dictionary_segment.hpp"
#include "utils/assert.hpp"

namespace opossum {

ReferenceSegment::ReferenceSegment(const std::shared_ptr<const Table>& referenced_table,
                                   const ColumnID referenced_column_id, const std::shared_ptr<const PosList>& pos)
: _referenced_table{referenced_table}, _referenced_column_id{referenced_column_id}, _pos{pos} {}

AllTypeVariant ReferenceSegment::operator[](const ChunkOffset chunk_offset) const {
  Assert(chunk_offset < size(), "Chunk offset " + std::to_string(chunk_offset) + " is out of bounds.");
  const auto row_id = pos_list()->operator[](chunk_offset);

  if (row_id.is_null()) {
    return NULL_VALUE;
  }

  const auto chunk = referenced_table()->get_chunk(row_id.chunk_id);
  const auto segment = chunk->get_segment(referenced_column_id());

  DebugAssert(!std::dynamic_pointer_cast<ReferenceSegment>(segment), "ReferenceSegment only supports referencing ValueSegment or DictionarySegment.");

  return segment->operator[](row_id.chunk_offset);
}

ChunkOffset ReferenceSegment::size() const {
  return pos_list()->size();
}

const std::shared_ptr<const PosList>& ReferenceSegment::pos_list() const {
  return _pos;
}

const std::shared_ptr<const Table>& ReferenceSegment::referenced_table() const {
  return _referenced_table;
}

ColumnID ReferenceSegment::referenced_column_id() const {
  return _referenced_column_id;
}

size_t ReferenceSegment::estimate_memory_usage() const {
  return sizeof(*this);
}


template<typename T>
std::optional<T> ReferenceSegment::_get_typed_value(const ChunkOffset chunk_offset) const {
  Assert(chunk_offset < size(), "Chunk offset " + std::to_string(chunk_offset) + " is out of bounds.");
  const auto row_id = pos_list()->operator[](chunk_offset);

  if (row_id.is_null()) {
    return NULL_VALUE;
  }

  const auto chunk = referenced_table()->get_chunk(row_id.chunk_id);
  const auto segment = chunk->get_segment(referenced_column_id());

  if (const auto value_segment = std::dynamic_pointer_cast<ValueSegment<T>>(segment)) {
    return value_segment->get_typed_value(chunk_offset);
  }
  if (const auto dictionary_segment = std::dynamic_pointer_cast<DictionarySegment<T>>(segment)) {
    return dictionary_segment->get_typed_value(chunk_offset);
  }
  Fail("Could not convert underlying segment of ReferenceSegment.");

}

}  // namespace opossum
