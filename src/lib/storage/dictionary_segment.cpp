#include "dictionary_segment.hpp"
#include <set>

#include "fixed_width_integer_vector.hpp"
#include "type_cast.hpp"
#include "utils/assert.hpp"
#include "abstract_attribute_vector.hpp"

namespace opossum {

template <typename T>
DictionarySegment<T>::DictionarySegment(const std::shared_ptr<AbstractSegment>& abstract_segment) {
  _compress(abstract_segment);
}

template <typename T>
void DictionarySegment<T>::_compress(const std::shared_ptr<AbstractSegment>& abstract_segment) {
  _create_dictionary(abstract_segment);
  _create_attribute_vector(abstract_segment);
}

template <typename T>
void DictionarySegment<T>::_create_dictionary(const std::shared_ptr<AbstractSegment>& abstract_segment) {
  auto unique_values = std::set<T>();
  const auto segment_size = abstract_segment->size();

  // Insert all values into a set.
  for (auto index = ChunkOffset{0}; index < segment_size; ++index) {
    const auto variant = abstract_segment->operator[](index);
    if (variant_is_null(variant)) {
      continue;
    }
    const auto typed_value = type_cast<T>(variant);
    unique_values.insert(typed_value);
  }

  // Create the sorted dictionary from set.
  _dictionary.reserve(unique_values.size());
  for (const auto& value : unique_values) {
    _dictionary.push_back(value);
  }
}

template <typename T>
void DictionarySegment<T>::_create_attribute_vector(const std::shared_ptr<AbstractSegment>& abstract_segment) {
  auto value_ids = std::vector<ValueID>();
  const auto segment_size = abstract_segment->size();
  value_ids.reserve(segment_size);

  // Iterate over values again to get index and save that in the value_ids.
  for (auto index = ChunkOffset{0}; index < segment_size; ++index) {
    const auto variant = abstract_segment->operator[](index);
    if (variant_is_null(variant)) {
      value_ids.push_back(null_value_id());
      continue;
    }
    const auto typed_value = type_cast<T>(variant);
    const auto value_id = lower_bound(typed_value);
    DebugAssert(value_id != INVALID_VALUE_ID, "Inserted value not in the set of unique values.");
    value_ids.push_back(value_id);
  }
  _attribute_vector = compress_attribute_vector(value_ids);
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
  Assert(optional, "Value at offset " + std::to_string(chunk_offset) + " is NULL.");
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
  return ValueID(dictionary().size());
}

template <typename T>
const T DictionarySegment<T>::value_of_value_id(const ValueID value_id) const {;
  DebugAssert(value_id < dictionary().size(), "ValueID " + std::to_string(value_id) + " is out of range.");
  return dictionary()[value_id];
}

template <typename T>
ValueID DictionarySegment<T>::lower_bound(const T value) const {
  const auto it = std::lower_bound(dictionary().begin(), dictionary().end(), value);
  if (it == dictionary().end()) {
    return INVALID_VALUE_ID;
  }
  return ValueID(std::distance(dictionary().begin(), it));
}

template <typename T>
ValueID DictionarySegment<T>::lower_bound(const AllTypeVariant& value) const {
  Assert(!variant_is_null(value), "Cannot get lower bound of null value.");
  return ValueID(lower_bound(type_cast<T>(value)));
}

template <typename T>
ValueID DictionarySegment<T>::upper_bound(const T value) const {
  const auto it = std::upper_bound(dictionary().begin(), dictionary().end(), value);
  if (it == dictionary().end()) {
    return INVALID_VALUE_ID;
  }
  return ValueID(std::distance(dictionary().begin(), it));
}

template <typename T>
ValueID DictionarySegment<T>::upper_bound(const AllTypeVariant& value) const {
  Assert(!variant_is_null(value), "Cannot get upper bound of null value.");
  return upper_bound(type_cast<T>(value));
}

template <typename T>
ChunkOffset DictionarySegment<T>::unique_values_count() const {
  return ChunkOffset(dictionary().size());
}

template <typename T>
ChunkOffset DictionarySegment<T>::size() const {
  return ChunkOffset(attribute_vector()->size());
}

template <typename T>
size_t DictionarySegment<T>::estimate_memory_usage() const {
  return sizeof(T) * dictionary().capacity() + attribute_vector()->width() * size();
}

EXPLICITLY_INSTANTIATE_DATA_TYPES(DictionarySegment);

}  // namespace opossum
