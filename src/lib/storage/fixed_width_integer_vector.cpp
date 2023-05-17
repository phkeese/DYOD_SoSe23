#include "fixed_width_integer_vector.hpp"
#include <memory>
#include "utils/assert.hpp"

namespace opossum {

template <typename T>
void FixedWidthIntegerVector<T>::set(const size_t index, const ValueID value_id) {
  Assert(index < _value_ids.size(), "Index " + std::to_string(index) + " is out of range.");
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
  Assert(index < _value_ids.size(), "Index " + std::to_string(index) + " is out of range.");
  return ValueID{_value_ids[index]};
}

template <typename T>
FixedWidthIntegerVector<T>::FixedWidthIntegerVector(size_t size) : _value_ids(size) {}

template <typename T>
FixedWidthIntegerVector<T>::FixedWidthIntegerVector(const std::vector<ValueID>& values) {
  _value_ids.reserve(values.size());
  for (const auto value : values) {
    const auto as_t = T(value);
    _value_ids.push_back(as_t);
  }
}

std::shared_ptr<AbstractAttributeVector> compress_attribute_vector(const std::vector<ValueID>& attribute_list) {
  const auto max_element = std::max_element(attribute_list.begin(), attribute_list.end());
  if (max_element == attribute_list.end()) {
    return std::make_shared<FixedWidthIntegerVector<uint8_t>>();
  }
  // +1 due to null value
  const auto total_number_of_values = static_cast<size_t>(*max_element) + 1;
  auto compressed_attribute_list = std::shared_ptr<AbstractAttributeVector>();

  if (total_number_of_values <= std::numeric_limits<uint8_t>::max()) {
    compressed_attribute_list = std::make_shared<FixedWidthIntegerVector<uint8_t>>(attribute_list);
  } else if (total_number_of_values <= std::numeric_limits<uint16_t>::max()) {
    compressed_attribute_list = std::make_shared<FixedWidthIntegerVector<uint16_t>>(attribute_list);
  } else if (total_number_of_values <= std::numeric_limits<uint32_t>::max()) {
    compressed_attribute_list = std::make_shared<FixedWidthIntegerVector<uint32_t>>(attribute_list);
  } else {
    Fail("Too many unique values in dictionary segment (" + std::to_string(total_number_of_values) +
         " unique values).");
  }
  return compressed_attribute_list;
}

template class FixedWidthIntegerVector<uint32_t>;
template class FixedWidthIntegerVector<uint16_t>;
template class FixedWidthIntegerVector<uint8_t>;

}  // namespace opossum
