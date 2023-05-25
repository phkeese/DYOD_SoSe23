#include "table_scan.hpp"
#include "../all_type_variant.hpp"
#include "../storage/table.hpp"
#include "../storage/dictionary_segment.hpp"
#include "../storage/value_segment.hpp"
#include "../types.hpp"
#include "resolve_type.hpp"

namespace opossum {

namespace {

/*
 * This should scan an entire table knowing that we should cast every value to T and compare.
 * It should fill another table with matching tuples and return that.
 */
template<typename T>
std::shared_ptr<const Table> scan(const T search_value, const ScanType scan_type, std::shared_ptr<const Table> table) {
  return nullptr;
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
  auto table = std::make_shared<const Table>();

  resolve_data_type(_left_input_table()->column_type(column_id()), [&] (auto type) {
    using Type = typename decltype(type)::type;
    table = scan<Type>(type_cast<Type>(search_value()), scan_type(), _left_input_table());
  });

  const auto chunk_count = _left_input_table()->chunk_count();
  // const auto selector
  for (auto chunk_index = ChunkID{0}; chunk_index < chunk_count; ++chunk_index) {
    const auto chunk = table->get_chunk(chunk_index);
    const auto segment = chunk->get_segment(column_id());
    resolve_data_type(table->column_type(column_id()), [&] (auto type) {
      using Type = typename decltype(type)::type;

      if (const auto typed_value_segment = std::dynamic_pointer_cast<const ValueSegment<Type>>(segment)) {
        const auto &values = typed_value_segment->values();
        const auto selector = Selector<Type>{scan_type(), type_cast<Type>(search_value())};
        for (const auto& value : values) {
          const auto matches = selector.selects(value);
        }
        return;
      }
      if (const auto typed_dictionary_segment = std::dynamic_pointer_cast<const DictionarySegment<Type>>(segment)) {
        // ...
        return;
      }
      Fail("Could not convert AbstractSegment to any type.");
    });
  }
  return table;
  Fail("Not implemented");
}

}