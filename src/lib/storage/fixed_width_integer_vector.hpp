#pragma once

#include "abstract_attribute_vector.hpp"

namespace opossum {

template <typename T>
class FixedWidthIntegerVector : public AbstractAttributeVector {
 public:
  FixedWidthIntegerVector() = default;
  FixedWidthIntegerVector(T size);
  virtual ~FixedWidthIntegerVector() = default;

  // We need to explicitly set the move constructor to default when we overwrite the copy constructor.
  FixedWidthIntegerVector(FixedWidthIntegerVector&&) = default;
  FixedWidthIntegerVector& operator=(FixedWidthIntegerVector&&) = default;

  // Returns the value id at a given position.
  virtual ValueID get(const size_t index) const override;

  // Sets the value id at a given position.
  virtual void set(const size_t index, const ValueID value_id) override;

  // Returns the number of values.
  virtual size_t size() const override;

  // Returns the width of biggest value id in bytes.
  virtual AttributeVectorWidth width() const override;

 private:
  // Stores ValueIDs for all original elements.
  std::vector<ValueID> _value_ids;
};

// Explicitly instantiate types
template class FixedWidthIntegerVector<uint32_t>;

}  // namespace opossum