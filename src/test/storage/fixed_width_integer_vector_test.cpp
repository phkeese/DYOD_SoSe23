#include <numeric>
#include "base_test.hpp"

#include "storage/abstract_attribute_vector.hpp"
#include "storage/fixed_width_integer_vector.hpp"

namespace opossum {

class FixedWidthIntegerVectorTest : public BaseTest {
 protected:
  std::shared_ptr<ValueSegment<int32_t>> value_segment_int{std::make_shared<ValueSegment<int32_t>>()};

  std::shared_ptr<FixedWidthIntegerVector<uint8_t>> fixed_width_integer_vector{
      std::make_shared<FixedWidthIntegerVector<uint8_t>>(10)};
};

TEST_F(FixedWidthIntegerVectorTest, CorrectWidth) {
  const auto test_width = []<typename T>() {
    // -1 to allow space for null_value_id.
    const auto attribute_vector = std::vector<ValueID>{ValueID{std::numeric_limits<T>::max() - 1}};
    const auto compressed = compress_attribute_vector(attribute_vector);
    EXPECT_EQ(compressed->width(), AttributeVectorWidth{sizeof(T)});
  };  // NOLINT (because of the ;)
  test_width.operator()<uint8_t>();
  test_width.operator()<uint16_t>();
  test_width.operator()<uint32_t>();
}

TEST_F(FixedWidthIntegerVectorTest, FailsForTooLargeInput) {
  const auto attribute_vector = std::vector<ValueID>{ValueID{std::numeric_limits<ValueID>::max()}};
  EXPECT_THROW(compress_attribute_vector(attribute_vector), std::logic_error);
}

TEST_F(FixedWidthIntegerVectorTest, SetValue) {
  EXPECT_EQ(fixed_width_integer_vector->get(0), 0);
  fixed_width_integer_vector->set(0, ValueID{1});
  EXPECT_EQ(fixed_width_integer_vector->get(0), 1);
}

}  // namespace opossum
