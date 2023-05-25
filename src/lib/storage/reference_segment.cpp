#include "reference_segment.hpp"

#include "storage/table.hpp"
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

}  // namespace opossum
