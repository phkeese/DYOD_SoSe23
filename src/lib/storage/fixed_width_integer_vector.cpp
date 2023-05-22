#include "fixed_width_integer_vector.hpp"
#include <memory>
#include <bit>
#include "utils/assert.hpp"

namespace opossum {

template <typename T>
FixedWidthIntegerVector<T>::FixedWidthIntegerVector(size_t size) : _value_ids(size) {}

template <typename T>
FixedWidthIntegerVector<T>::FixedWidthIntegerVector(const std::vector<ValueID>& values) {
  _value_ids.reserve(values.size());
  for (const auto value : values) {
    Assert(value <= std::numeric_limits<T>::max(),
           "ValueID " + std::to_string(value) + " is too large for FixedWidthIntegerVector with width " +
           std::to_string(std::bit_width(std::numeric_limits<T>::max())));
    const auto as_t = T(value);
    _value_ids.push_back(as_t);
  }
}

template <typename T>
void FixedWidthIntegerVector<T>::set(const size_t index, const ValueID value_id) {
  Assert(index < size(), "Index " + std::to_string(index) + " is out of range.");
  _value_ids[index] = value_id;
}

template <typename T>
size_t FixedWidthIntegerVector<T>::size() const {
  return _value_ids.size();
}

template <typename T>
AttributeVectorWidth FixedWidthIntegerVector<T>::width() const {
  return sizeof(T);
}

template <typename T>
ValueID FixedWidthIntegerVector<T>::get(const size_t index) const {
  Assert(index < size(), "Index " + std::to_string(index) + " is out of range.");
  return ValueID{_value_ids[index]};
}

std::shared_ptr<AbstractAttributeVector> compress_attribute_vector(const std::vector<ValueID>& value_ids) {
  if (value_ids.empty()) {
    return std::make_shared<FixedWidthIntegerVector<uint8_t>>();
  }

  const auto max_element = *std::max_element(value_ids.begin(), value_ids.end());
  Assert(max_element < INVALID_VALUE_ID, "Maximum ValueID is too large.");

  auto compressed_value_ids = std::shared_ptr<AbstractAttributeVector>();

  if (max_element <= std::numeric_limits<uint8_t>::max()) {
    compressed_value_ids = std::make_shared<FixedWidthIntegerVector<uint8_t>>(value_ids);
  } else if (max_element <= std::numeric_limits<uint16_t>::max()) {
    compressed_value_ids = std::make_shared<FixedWidthIntegerVector<uint16_t>>(value_ids);
  } else if (max_element <= std::numeric_limits<uint32_t>::max()) {
    compressed_value_ids = std::make_shared<FixedWidthIntegerVector<uint32_t>>(value_ids);
  } else {
    Fail("Too many unique values in dictionary segment (" + std::to_string(max_element) +
         " unique values).");
  }
  return compressed_value_ids;
}

template class FixedWidthIntegerVector<uint32_t>;
template class FixedWidthIntegerVector<uint16_t>;
template class FixedWidthIntegerVector<uint8_t>;

}  // namespace opossum
