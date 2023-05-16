#include "dictionary_segment.hpp"
#include <map>
#include <set>

#include "fixed_width_integer_vector.hpp"
#include "type_cast.hpp"
#include "utils/assert.hpp"

namespace opossum {

template <typename T>
DictionarySegment<T>::DictionarySegment(const std::shared_ptr<AbstractSegment>& abstract_segment) : _attribute_vector{_compress(abstract_segment)} {}

template <typename T>
std::shared_ptr<AbstractAttributeVector> DictionarySegment<T>::_compress(
    const std::shared_ptr<AbstractSegment>& abstract_segment) {

  // 1. Insert all values into an ordered map
  auto unique_values = std::set<T>();
  const auto size = abstract_segment->size();
  for (auto index = ChunkOffset{0}; index < size; ++index) {
    const auto variant = abstract_segment->operator[](index);
    if (variant_is_null(variant)) {
      continue;
    }
    const auto typed_value = type_cast<T>(variant);
    unique_values.insert(typed_value);
  }

  // 2. Iterate over values again to get index and save that in the attribute_list
  auto attribute_list = std::vector<ValueID>();
  attribute_list.reserve(size);
  for (auto index = ChunkOffset{0}; index < size; ++index) {
    const auto variant = abstract_segment->operator[](index);
    if (variant_is_null(variant)) {
      attribute_list.push_back(null_value_id());
      continue ;
    }
    const auto typed_value = type_cast<T>(variant);
    const auto it = unique_values.find(typed_value);
    DebugAssert(it != unique_values.end(), "Inserted value not in set of unique values.");
    const auto dictionary_index = std::distance(unique_values.begin(), it) + 1; // +1 to account for null_value_id()
    attribute_list.push_back(ValueID(dictionary_index));
  }

  _dictionary.reserve(unique_values.size());
  for (const auto & value : unique_values) {
    _dictionary.push_back(value);
  }

  return _compress_attribute_vector(attribute_list);
}

template <typename T>
std::shared_ptr<AbstractAttributeVector> DictionarySegment<T>::_compress_attribute_vector(
    const std::vector<ValueID>& attribute_list) {
  const auto total_number_of_values = _dictionary.size() + 1; // +1 due to null value
  auto compressed_attribute_list = std::shared_ptr<AbstractAttributeVector>();
  if (total_number_of_values <= std::numeric_limits<uint8_t>::max()) {
    compressed_attribute_list = std::make_shared<FixedWidthIntegerVector<uint8_t>>(attribute_list);
  } else if (total_number_of_values <= std::numeric_limits<uint16_t>::max()) {
    compressed_attribute_list = std::make_shared<FixedWidthIntegerVector<uint16_t>>(attribute_list);
  } else if (total_number_of_values <= std::numeric_limits<uint32_t>::max()) {
    compressed_attribute_list = std::make_shared<FixedWidthIntegerVector<uint32_t>>(attribute_list);
  } else {
    Fail("Too many unique values in dictionary segment (" + std::to_string(total_number_of_values) + " unique values).");
  }
  return compressed_attribute_list;
}

template <typename T>
AllTypeVariant DictionarySegment<T>::operator[](const ChunkOffset chunk_offset) const {
  const auto optional = get_typed_value(chunk_offset);
  if (optional) {
    return optional.value();
  }
  return NULL_VALUE;
}

template <typename T>
T DictionarySegment<T>::get(const ChunkOffset chunk_offset) const {
  const auto optional = get_typed_value(chunk_offset);
  Assert(optional, "Value is NULL.");
  return optional.value();
}

template <typename T>
std::optional<T> DictionarySegment<T>::get_typed_value(const ChunkOffset chunk_offset) const {
  const auto value_id = attribute_vector()->get(chunk_offset);
  if (value_id == null_value_id()) {
    return std::nullopt;
  }
  return value_of_value_id(value_id);
}

template <typename T>
const std::vector<T>& DictionarySegment<T>::dictionary() const {
  return _dictionary;
}

template <typename T>
std::shared_ptr<const AbstractAttributeVector> DictionarySegment<T>::attribute_vector() const {
  return _attribute_vector;
}

template <typename T>
ValueID DictionarySegment<T>::null_value_id() const {
  return ValueID{0};
}

template <typename T>
const T DictionarySegment<T>::value_of_value_id(const ValueID value_id) const {
  DebugAssert(value_id != null_value_id(), "ValueID " + std::to_string(value_id) + " is reserved for NULL values.");
  DebugAssert(value_id < _dictionary.size(), "ValueID " + std::to_string(value_id) + " is out of range.");
  return _dictionary[value_id - 1];
}

template <typename T>
ValueID DictionarySegment<T>::lower_bound(const T value) const {
  const auto it = std::lower_bound(_dictionary.begin(), _dictionary.end(), value);
  if (it == _dictionary.end()) {
    return INVALID_VALUE_ID;
  }
  return ValueID(std::distance(_dictionary.begin(), it));
}

template <typename T>
ValueID DictionarySegment<T>::lower_bound(const AllTypeVariant& value) const {
  return lower_bound(type_cast<T>(value));
}

template <typename T>
ValueID DictionarySegment<T>::upper_bound(const T value) const {
  const auto it = std::upper_bound(_dictionary.begin(), _dictionary.end(), value);
  if (it == _dictionary.end()) {
    return INVALID_VALUE_ID;
  }
  return ValueID(std::distance(_dictionary.begin(), it));
}

template <typename T>
ValueID DictionarySegment<T>::upper_bound(const AllTypeVariant& value) const {
  return upper_bound(type_cast<T>(value));
}

template <typename T>
ChunkOffset DictionarySegment<T>::unique_values_count() const {
  return _dictionary.size();
}

template <typename T>
ChunkOffset DictionarySegment<T>::size() const {
  return ChunkOffset(attribute_vector()->size());
}

template <typename T>
size_t DictionarySegment<T>::estimate_memory_usage() const {
  return sizeof(T) * _dictionary.capacity() + attribute_vector()->width() * attribute_vector()->size();
}

EXPLICITLY_INSTANTIATE_DATA_TYPES(DictionarySegment);

}  // namespace opossum
