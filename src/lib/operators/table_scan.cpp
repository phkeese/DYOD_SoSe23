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
    const auto value = segment_values[chunk_offset];
    if (selector.selects(value)) {
      pos_list->emplace_back(RowID{chunk_id, chunk_offset});
    }
  }
}

template<typename T>
void scan_in_dictionary_segment(const Selector<T>& selector, const ChunkID chunk_id, const std::shared_ptr<const DictionarySegment<T>>& segment, const std::shared_ptr<PosList>& pos_list) {
  const auto values_count = segment->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < values_count; ++chunk_offset) {
    const auto value = segment->get(chunk_offset);
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
    DebugAssert(_right_input == nullptr, "There should not be a right input for the table scan operator.");
}

ColumnID TableScan::column_id() const {
  return _column_id;
}

ScanType TableScan::scan_type() const {
  return _scan_type;
}

const AllTypeVariant& TableScan::search_value() const {
  return _search_value;
}

std::shared_ptr<const Table> TableScan::_on_execute() {
  auto table = std::shared_ptr<const Table>(nullptr);

  resolve_data_type(_left_input_table()->column_type(column_id()), [this, &table] (auto type) {
    using Type = typename decltype(type)::type;
    table = scan<Type>(
        type_cast<Type>(search_value()),
        scan_type(),
        column_id(),
        _left_input_table()
    );
  });

//  const auto chunk_count = _left_input_table()->chunk_count();
//  // const auto selector
//  for (auto chunk_index = ChunkID{0}; chunk_index < chunk_count; ++chunk_index) {
//    const auto chunk = table->get_chunk(chunk_index);
//    const auto segment = chunk->get_segment(column_id());
//    resolve_data_type(table->column_type(column_id()), [&] (auto type) {
//      using Type = typename decltype(type)::type;
//
//      if (const auto typed_value_segment = std::dynamic_pointer_cast<const ValueSegment<Type>>(segment)) {
//        const auto &values = typed_value_segment->values();
//        const auto selector = Selector<Type>{scan_type(), type_cast<Type>(search_value())};
//        for (const auto& value : values) {
//          const auto matches = selector.selects(value);
//        }
//        return;
//      }
//      if (const auto typed_dictionary_segment = std::dynamic_pointer_cast<const DictionarySegment<Type>>(segment)) {
//        // ...
//        return;
//      }
//      Fail("Could not convert AbstractSegment to any type.");
//    });
//  }
  return table;
}

}