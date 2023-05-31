#include "base_test.hpp"

#include "operators/abstract_operator.hpp"
#include "operators/get_table.hpp"
#include "operators/print.hpp"
#include "operators/table_scan.hpp"
#include "storage/reference_segment.hpp"
#include "storage/storage_manager.hpp"
#include "utils/load_table.hpp"

namespace opossum {

class ReferenceSegmentTest : public BaseTest {
  virtual void SetUp() {
    _test_table = std::make_shared<Table>(3);
    _test_table->add_column("a", "int", false);
    _test_table->add_column("b", "float", true);
    _test_table->append({123, 456.7f});
    _test_table->append({1234, 457.7f});
    _test_table->append({12345, 458.7f});
    _test_table->append({54321, 458.7f});
    _test_table->append({12345, 458.7f});

    _test_table_dict = std::make_shared<Table>(5);
    _test_table_dict->add_column("a", "int", false);
    _test_table_dict->add_column("b", "int", true);
    for (auto value = int32_t{0}; value <= 24; value += 2) {
      _test_table_dict->append({value, 100 + value});
    }

    _test_table_dict->compress_chunk(ChunkID{0});
    _test_table_dict->compress_chunk(ChunkID{1});

    StorageManager::get().add_table("test_table_dict", _test_table_dict);
  }

 public:
  std::shared_ptr<Table> _test_table, _test_table_dict;
};

TEST_F(ReferenceSegmentTest, RetrievesValues) {
  // PosList with (0, 0), (0, 1), (0, 2)
  auto pos_list = std::make_shared<PosList>(
      std::initializer_list<RowID>({RowID{ChunkID{0}, 0}, RowID{ChunkID{0}, 1}, RowID{ChunkID{0}, 2}}));
  auto reference_segment = ReferenceSegment(_test_table, ColumnID{0}, pos_list);

  auto& segment = *(_test_table->get_chunk(ChunkID{0})->get_segment(ColumnID{0}));

  EXPECT_EQ(reference_segment[0], segment[0]);
  EXPECT_EQ(reference_segment[1], segment[1]);
  EXPECT_EQ(reference_segment[2], segment[2]);
}

TEST_F(ReferenceSegmentTest, RetrievesValuesOutOfOrder) {
  // PosList with (0, 1), (0, 2), (0, 0)
  auto pos_list = std::make_shared<PosList>(
      std::initializer_list<RowID>({RowID{ChunkID{0}, 1}, RowID{ChunkID{0}, 2}, RowID{ChunkID{0}, 0}}));
  auto reference_segment = ReferenceSegment(_test_table, ColumnID{0}, pos_list);

  auto& segment = *(_test_table->get_chunk(ChunkID{0})->get_segment(ColumnID{0}));

  EXPECT_EQ(reference_segment[0], segment[1]);
  EXPECT_EQ(reference_segment[1], segment[2]);
  EXPECT_EQ(reference_segment[2], segment[0]);
}

TEST_F(ReferenceSegmentTest, RetrievesValuesFromChunks) {
  // PosList with (0, 2), (1, 0), (1, 1)
  auto pos_list = std::make_shared<PosList>(
      std::initializer_list<RowID>({RowID{ChunkID{0}, 2}, RowID{ChunkID{1}, 0}, RowID{ChunkID{1}, 1}}));
  auto reference_segment = ReferenceSegment(_test_table, ColumnID{0}, pos_list);

  auto& segment_1 = *(_test_table->get_chunk(ChunkID{0})->get_segment(ColumnID{0}));
  auto& segment_2 = *(_test_table->get_chunk(ChunkID{1})->get_segment(ColumnID{0}));

  EXPECT_EQ(reference_segment[0], segment_1[2]);
  EXPECT_EQ(reference_segment[2], segment_2[1]);
}

TEST_F(ReferenceSegmentTest, RetrieveNullValueFromNullRowID) {
  // RowIDPosList with (0, 0), (0, 1), NULL_ROW_ID, (0, 2)
  auto pos_list = std::make_shared<PosList>(
      std::initializer_list<RowID>({RowID{ChunkID{0}, ChunkOffset{0}}, RowID{ChunkID{0}, ChunkOffset{1}}, NULL_ROW_ID,
                                    RowID{ChunkID{0}, ChunkOffset{2}}}));

  auto ref_segment = ReferenceSegment(_test_table, ColumnID{0}, pos_list);

  auto& segment = *(_test_table->get_chunk(ChunkID{0})->get_segment(ColumnID{0}));

  EXPECT_EQ(ref_segment[ChunkOffset{0}], segment[ChunkOffset{0}]);
  EXPECT_EQ(ref_segment[ChunkOffset{1}], segment[ChunkOffset{1}]);
  EXPECT_TRUE(variant_is_null(ref_segment[ChunkOffset{2}]));
  EXPECT_EQ(ref_segment[ChunkOffset{3}], segment[ChunkOffset{2}]);
}

#ifndef __OPTIMIZE__
TEST_F(ReferenceSegmentTest, DisallowRecursiveReferences) {
  auto table = load_table("src/test/tables/int_float_filtered2.tbl", 1);

  auto positions = std::make_shared<PosList>();
  positions->emplace_back(RowID{ChunkID{0}, 0});
  auto first_reference = std::make_shared<ReferenceSegment>(table, ColumnID{0}, positions);

  auto second_table = std::make_shared<Table>();
  second_table->add_column_definition("ref", "int", false);
  second_table->get_chunk(ChunkID{0})->add_segment(first_reference);

  auto second_reference = std::make_shared<ReferenceSegment>(second_table, ColumnID{0}, positions);
  ASSERT_THROW(second_reference->operator[](0), std::logic_error);
}
#endif

TEST_F(ReferenceSegmentTest, EstimateMemoryUsage) {
  auto pos_list = std::make_shared<PosList>();
  const auto reference_segment = ReferenceSegment(_test_table, ColumnID{0}, pos_list);
  constexpr auto alignment_bytes = 14;
  EXPECT_EQ(reference_segment.estimate_memory_usage(), sizeof(std::shared_ptr<const Table>) +
                                                           sizeof(std::shared_ptr<const PosList>) + sizeof(ColumnID) +
                                                           alignment_bytes);
  pos_list->push_back(RowID{ChunkID{0}, ChunkOffset{0}});
  // The size should not have changed as no data is actually held by the ReferenceSegment.
  EXPECT_EQ(reference_segment.estimate_memory_usage(), sizeof(std::shared_ptr<const Table>) +
                                                           sizeof(std::shared_ptr<const PosList>) + sizeof(ColumnID) +
                                                           alignment_bytes);
}

}  // namespace opossum
