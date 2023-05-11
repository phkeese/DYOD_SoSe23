#include "value_segment.hpp"

#include "type_cast.hpp"
#include "utils/assert.hpp"

namespace opossum {

template <typename T>
ValueSegment<T>::ValueSegment(bool nullable) : _is_nullable{nullable} {}

template <typename T>
AllTypeVariant ValueSegment<T>::operator[](const ChunkOffset chunk_offset) const {
  const auto value = get_typed_value(chunk_offset);
  if (value) {
    return value.value();
  }
  return NULL_VALUE;
}

template <typename T>
bool ValueSegment<T>::is_null(const ChunkOffset chunk_offset) const {
  Assert(chunk_offset < _null_values.size(),
         "Null value at offset " + std::to_string(chunk_offset) + " does not exist.");
  return _is_nullable && _null_values[chunk_offset];
}

template <typename T>
T ValueSegment<T>::get(const ChunkOffset chunk_offset) const {
  const auto optional = get_typed_value(chunk_offset);
  Assert(optional, "Value is NULL.");
  return optional.value();
}

template <typename T>
std::optional<T> ValueSegment<T>::get_typed_value(const ChunkOffset chunk_offset) const {
  if (is_null(chunk_offset)) {
    return std::nullopt;
  }
  Assert(chunk_offset < _values.size(), "Value at offset " + std::to_string(chunk_offset) + " does not exist.");
  return _values[chunk_offset];
}

template <typename T>
void ValueSegment<T>::append(const AllTypeVariant& value) {
  const auto is_null = variant_is_null(value);
  if (is_null && is_nullable()) {
    _null_values.push_back(true);
    _values.push_back(T{});
  } else {
    Assert(!is_null, "Cannot append null value to non-nullable ValueSegment.");
    try {
      auto typed_value = type_cast<T>(value);
      _values.push_back(typed_value);
      if (is_nullable()) {
        _null_values.push_back(false);
      }
    } catch (const boost::wrapexcept<boost::bad_lexical_cast>& e) {
      throw std::logic_error{e.what()};
    }
  }
}

template <typename T>
ChunkOffset ValueSegment<T>::size() const {
  return _values.size();
}

template <typename T>
const std::vector<T>& ValueSegment<T>::values() const {
  return _values;
}

template <typename T>
bool ValueSegment<T>::is_nullable() const {
  return _is_nullable;
}

template <typename T>
const std::vector<bool>& ValueSegment<T>::null_values() const {
  Assert(is_nullable(), "ValueSegment is not nullable.");
  return _null_values;
}

template <typename T>
size_t ValueSegment<T>::estimate_memory_usage() const {
  return values().size() * sizeof(T);
}

// Macro to instantiate the following classes:
// template class ValueSegment<int32_t>;
// template class ValueSegment<int64_t>;
// template class ValueSegment<float>;
// template class ValueSegment<double>;
// template class ValueSegment<std::string>;
EXPLICITLY_INSTANTIATE_DATA_TYPES(ValueSegment);

}  // namespace opossum
