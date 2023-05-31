#pragma once

#include <optional>
#include "abstract_segment.hpp"
#include "utils/assert.hpp"
#include "storage/table.hpp"
#include "storage/value_segment.hpp"
#include "storage/dictionary_segment.hpp"

namespace opossum {

// ReferenceSegment is a specific segment type that stores all its values as position list of a referenced column.
class ReferenceSegment : public AbstractSegment {
 public:
  // Creates a reference segment. The parameters specify the positions and the referenced column.
  ReferenceSegment(const std::shared_ptr<const Table>& referenced_table, const ColumnID referenced_column_id,
                   const std::shared_ptr<const PosList>& pos);

  AllTypeVariant operator[](const ChunkOffset chunk_offset) const override;

  ChunkOffset size() const override;

  const std::shared_ptr<const PosList>& pos_list() const;

  const std::shared_ptr<const Table>& referenced_table() const;

  ColumnID referenced_column_id() const;

  size_t estimate_memory_usage() const final;


 protected:
  template<typename T>
  std::optional<T> _get_typed_value(const ChunkOffset chunk_offset) const;
  const std::shared_ptr<const Table> _referenced_table;
  const ColumnID _referenced_column_id;
  const std::shared_ptr<const PosList> _pos;

  friend class TableScan;
};


template<typename T>
std::optional<T> ReferenceSegment::_get_typed_value(const ChunkOffset chunk_offset) const {
  Assert(chunk_offset < size(), "Chunk offset " + std::to_string(chunk_offset) + " is out of bounds.");
  const auto row_id = pos_list()->operator[](chunk_offset);

  if (row_id.is_null()) {
    return std::nullopt;
  }

  const auto chunk = referenced_table()->get_chunk(row_id.chunk_id);
  const auto segment = chunk->get_segment(referenced_column_id());

  if (const auto value_segment = std::dynamic_pointer_cast<ValueSegment<T>>(segment)) {
    return value_segment->get_typed_value(row_id.chunk_offset);
  }
  if (const auto dictionary_segment = std::dynamic_pointer_cast<DictionarySegment<T>>(segment)) {
    return dictionary_segment->get_typed_value(row_id.chunk_offset);
  }
  Fail("Could not convert underlying segment of ReferenceSegment.");
}

}  // namespace opossum
