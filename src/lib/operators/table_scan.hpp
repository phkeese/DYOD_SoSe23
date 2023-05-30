#pragma once

#include "../all_type_variant.hpp"
#include "abstract_operator.hpp"
#include "utils/assert.hpp"
#include "../storage/value_segment.hpp"
#include "../storage/dictionary_segment.hpp"
#include "../storage/reference_segment.hpp"

namespace opossum {

class TableScan : public AbstractOperator {
 public:
  TableScan(const std::shared_ptr<const AbstractOperator>& in, const ColumnID column_id, const ScanType scan_type,
            const AllTypeVariant search_value);

  inline ColumnID column_id() const { return _column_id; }

  inline ScanType scan_type() const { return _scan_type; }

  inline const AllTypeVariant& search_value() const { return _search_value; }

 protected:
  void _emit(ChunkID chunk_id, ChunkOffset offset);

  std::shared_ptr<const Table> _on_execute() override;
  template<typename T>
  void _scan_table();

  template<typename T>
  void _scan_abstract_segment(ChunkID chunk_id, std::shared_ptr<AbstractSegment> segment);

  template<typename T>
  void _scan_value_segment(ChunkID chunk_id, std::shared_ptr<ValueSegment<T>> segment);

  template<typename T>
  void _scan_dictionary_segment(ChunkID chunk_id, std::shared_ptr<DictionarySegment<T>>& segment);

  template<typename T>
  void _scan_reference_segment(ChunkID chunk_id, std::shared_ptr<ReferenceSegment>& segment);

  ColumnID _column_id;
  ScanType _scan_type;
  AllTypeVariant _search_value;
  std::shared_ptr<PosList> _pos_list;
  std::shared_ptr<Table> _result_table;
};

template<typename T>
struct Selector {
  Selector(ScanType scan_type, const T search_value) : _scan_type{scan_type} , _search_value{search_value} {}
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
        // TODO: Better error, with value
        Fail("Could not match any possible ScanType. ScanType with name " + typeid(_search_value).name() + " is not supported.");
    }
  }

  T search_value() const {
    return _search_value;
  }

  ScanType scan_type() const {
    return _scan_type;
  }

 private:
  ScanType _scan_type;
  T _search_value;
};

}  // namespace opossum
