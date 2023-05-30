#include <optional>
#include "table_scan.hpp"
#include "resolve_type.hpp"

namespace opossum {

namespace {

template<typename ConditionCallback, typename EmitCallback>
void _select_in_dictionary_segment(
    const std::shared_ptr<const AbstractAttributeVector>& value_ids,
    const ValueID null_value_id,
    const ConditionCallback condition_callback,
    const EmitCallback emit_callback) {
  const auto value_id_count = value_ids->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < value_id_count; ++chunk_offset) {
    const auto value_id = value_ids->get(chunk_offset);
    if (value_id != null_value_id && condition_callback(value_id)) {
      emit_callback(chunk_offset);
    }
  }
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
  }

  resolve_data_type(_left_input_table()->column_type(column_id()), [this] (auto type) {
    using Type = typename decltype(type)::type;
    _scan_table<Type>();
  });

  const auto chunk = input->get_chunk(ChunkID{0});
  for (auto column_id = ColumnID{0}; column_id < column_count; ++column_id) {
    const auto current_segment = std::dynamic_pointer_cast<ReferenceSegment>(chunk->get_segment(column_id));
    const auto current_input = current_segment ? current_segment->referenced_table() : input;
    const auto reference_segment = std::make_shared<ReferenceSegment>(current_input, column_id, _pos_list);
    initial_result_chunk->add_segment(reference_segment);
  }
  return _result_table;
}

void TableScan::_emit(ChunkID chunk_id, ChunkOffset offset) {
  _pos_list->emplace_back(RowID{chunk_id, offset});
}

void TableScan::_emit(const RowID& row_id) {
  _pos_list->emplace_back(row_id);
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
void TableScan::_scan_abstract_segment(ChunkID chunk_id, const std::shared_ptr<AbstractSegment>& segment) {
  if (auto value_segment = std::dynamic_pointer_cast<ValueSegment<T>>(segment)) {
    _scan_value_segment(chunk_id, value_segment);
  } else if (auto dictionary_segment = std::dynamic_pointer_cast<DictionarySegment<T>>(segment)) {
    _scan_dictionary_segment(chunk_id, dictionary_segment);
  } else if (auto reference_segment = std::dynamic_pointer_cast<ReferenceSegment>(segment)) {
    _scan_reference_segment<T>(chunk_id, reference_segment);
  } else {
    Fail("Segment type is not supported.");
  }
}

template<typename T>
void TableScan::_scan_value_segment(ChunkID chunk_id, const std::shared_ptr<ValueSegment<T>>& segment) {
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
void TableScan::_scan_dictionary_segment(ChunkID chunk_id, const std::shared_ptr<DictionarySegment<T>>& segment) {
  const auto value_ids = segment->attribute_vector();
  const auto dictionary = segment->dictionary();

  const auto upper_bound_value_id = segment->upper_bound(search_value());
  const auto lower_bound_value_id = segment->lower_bound(search_value());
  const auto lower_bound_equals_upper_bound = upper_bound_value_id == lower_bound_value_id;

  const auto emit_callback = [this, &chunk_id](const ChunkOffset chunk_offset) {
    _emit(chunk_id, chunk_offset);
  };

  switch (_scan_type) {
    case ScanType::OpEquals:
      if (!lower_bound_equals_upper_bound) {
        _select_in_dictionary_segment(
            value_ids,
            segment->null_value_id(),
            [&](const ValueID value_id) {
              return value_id == lower_bound_value_id;
            },
            emit_callback);
      }
      break;
    case ScanType::OpNotEquals:
      _select_in_dictionary_segment(
          value_ids,
          segment->null_value_id(),
          [&](const ValueID value_id) {
            return lower_bound_equals_upper_bound || value_id != lower_bound_value_id;
          },
          emit_callback);
      break;
    case ScanType::OpGreaterThan:
      _select_in_dictionary_segment(
          value_ids,
          segment->null_value_id(),
          [&](const ValueID value_id) {
            return value_id >= upper_bound_value_id;
          },
          emit_callback);
      break;
    case ScanType::OpGreaterThanEquals:
      _select_in_dictionary_segment(
          value_ids,
          segment->null_value_id(),
          [&](const ValueID value_id) {
            return value_id >= lower_bound_value_id;
          },
          emit_callback);
      break;
    case ScanType::OpLessThan:
      _select_in_dictionary_segment(
          value_ids,
          segment->null_value_id(),
          [&](const ValueID value_id) {
            return value_id < lower_bound_value_id;
          },
          emit_callback);
      break;
    case ScanType::OpLessThanEquals:
      _select_in_dictionary_segment(
          value_ids,
          segment->null_value_id(),
          [&](const ValueID value_id) {
            return value_id < upper_bound_value_id;
          },
          emit_callback);
      break;
    default:
      // TODO: Include _scan_type in error message
      Fail("Could not match any possible ScanType. ScanType with name " + typeid(_search_value).name() + " is not supported.");
      break;
  }
}

template<typename T>
void TableScan::_scan_reference_segment(ChunkID chunk_id, const std::shared_ptr<ReferenceSegment>& segment) {
  const auto selector = Selector<T>(scan_type(), type_cast<T>(search_value()));
  const auto values_count = segment->size();
  const auto pos_list = segment->pos_list();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < values_count; ++chunk_offset) {
    const auto value = segment->_get_typed_value<T>(chunk_offset);
    if (value && selector.selects(value.value())) {
      _emit(pos_list->operator[](chunk_offset));
    }
  }
}


}
