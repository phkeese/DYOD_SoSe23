#include "fixed_width_integer_vector.hpp"
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

template class FixedWidthIntegerVector<uint32_t>;
template class FixedWidthIntegerVector<uint16_t>;
template class FixedWidthIntegerVector<uint8_t>;

}  // namespace opossum
