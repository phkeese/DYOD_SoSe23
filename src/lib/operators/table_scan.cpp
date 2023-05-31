#include "table_scan.hpp"
#include "resolve_type.hpp"

namespace opossum {

TableScan::TableScan(const std::shared_ptr<const AbstractOperator>& in, const ColumnID column_id,
                     const ScanType scan_type, const AllTypeVariant search_value)
    : AbstractOperator(in), _column_id{column_id}, _scan_type{scan_type}, _search_value{search_value} {
    DebugAssert(_left_input != nullptr, "TableScan operator requires a left input.");
    DebugAssert(_right_input == nullptr, "There should not be a right input for the TableScan operator.");
}


std::shared_ptr<const Table> TableScan::_on_execute() {
  _result_table = std::make_shared<Table>();
  // A PosList that is shared between all columns of the result table.
  _pos_list = std::make_shared<PosList>();

  // Creates all columns of old table in the result table and Initializes the result chunks.
  const auto input = _left_input_table();
  const auto column_count = input->column_count();

  for (auto column_id = ColumnID{0}; column_id < column_count; ++column_id) {
    _result_table->add_column_definition(input->column_name(column_id), input->column_type(column_id),
                                         input->column_nullable(column_id));
  }

  // Find the correct data type and scan with this knowledge.
  resolve_data_type(_left_input_table()->column_type(column_id()), [this] (auto type) {
    using Type = typename decltype(type)::type;
    _scan_table<Type>();
  });

  // Add ReferenceSegments using the shared PosList.
  const auto initial_input_chunk = input->get_chunk(ChunkID{0});
  const auto initial_result_chunk = _result_table->get_chunk(ChunkID{0});
  for (auto column_id = ColumnID{0}; column_id < column_count; ++column_id) {
    const auto segment = initial_input_chunk->get_segment(column_id);
    const auto segment_as_reference_segment = std::dynamic_pointer_cast<ReferenceSegment>(segment);
    // Use the referenced table of the reference segment, in case we have one.
    // Otherwise, use the provided input table as the basis.
    const auto current_input = segment_as_reference_segment ? segment_as_reference_segment->referenced_table() : input;
    const auto reference_segment = std::make_shared<ReferenceSegment>(current_input, column_id, _pos_list);
    initial_result_chunk->add_segment(reference_segment);
  }
  return _result_table;
}

void TableScan::_emit(const ChunkID chunk_id, const ChunkOffset offset) {
  _emit(RowID{chunk_id, offset});
}

void TableScan::_emit(const RowID& row_id) {
  _pos_list->emplace_back(row_id);
}

template <typename Type>
void TableScan::_scan_table() {
  // Scan each segment.
  const auto chunk_count = _left_input_table()->chunk_count();
  for (auto chunk_id = ChunkID{0}; chunk_id < chunk_count; ++chunk_id) {
    const auto chunk = _left_input_table()->get_chunk(chunk_id);
    // Get the segment that contains the values that should be filtered.
    const auto segment = chunk->get_segment(column_id());
    _scan_abstract_segment<Type>(chunk_id, segment);
  }
}

template <typename T>
void TableScan::_scan_abstract_segment(const ChunkID chunk_id, const std::shared_ptr<AbstractSegment>& segment) {
  if (const auto value_segment = std::dynamic_pointer_cast<ValueSegment<T>>(segment)) {
    _scan_value_segment(chunk_id, value_segment);
  } else if (const auto dictionary_segment = std::dynamic_pointer_cast<DictionarySegment<T>>(segment)) {
    _scan_dictionary_segment(chunk_id, dictionary_segment);
  } else if (const auto reference_segment = std::dynamic_pointer_cast<ReferenceSegment>(segment)) {
    _scan_reference_segment<T> (reference_segment);
  } else {
    Fail("Segment type is not supported.");
  }
}

template<typename T>
void TableScan::_scan_value_segment(const ChunkID chunk_id, const std::shared_ptr<ValueSegment<T>>& segment) {
  if (variant_is_null(search_value())) {
    _scan_for_null_value<ValueSegment<T>, T>(chunk_id, segment);
    return;
  }
  const auto selector = Selector<T>(scan_type(), type_cast<T>(search_value()));
  const auto segment_size = segment->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < segment_size; ++chunk_offset) {
    const auto value = segment->get_typed_value(chunk_offset);
    // We don't want to match NULL, regardless of the condition.
    if (value && selector.selects(value.value())) {
      _emit(chunk_id, chunk_offset);
    }
  }
}

template<typename T>
void TableScan::_scan_dictionary_segment(const ChunkID chunk_id, const std::shared_ptr<DictionarySegment<T>>& segment) {
  if (variant_is_null(search_value())) {
    _scan_for_null_value<DictionarySegment<T>, T>(chunk_id, segment);
    return;
  }
  const auto value_ids = segment->attribute_vector();

  const auto lower_bound_value_id = segment->lower_bound(search_value());
  const auto upper_bound_value_id = segment->upper_bound(search_value());

  const auto emit_callback = [this, &chunk_id](const ChunkOffset chunk_offset) {
    _emit(chunk_id, chunk_offset);
  };

  const auto selector =
      DictionarySegmentSelector(scan_type(), search_value(), emit_callback, value_ids, segment->null_value_id());
  selector.select(lower_bound_value_id, upper_bound_value_id);
}

template<typename T>
void TableScan::_scan_reference_segment(const std::shared_ptr<ReferenceSegment>& segment) {
  const auto segment_size = segment->size();
  const auto pos_list = segment->pos_list();

  if (variant_is_null(search_value())) {
    _scan_for_null_value<ReferenceSegment, T>(ChunkID{0}, segment, pos_list);
    return;
  }

  const auto selector = Selector<T>(scan_type(), type_cast<T>(search_value()));
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < segment_size; ++chunk_offset) {
    const auto value = segment->_get_typed_value<T>(chunk_offset);
    // We don't want to match NULL, regardless of the condition.
    if (value && selector.selects(value.value())) {
      // Emit using the existing PosList.
      // This way, we are able to omit ReferenceSegments referencing ReferenceSegments.
      _emit(pos_list->operator[](chunk_offset));
    }
  }
}

template<typename SegmentType, typename T>
void TableScan::_scan_for_null_value(const ChunkID chunk_id, const std::shared_ptr<SegmentType>& segment, const std::shared_ptr<const PosList>& pos_list) {
  if (scan_type() != ScanType::OpNotEquals) {
    return;
  }
  const auto segment_size = segment->size();
  for (auto chunk_offset = ChunkOffset{0}; chunk_offset < segment_size; ++chunk_offset) {
    if constexpr (std::is_base_of<ReferenceSegment, SegmentType>::value) {
      const auto value = segment->template _get_typed_value<T>(chunk_offset);
      if (value) {
        _emit(pos_list->operator[](chunk_offset));
      }
    } else {
      // Don't use early return, because the compiler would look for get_typed_value in ReferenceSegment,
      // which result in an error.
      const auto value = segment->get_typed_value(chunk_offset);
      if (value) {
        _emit(chunk_id, chunk_offset);
      }
    }
  }
}

}  // namespace opossum
