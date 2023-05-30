#include "table_scan.hpp"
#include "../all_type_variant.hpp"
#include "../storage/table.hpp"
#include "../storage/dictionary_segment.hpp"
#include "../storage/reference_segment.hpp"
#include "../storage/value_segment.hpp"
#include "../types.hpp"
#include "resolve_type.hpp"

namespace opossum {

namespace {

template<typename T>
void scan_in_value_segment(const Selector<T>& selector, const ChunkID chunk_id, const std::shared_ptr<const ValueSegment<T>>& segment, const std::shared_ptr<PosList>& pos_list) {
  const auto& segment_values = segment->values();
  const auto values_count = segment_values.size();

  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < values_count; ++chunk_offset) {
    if (segment->is_null(chunk_offset)) {
      continue;
    }
    const auto value = segment_values[chunk_offset];
    if (selector.selects(value)) {
      pos_list->emplace_back(RowID{chunk_id, chunk_offset});
    }
  }
}

template<typename T>
void scan_in_dictionary_segment(
    const Selector<T>& selector,
    const ChunkID chunk_id,
    const std::shared_ptr<const DictionarySegment<T>>& segment,
    const std::shared_ptr<PosList>& pos_list) {
  const auto value_ids = segment->attribute_vector();
  const auto search_value = selector.search_value();
  const auto search_id = segment->lower_bound(search_value);

  if (search_id == INVALID_VALUE_ID) {
    return;
  }

  const auto values_count = segment->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < values_count; ++chunk_offset) {
    const auto value_id = value_ids->get(chunk_offset);
    const auto value = segment->dictionary()[value_id];
    (void) value;
    if (selector.selects(value)) {
      pos_list->emplace_back(RowID{chunk_id, chunk_offset});
    }
  }
}

template<typename T>
void scan_in_reference_segment(const Selector<T>& selector, const ChunkID chunk_id, const std::shared_ptr<const ReferenceSegment>& segment, const std::shared_ptr<PosList>& pos_list) {
  const auto values_count = segment->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < values_count; ++chunk_offset) {
    // TODO: We are not supposed to do that.
    const auto all_type_variant = segment->operator[](chunk_offset);
    if (variant_is_null(all_type_variant)) {
      continue;
    }
    const auto value = type_cast<T>(all_type_variant);
    if (selector.selects(value)) {
      pos_list->emplace_back(RowID{chunk_id, chunk_offset});
    }
  }
}

/*
 * This should scan an entire table knowing that we should cast every value to T and compare.
 * It should fill another table with matching tuples and return that.
 */
template<typename T>
std::shared_ptr<const Table> scan(const T search_value, const ScanType scan_type, const ColumnID search_column_id, const std::shared_ptr<const Table>& table) {
  auto result_table = std::make_shared<Table>();
  auto pos_list = std::make_shared<PosList>();
  const auto initial_result_chunk = result_table->get_chunk(ChunkID{0});
  const auto column_count = table->column_count();

  // Creates all columns of old table in the result table and Initializes the result chunks.
  for (auto column_id = ColumnID{0}; column_id < column_count; ++column_id) {
    result_table->add_column_definition(table->column_name(column_id), table->column_type(column_id), table->column_nullable(column_id));
    const auto reference_segment = std::make_shared<ReferenceSegment>(table, column_id, pos_list);
    initial_result_chunk->add_segment(reference_segment);
  }

  const auto selector = Selector<T>{scan_type, search_value};
  const auto chunk_count = table->chunk_count();

  for (auto chunk_id = ChunkID{0}; chunk_id < chunk_count; ++chunk_id) {
    const auto current_chunk = table->get_chunk(chunk_id);
    const auto search_segment = current_chunk->get_segment(search_column_id);

    if (const auto value_segment = std::dynamic_pointer_cast<const ValueSegment<T>>(search_segment)) {
      scan_in_value_segment(selector, chunk_id, value_segment, pos_list);
    } else if (const auto dictionary_segment = std::dynamic_pointer_cast<const DictionarySegment<T>>(search_segment)) {
      scan_in_dictionary_segment(selector, chunk_id, dictionary_segment, pos_list);
    } else if (const auto reference_segment = std::dynamic_pointer_cast<const ReferenceSegment>(search_segment)) {
      scan_in_reference_segment(selector, chunk_id, reference_segment, pos_list);
    } else {
      Fail("Could not match any known segment type.");
    }
  }
  return result_table;
}

}


TableScan::TableScan(const std::shared_ptr<const AbstractOperator>& in, const ColumnID column_id, const ScanType scan_type,
          const AllTypeVariant search_value) : AbstractOperator(in), _column_id{column_id}, _scan_type{scan_type}, _search_value{search_value} {
    DebugAssert(_left_input != nullptr, "TableScan operator requires a left input.");
    DebugAssert(_right_input == nullptr, "There should not be a right input for the table scan operator.");
}


std::shared_ptr<const Table> TableScan::_on_execute() {
   _result_table = std::make_shared<Table>();
   _pos_list = std::make_shared<PosList>();
  const auto initial_result_chunk = _result_table->get_chunk(ChunkID{0});


  // Creates all columns of old table in the result table and Initializes the result chunks.
  const auto input = _left_input_table();
  const auto column_count = input->column_count();

  for (auto column_id = ColumnID{0}; column_id < column_count; ++column_id) {
    _result_table->add_column_definition(input->column_name(column_id), input->column_type(column_id), input->column_nullable(column_id));
    const auto reference_segment = std::make_shared<ReferenceSegment>(input, column_id, _pos_list);
    initial_result_chunk->add_segment(reference_segment);
  }

  resolve_data_type(_left_input_table()->column_type(column_id()), [this] (auto type) {
    using Type = typename decltype(type)::type;
    _scan_table<Type>();
  });
  return _result_table;
}

void TableScan::_emit(ChunkID chunk_id, ChunkOffset offset) {
  _pos_list->emplace_back(RowID{chunk_id, offset});
}


template <typename Type>
void TableScan::_scan_table() {
  // Scan each segment
  const auto chunk_count = _left_input_table()->chunk_count();
  for (auto chunk_id = ChunkID{0}; chunk_id < chunk_count; ++chunk_id) {
    const auto chunk = _left_input_table()->get_chunk(chunk_id);
    const auto segment = chunk->get_segment(column_id());
    _scan_abstract_segment<Type>(chunk_id, segment);
  }
}

template <typename T>
void TableScan::_scan_abstract_segment(ChunkID chunk_id, std::shared_ptr<AbstractSegment> segment) {
  if (auto value_segment = std::dynamic_pointer_cast<ValueSegment<T>>(segment)) {
    _scan_value_segment(chunk_id, value_segment);
  } else if (auto dictionary_segment = std::dynamic_pointer_cast<DictionarySegment<T>>(segment)) {
    _scan_dictionary_segment(dictionary_segment);
  } else if (auto reference_segment = std::dynamic_pointer_cast<ReferenceSegment>(segment)) {
    _scan_reference_segment<T>(chunk_id, reference_segment);
  } else {
    Fail("Segment type is not supported.");
  }
}

template<typename T>
void TableScan::_scan_value_segment(ChunkID chunk_id, std::shared_ptr<ValueSegment<T>> segment) {
  const auto selector = Selector<T>(scan_type(), type_cast<T>(search_value()));
  const auto segment_size = segment->size();
  for (auto index = ChunkOffset{0}; index < segment_size; ++index) {
    if (segment->is_null(index)) {
      continue;
    }
    const auto value = segment->get(index);
    if (selector.selects(value)) {
      _emit(chunk_id, index);
    }
  }
}

template<typename T>
void TableScan::_scan_dictionary_segment(std::shared_ptr<DictionarySegment<T>>& segment) {
  Fail("Dictionary segment scan not yet implemented.");
}

template<typename T>
void TableScan::_scan_reference_segment(ChunkID chunk_id, std::shared_ptr<ReferenceSegment>& segment) {
  const auto selector = Selector<T>(scan_type(), type_cast<T>(search_value()));
  const auto values_count = segment->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < values_count; ++chunk_offset) {
    // TODO: We are not supposed to do that.
    const auto all_type_variant = segment->operator[](chunk_offset);
    if (variant_is_null(all_type_variant)) {
      continue;
    }
    const auto value = type_cast<T>(all_type_variant);
    if (selector.selects(value)) {
      _emit(chunk_id, chunk_offset);
    }
  }
}


}
