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
  return _value_ids[index];
}

template <typename T>
FixedWidthIntegerVector<T>::FixedWidthIntegerVector(T size) : _value_ids{size} {}

}  // namespace opossum