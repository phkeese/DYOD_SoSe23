#pragma once

#include "../all_type_variant.hpp"
#include "../storage/dictionary_segment.hpp"
#include "../storage/reference_segment.hpp"
#include "../storage/value_segment.hpp"
#include "abstract_operator.hpp"
#include "utils/assert.hpp"

namespace opossum {

class TableScan : public AbstractOperator {
 public:
  TableScan(const std::shared_ptr<const AbstractOperator>& in, const ColumnID column_id, const ScanType scan_type,
            const AllTypeVariant search_value);

  inline ColumnID column_id() const {
    return _column_id;
  }

  inline ScanType scan_type() const {
    return _scan_type;
  }

  inline const AllTypeVariant& search_value() const {
    return _search_value;
  }

 protected:
  void _emit(const ChunkID chunk_id, const ChunkOffset offset);
  void _emit(const RowID& row_id);

  std::shared_ptr<const Table> _on_execute() override;
  template <typename T>
  void _scan_table();

  template <typename T>
  void _scan_abstract_segment(const ChunkID chunk_id, const std::shared_ptr<AbstractSegment>& segment);

  template <typename T>
  void _scan_value_segment(const ChunkID chunk_id, const std::shared_ptr<ValueSegment<T>>& segment);

  template <typename T>
  void _scan_dictionary_segment(const ChunkID chunk_id, const std::shared_ptr<DictionarySegment<T>>& segment);

  template <typename T>
  void _scan_reference_segment(const std::shared_ptr<ReferenceSegment>& segment);

  template <typename SegmentType, typename T>
  void _scan_for_null_value(const ChunkID chunk_id, const std::shared_ptr<SegmentType>& segment,
                            const std::shared_ptr<const PosList>& pos_list = nullptr);

  ColumnID _column_id;
  ScanType _scan_type;
  AllTypeVariant _search_value;
  std::shared_ptr<PosList> _pos_list;
  std::shared_ptr<Table> _result_table;
};

// A helper to simplify checking many values against one search value and search type.
template <typename T>
struct Selector {
  Selector(ScanType scan_type, const T search_value) : _scan_type{scan_type}, _search_value{search_value} {}

  // Check whether `other` satisfies the condition specified by the search value and the scan type.
  bool selects(const T& other) const {
    switch (_scan_type) {
      case ScanType::OpEquals:
        return other == _search_value;
      case ScanType::OpNotEquals:
        return other != _search_value;
      case ScanType::OpGreaterThan:
        return other > _search_value;
      case ScanType::OpGreaterThanEquals:
        return other >= _search_value;
      case ScanType::OpLessThan:
        return other < _search_value;
      case ScanType::OpLessThanEquals:
        return other <= _search_value;
      default:
        Fail("Could not match any possible ScanType.");
    }
  }

 private:
  ScanType _scan_type;
  T _search_value;
};

// A helper to scan in a DictionarySegment using the dictionary and lower/upper bound.
template <typename T, typename EmitCallback>
struct DictionarySegmentSelector {
  DictionarySegmentSelector(const ScanType scan_type, const T search_value, const EmitCallback emit_callback,
                            const std::shared_ptr<const AbstractAttributeVector>& value_ids,
                            const ValueID null_value_id)
      : _scan_type{scan_type},
        _search_value{search_value},
        _emit_callback{emit_callback},
        _value_ids{value_ids},
        _null_value_id{null_value_id} {}

  // Emit all positions that match the condition using the emit_callback.
  void select(const ValueID lower_bound_value_id, const ValueID upper_bound_value_id) const {
    // When the bounds are equal, the search_value is not in the segment.
    const auto search_value_not_in_segment = upper_bound_value_id == lower_bound_value_id;
    // We heavily rely on the fact that the dictionary only contains distinct values.
    // This allows us to simplify some of the conditions.
    switch (_scan_type) {
      case ScanType::OpEquals:
        // In case the search_value is not in the segment, there is nothing to emit for OpEquals.
        if (!search_value_not_in_segment) {
          _select_in_dictionary_segment([&](const ValueID value_id) { return value_id == lower_bound_value_id; });
        }
        break;
      case ScanType::OpNotEquals:
        _select_in_dictionary_segment([&](const ValueID value_id) {
          // When the search_value is not in the segment, every value has to be emitted.
          return search_value_not_in_segment || value_id != lower_bound_value_id;
        });
        break;
      case ScanType::OpGreaterThan:
        _select_in_dictionary_segment([&](const ValueID value_id) { return value_id >= upper_bound_value_id; });
        break;
      case ScanType::OpGreaterThanEquals:
        _select_in_dictionary_segment([&](const ValueID value_id) { return value_id >= lower_bound_value_id; });
        break;
      case ScanType::OpLessThan:
        _select_in_dictionary_segment([&](const ValueID value_id) { return value_id < lower_bound_value_id; });
        break;
      case ScanType::OpLessThanEquals:
        _select_in_dictionary_segment([&](const ValueID value_id) { return value_id < upper_bound_value_id; });
        break;
      default:
        Fail("Could not match any possible ScanType.");
    }
  }

 private:
  // A little helper to encapsulate the looping.
  template <typename ConditionCallback>
  void _select_in_dictionary_segment(const ConditionCallback condition_callback) const {
    const auto value_id_count = _value_ids->size();
    for (auto chunk_offset = ChunkOffset{0}; chunk_offset < value_id_count; ++chunk_offset) {
      const auto value_id = _value_ids->get(chunk_offset);
      if (value_id != _null_value_id && condition_callback(value_id)) {
        _emit_callback(chunk_offset);
      }
    }
  }

  ScanType _scan_type;
  T _search_value;
  EmitCallback _emit_callback;
  std::shared_ptr<const AbstractAttributeVector> _value_ids;
  ValueID _null_value_id;
};

}  // namespace opossum
