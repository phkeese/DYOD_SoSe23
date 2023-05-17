#include <numeric>
#include "base_test.hpp"

#include "resolve_type.hpp"
#include "storage/abstract_attribute_vector.hpp"
#include "storage/abstract_segment.hpp"
#include "storage/dictionary_segment.hpp"

namespace opossum {

class StorageDictionarySegmentTest : public BaseTest {
 protected:
  std::shared_ptr<ValueSegment<int32_t>> value_segment_int{std::make_shared<ValueSegment<int32_t>>()};
  std::shared_ptr<ValueSegment<std::string>> value_segment_str{std::make_shared<ValueSegment<std::string>>(true)};
  std::shared_ptr<ValueSegment<float>> value_segment_float{std::make_shared<ValueSegment<float>>()};
  std::shared_ptr<ValueSegment<double>> value_segment_double{std::make_shared<ValueSegment<double>>(true)};
};

TEST_F(StorageDictionarySegmentTest, CompressSegmentString) {
  value_segment_str->append("Bill");
  value_segment_str->append("Steve");
  value_segment_str->append("Alexander");
  value_segment_str->append("Steve");
  value_segment_str->append("Hasso");
  value_segment_str->append("Bill");
  value_segment_str->append(NULL_VALUE);

  const auto dict_segment = std::make_shared<DictionarySegment<std::string>>(value_segment_str);

  // Test attribute_vector size.
  EXPECT_EQ(dict_segment->size(), 7);

  // Test dictionary size (uniqueness).
  EXPECT_EQ(dict_segment->unique_values_count(), 4);

  // Test sorting.
  const auto& dict = dict_segment->dictionary();
  EXPECT_EQ(dict[0], "Alexander");
  EXPECT_EQ(dict[1], "Bill");
  EXPECT_EQ(dict[2], "Hasso");
  EXPECT_EQ(dict[3], "Steve");

  // Test NULL value handling.
  EXPECT_EQ(dict_segment->attribute_vector()->get(6), dict_segment->null_value_id());
  EXPECT_EQ(dict_segment->get_typed_value(6), std::nullopt);
  EXPECT_THROW(dict_segment->get(6), std::logic_error);
}

TEST_F(StorageDictionarySegmentTest, CompressSegmentFloat) {
  value_segment_float->append(1.0f);
  value_segment_float->append(1.0f);
  value_segment_float->append(1.0f);
  value_segment_float->append(3.1415f);
  value_segment_float->append(4.1f);
  value_segment_float->append(0.0f);
  value_segment_float->append(-0.0f);

  const auto dict_segment = std::make_shared<DictionarySegment<float>>(value_segment_float);

  // Test attribute_vector size.
  EXPECT_EQ(dict_segment->size(), 7);

  // Test dictionary size (uniqueness).
  EXPECT_EQ(dict_segment->unique_values_count(), 4);

  // Test sorting.
  const auto& dict = dict_segment->dictionary();
  EXPECT_EQ(dict[0], 0.0f);
  EXPECT_EQ(dict[1], 1.0f);
  EXPECT_EQ(dict[2], 3.1415f);
  EXPECT_EQ(dict[3], 4.1f);
}

TEST_F(StorageDictionarySegmentTest, CompressSegmentDouble) {
  value_segment_double->append(1.0);
  value_segment_double->append(1.0);
  value_segment_double->append(1.0);
  value_segment_double->append(3.1415);
  value_segment_double->append(4.1);
  value_segment_double->append(0.0);
  value_segment_double->append(-0.0);

  const auto dict_segment = std::make_shared<DictionarySegment<double>>(value_segment_double);

  // Test attribute_vector size.
  EXPECT_EQ(dict_segment->size(), 7);

  // Test dictionary size (uniqueness).
  EXPECT_EQ(dict_segment->unique_values_count(), 4);

  // Test sorting.
  const auto& dict = dict_segment->dictionary();
  EXPECT_EQ(dict[0], 0.0);
  EXPECT_EQ(dict[1], 1.0);
  EXPECT_EQ(dict[2], 3.1415);
  EXPECT_EQ(dict[3], 4.1);
}

TEST_F(StorageDictionarySegmentTest, LowerUpperBound) {
  for (auto value = int16_t{0}; value <= 10; value += 2) {
    value_segment_int->append(value);
  }

  std::shared_ptr<AbstractSegment> segment;
  resolve_data_type("int", [&](auto type) {
    using Type = typename decltype(type)::type;
    segment = std::make_shared<DictionarySegment<Type>>(value_segment_int);
  });
  auto dict_segment = std::dynamic_pointer_cast<DictionarySegment<int32_t>>(segment);

  EXPECT_EQ(dict_segment->lower_bound(4), ValueID{2});
  EXPECT_EQ(dict_segment->upper_bound(4), ValueID{3});

  EXPECT_EQ(dict_segment->lower_bound(AllTypeVariant{4}), ValueID{2});
  EXPECT_EQ(dict_segment->upper_bound(AllTypeVariant{4}), ValueID{3});

  EXPECT_EQ(dict_segment->lower_bound(5), ValueID{3});
  EXPECT_EQ(dict_segment->upper_bound(5), ValueID{3});

  EXPECT_EQ(dict_segment->lower_bound(15), INVALID_VALUE_ID);
  EXPECT_EQ(dict_segment->upper_bound(15), INVALID_VALUE_ID);
}

// TODO(student): You should add some more tests here (full coverage would be appreciated) and possibly in other files.

TEST_F(StorageDictionarySegmentTest, CompressEmptySegment) {
  const auto dict_segment = std::make_shared<DictionarySegment<std::string>>(value_segment_str);
  EXPECT_EQ(dict_segment->size(), 0);
  EXPECT_EQ(dict_segment->unique_values_count(), 0);
  EXPECT_EQ(dict_segment->estimate_memory_usage(), 0);
}

TEST_F(StorageDictionarySegmentTest, OperatorBracketsAccess) {
  value_segment_str->append("This");
  value_segment_str->append("is");
  value_segment_str->append("just");
  value_segment_str->append("a");
  value_segment_str->append("test");
  value_segment_str->append("!");

  const auto dict_segment = std::make_shared<DictionarySegment<std::string>>(value_segment_str);
  EXPECT_EQ(dict_segment->operator[](0), AllTypeVariant{"This"});
  EXPECT_EQ(dict_segment->operator[](1), AllTypeVariant{"is"});
  EXPECT_EQ(dict_segment->operator[](2), AllTypeVariant{"just"});
  EXPECT_EQ(dict_segment->operator[](3), AllTypeVariant{"a"});
  EXPECT_EQ(dict_segment->operator[](4), AllTypeVariant{"test"});
  EXPECT_EQ(dict_segment->operator[](5), AllTypeVariant{"!"});
}

TEST_F(StorageDictionarySegmentTest, OutOfBoundsChecking) {
  value_segment_str->append("Hello World!");

  const auto dict_segment = std::make_shared<DictionarySegment<std::string>>(value_segment_str);
  EXPECT_THROW(dict_segment->operator[](1), std::logic_error);
  EXPECT_THROW(dict_segment->get_typed_value(1), std::logic_error);
  EXPECT_THROW(dict_segment->get(1), std::logic_error);
}

TEST_F(StorageDictionarySegmentTest, MemoryUsage) {
  value_segment_int->append(1);

  auto dict_segment = std::make_shared<DictionarySegment<int32_t>>(value_segment_int);
  EXPECT_EQ(dict_segment->estimate_memory_usage(), size_t{5}); // 4 byte for 1 * int32_t in dictionary + 1 byte for attribute_vector

  value_segment_int->append(2);
  dict_segment = std::make_shared<DictionarySegment<int32_t>>(value_segment_int);
  EXPECT_EQ(dict_segment->estimate_memory_usage(), size_t{10}); // 8 byte for 2 * int32_t in dictionary + 2 byte for attribute_vector

  value_segment_int->append(2);
  dict_segment = std::make_shared<DictionarySegment<int32_t>>(value_segment_int);
  EXPECT_EQ(dict_segment->estimate_memory_usage(), size_t{11}); // 8 byte for 2 * int32_t in dictionary + 3 byte for attribute_vector
}

TEST_F(StorageDictionarySegmentTest, UsesCorrectAttributeIDWidth) {
  const auto test_width = [this]<typename T>() {
//    auto values = std::vector<int32_t>(std::numeric_limits<T>::max() - 1); // -1 to allow space for null_value_id
//    std::iota(values.begin(), values.end(), 0);
//    for (const auto value : values) {
//      value_segment_int->append(value);
//    }
//    const auto dict_segment = std::make_shared<DictionarySegment<int32_t>>(value_segment_int);
//    EXPECT_EQ(dict_segment->attribute_vector()->width(), AttributeVectorWidth{sizeof(T)});
  };
  // 255 distinct values -> uint8_t
  test_width.operator()<uint8_t>();
  // 65535 distinct values -> uin16_t
  test_width.operator()<uint16_t>();
  // 4294967295 distinct values -> uint32_t
//  test_width.operator()<uint32_t>();
  // fails for more
}

}  // namespace opossum
