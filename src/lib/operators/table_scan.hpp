#pragma once

#include "../all_type_variant.hpp"
#include "abstract_operator.hpp"
#include "utils/assert.hpp"

namespace opossum {

class TableScan : public AbstractOperator {
 public:
  TableScan(const std::shared_ptr<const AbstractOperator>& in, const ColumnID column_id, const ScanType scan_type,
            const AllTypeVariant search_value);

  ColumnID column_id() const;

  ScanType scan_type() const;

  const AllTypeVariant& search_value() const;

 protected:
  std::shared_ptr<const Table> _on_execute() override;

  ColumnID _column_id;
  ScanType _scan_type;
  AllTypeVariant _search_value;
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

 private:
  ScanType _scan_type;
  T _search_value;
};



}  // namespace opossum
