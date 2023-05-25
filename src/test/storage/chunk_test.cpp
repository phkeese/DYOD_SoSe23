#include "base_test.hpp"

#include "resolve_type.hpp"
#include "storage/abstract_segment.hpp"
#include "storage/chunk.hpp"
#include "storage/dictionary_segment.hpp"

namespace opossum {

class StorageChunkTest : public BaseTest {
 protected:
  void SetUp() override {
    int_value_segment = std::make_shared<ValueSegment<int32_t>>();
    int_value_segment->append(4);
    int_value_segment->append(6);
    int_value_segment->append(3);

    string_value_segment = std::make_shared<ValueSegment<std::string>>();
    string_value_segment->append("Hello,");
    string_value_segment->append("world");
    string_value_segment->append("!");

    float_value_segment = std::make_shared<ValueSegment<float>>();
    float_value_segment->append(4.0f);
    float_value_segment->append(6.0f);
    float_value_segment->append(3.8f);

    double_value_segment = std::make_shared<ValueSegment<double>>();
    double_value_segment->append(14.4);
    double_value_segment->append(0.0);
    double_value_segment->append(12.8);

    int_empty_value_segment = std::make_shared<ValueSegment<int32_t>>(true);
    string_empty_value_segment = std::make_shared<ValueSegment<std::string>>(true);

    long_nullable_value_segment = std::make_shared<ValueSegment<int64_t>>(true);
    long_nullable_value_segment->append(4);
    long_nullable_value_segment->append(NULL_VALUE);
    long_nullable_value_segment->append(3);
  }

  Chunk chunk;
  std::shared_ptr<ValueSegment<int32_t>> int_value_segment{};
  std::shared_ptr<ValueSegment<std::string>> string_value_segment{};
  std::shared_ptr<ValueSegment<float>> float_value_segment{};
  std::shared_ptr<ValueSegment<double>> double_value_segment{};

  std::shared_ptr<ValueSegment<int32_t>> int_empty_value_segment{};
  std::shared_ptr<ValueSegment<std::string>> string_empty_value_segment{};

  std::shared_ptr<ValueSegment<int64_t>> long_nullable_value_segment{};
};

TEST_F(StorageChunkTest, AddSegmentToChunk) {
  EXPECT_EQ(chunk.size(), 0);
  chunk.add_segment(int_value_segment);
  chunk.add_segment(string_value_segment);
  EXPECT_EQ(chunk.size(), 3);
}

TEST_F(StorageChunkTest, AddNullValue) {
  chunk.add_segment(long_nullable_value_segment);
  chunk.add_segment(string_value_segment);
  chunk.append({NULL_VALUE, "two"});
  EXPECT_EQ(chunk.size(), 4);
}

TEST_F(StorageChunkTest, AddValuesToChunk) {
  chunk.add_segment(int_value_segment);
  chunk.add_segment(string_value_segment);
  chunk.append({2, "two"});
  EXPECT_EQ(chunk.size(), 4);

  if constexpr (OPOSSUM_DEBUG) {
    EXPECT_THROW(chunk.append({}), std::logic_error);
    EXPECT_THROW(chunk.append({4, "val", 3}), std::logic_error);
    EXPECT_EQ(chunk.size(), 4);
  }
}

TEST_F(StorageChunkTest, RetrieveSegment) {
  chunk.add_segment(int_value_segment);
  chunk.add_segment(string_value_segment);
  chunk.append({2, "two"});

  auto segment = chunk.get_segment(ColumnID{0});
  EXPECT_EQ(segment->size(), 4);
}

TEST_F(StorageChunkTest, AddValueInEmptySegemnt) {
  chunk.add_segment(int_empty_value_segment);
  chunk.add_segment(string_empty_value_segment);
  chunk.append({2, "two"});
  EXPECT_EQ(chunk.size(), 1);
}

TEST_F(StorageChunkTest, AddNullValueInEmptySegemnt) {
  chunk.add_segment(int_empty_value_segment);
  chunk.add_segment(string_empty_value_segment);
  chunk.append({NULL_VALUE, NULL_VALUE});
  EXPECT_EQ(chunk.size(), 1);
}

TEST_F(StorageChunkTest, AddAllDataTypes) {
  chunk.add_segment(int_value_segment);
  chunk.add_segment(string_value_segment);
  chunk.add_segment(float_value_segment);
  chunk.add_segment(double_value_segment);
  chunk.add_segment(long_nullable_value_segment);
  EXPECT_EQ(chunk.size(), 3);

  chunk.append({4, "4", 4.0f, 4.0, 4});
  EXPECT_EQ(chunk.size(), 4);
}

TEST_F(StorageChunkTest, AddSegmentTwice) {
  chunk.add_segment(int_value_segment);
  EXPECT_THROW(chunk.add_segment(int_value_segment), std::logic_error);
}

TEST_F(StorageChunkTest, AppendToNonValueSegment) {
  const auto dict_segment = std::make_shared<DictionarySegment<int32_t>>(int_value_segment);
  chunk.add_segment(dict_segment);
  EXPECT_THROW(chunk.append({0}), std::logic_error);
}

}  // namespace opossum
