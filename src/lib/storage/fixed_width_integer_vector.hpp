#pragma once

#include "abstract_attribute_vector.hpp"

namespace opossum {

template <typename T>
class FixedWidthIntegerVector : public AbstractAttributeVector {
 public:
  FixedWidthIntegerVector() = default;
  explicit FixedWidthIntegerVector(size_t size);
  explicit FixedWidthIntegerVector(const std::vector<ValueID>& values);
  virtual ~FixedWidthIntegerVector() = default;

  // We need to explicitly set the move constructor to default when we overwrite the copy constructor.
  FixedWidthIntegerVector(FixedWidthIntegerVector&&) = default;
  FixedWidthIntegerVector& operator=(FixedWidthIntegerVector&&) = default;

  // Returns the value id at a given position.
  ValueID get(const size_t index) const override;

  // Sets the value id at a given position.
  void set(const size_t index, const ValueID value_id) override;

  // Returns the number of values.
  size_t size() const override;

  // Returns the width of biggest value id in bytes.
  AttributeVectorWidth width() const override;

 private:
  // Stores ValueIDs for all original elements.
  std::vector<T> _value_ids;
};

std::shared_ptr<AbstractAttributeVector> compress_attribute_vector(const std::vector<ValueID>& attribute_list);
// Explicitly instantiate types
extern template class FixedWidthIntegerVector<uint32_t>;
extern template class FixedWidthIntegerVector<uint16_t>;
extern template class FixedWidthIntegerVector<uint8_t>;

}  // namespace opossum
