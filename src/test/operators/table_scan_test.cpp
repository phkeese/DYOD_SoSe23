#include "base_test.hpp"

#include "operators/print.hpp"
#include "operators/table_scan.hpp"
#include "operators/table_wrapper.hpp"
#include "storage/reference_segment.hpp"
#include "utils/load_table.hpp"

namespace opossum {

class OperatorsTableScanTest : public BaseTest {
 protected:
  void SetUp() override {
    _table_wrapper = std::make_shared<TableWrapper>(load_table("src/test/tables/int_float.tbl", 2));
    _table_wrapper->execute();

    std::shared_ptr<Table> test_even_dict = std::make_shared<Table>(5);
    test_even_dict->add_column("a", "int", false);
    test_even_dict->add_column("b", "int", true);
    for (auto index = int32_t{0}; index <= 24; index += 2) {
      test_even_dict->append({index, 100 + index});
    }
    test_even_dict->append({25, NULL_VALUE});

    test_even_dict->compress_chunk(ChunkID{0});
    test_even_dict->compress_chunk(ChunkID{1});

    _table_wrapper_even_dict = std::make_shared<TableWrapper>(std::move(test_even_dict));
    _table_wrapper_even_dict->execute();
  }

  std::shared_ptr<TableWrapper> get_table_op_part_dict() {
    auto table = std::make_shared<Table>(5);
    table->add_column("a", "int", false);
    table->add_column("b", "float", true);

    for (auto index = int32_t{1}; index < 20; ++index) {
      table->append({index, 100.1 + index});
    }

    table->compress_chunk(ChunkID{0});
    table->compress_chunk(ChunkID{1});

    auto table_wrapper = std::make_shared<TableWrapper>(table);
    table_wrapper->execute();

    return table_wrapper;
  }

  std::shared_ptr<TableWrapper> get_table_op_with_n_dict_entries(const int32_t num_entries) {
    // Set up dictionary encoded table with a dictionary consisting of num_entries entries.
    auto table = std::make_shared<opossum::Table>(0);
    table->add_column("a", "int", false);
    table->add_column("b", "float", true);

    for (auto index = int32_t{0}; index <= num_entries; index++) {
      table->append({index, 100.0f + index});
    }

    table->compress_chunk(ChunkID{0});

    auto table_wrapper = std::make_shared<opossum::TableWrapper>(std::move(table));
    table_wrapper->execute();
    return table_wrapper;
  }

  void ASSERT_COLUMN_EQ(std::shared_ptr<const Table> table, const ColumnID column_id,
                        std::vector<AllTypeVariant> expected) {
    for (auto chunk_id = ChunkID{0}; chunk_id < table->chunk_count(); ++chunk_id) {
      const auto& chunk = table->get_chunk(chunk_id);

      for (auto chunk_offset = ChunkOffset{0}; chunk_offset < chunk->size(); ++chunk_offset) {
        const auto segment = chunk->get_segment(column_id);

        const auto found_value = (*segment)[chunk_offset];
        const auto comparator = [found_value](const AllTypeVariant expected_value) {
          // Returns equivalency, not equality.
          return !(found_value < expected_value) && !(expected_value < found_value);
        };

        auto search = std::find_if(expected.begin(), expected.end(), comparator);

        ASSERT_TRUE(search != expected.end());
        expected.erase(search);
      }
    }

    ASSERT_TRUE(expected.empty());
  }

  std::shared_ptr<TableWrapper> _table_wrapper, _table_wrapper_even_dict;
};

TEST_F(OperatorsTableScanTest, DoubleScan) {
  auto expected_result = load_table("src/test/tables/int_float_filtered.tbl", 2);

  auto scan_1 = std::make_shared<TableScan>(_table_wrapper, ColumnID{0}, ScanType::OpGreaterThanEquals, 1234);
  scan_1->execute();

  auto scan_2 = std::make_shared<TableScan>(scan_1, ColumnID{1}, ScanType::OpLessThan, 457.9);
  scan_2->execute();

  EXPECT_TABLE_EQ(scan_2->get_output(), expected_result);
}

TEST_F(OperatorsTableScanTest, EmptyResultScan) {
  auto scan_1 = std::make_shared<TableScan>(_table_wrapper, ColumnID{0}, ScanType::OpGreaterThan, 90000);
  scan_1->execute();

  for (auto chunk_index = ChunkID{0}; chunk_index < scan_1->get_output()->chunk_count(); chunk_index++)
    EXPECT_EQ(scan_1->get_output()->get_chunk(chunk_index)->column_count(), 2);
}

TEST_F(OperatorsTableScanTest, SingleScanReturnsCorrectRowCount) {
  auto expected_result = load_table("src/test/tables/int_float_filtered2.tbl", 1);

  auto scan = std::make_shared<TableScan>(_table_wrapper, ColumnID{0}, ScanType::OpGreaterThanEquals, 1234);
  scan->execute();

  EXPECT_TABLE_EQ(scan->get_output(), expected_result);
}

TEST_F(OperatorsTableScanTest, ScanOnDictColumn) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {104};
  tests[ScanType::OpNotEquals] = {100, 102, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};
  tests[ScanType::OpLessThan] = {100, 102};
  tests[ScanType::OpLessThanEquals] = {100, 102, 104};
  tests[ScanType::OpGreaterThan] = {106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};
  tests[ScanType::OpGreaterThanEquals] = {104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(_table_wrapper_even_dict, ColumnID{0}, test.first, 4);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{1}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanOnReferencedDictColumn) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second
  // chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {104};
  tests[ScanType::OpNotEquals] = {100, 102, 106};
  tests[ScanType::OpLessThan] = {100, 102};
  tests[ScanType::OpLessThanEquals] = {100, 102, 104};
  tests[ScanType::OpGreaterThan] = {106};
  tests[ScanType::OpGreaterThanEquals] = {104, 106};

  for (const auto& test : tests) {
    auto scan1 = std::make_shared<TableScan>(_table_wrapper_even_dict, ColumnID{1}, ScanType::OpLessThan, 108);
    scan1->execute();

    auto scan2 = std::make_shared<TableScan>(scan1, ColumnID{0}, test.first, 4);
    scan2->execute();

    ASSERT_COLUMN_EQ(scan2->get_output(), ColumnID{1}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanPartiallyCompressed) {
  auto expected_result = load_table("src/test/tables/int_float_seq_filtered.tbl", 2);

  auto table_wrapper = get_table_op_part_dict();
  auto scan_1 = std::make_shared<TableScan>(table_wrapper, ColumnID{0}, ScanType::OpLessThan, 10);
  scan_1->execute();

  EXPECT_TABLE_EQ(scan_1->get_output(), expected_result);
}

TEST_F(OperatorsTableScanTest, ScanOnDictColumnValueGreaterThanMaxDictionaryValue) {
  const auto all_rows =
      std::vector<AllTypeVariant>{100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};
  const auto no_rows = std::vector<AllTypeVariant>{};

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = no_rows;
  tests[ScanType::OpNotEquals] = all_rows;
  tests[ScanType::OpLessThan] = all_rows;
  tests[ScanType::OpLessThanEquals] = all_rows;
  tests[ScanType::OpGreaterThan] = no_rows;
  tests[ScanType::OpGreaterThanEquals] = no_rows;

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(_table_wrapper_even_dict, ColumnID{0}, test.first, 30);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{1}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanOnDictColumnValueLessThanMinDictionaryValue) {
  const auto all_rows =
      std::vector<AllTypeVariant>{100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};
  const auto no_rows = std::vector<AllTypeVariant>{};

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = no_rows;
  tests[ScanType::OpNotEquals] = all_rows;
  tests[ScanType::OpLessThan] = no_rows;
  tests[ScanType::OpLessThanEquals] = no_rows;
  tests[ScanType::OpGreaterThan] = all_rows;
  tests[ScanType::OpGreaterThanEquals] = all_rows;

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(_table_wrapper_even_dict, ColumnID{0} /* "a" */, test.first, -10);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{1}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanOnDictColumnAroundBounds) {
  // Scanning for a value that is around the dictionary's bounds.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {100};
  tests[ScanType::OpLessThan] = {};
  tests[ScanType::OpLessThanEquals] = {100};
  tests[ScanType::OpGreaterThan] = {102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};
  tests[ScanType::OpGreaterThanEquals] = {100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};
  tests[ScanType::OpNotEquals] = {102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, NULL_VALUE};

  for (const auto& test : tests) {
    auto scan = std::make_shared<opossum::TableScan>(_table_wrapper_even_dict, ColumnID{0}, test.first, 0);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{1}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanWithEmptyInput) {
  auto scan_1 = std::make_shared<opossum::TableScan>(_table_wrapper, ColumnID{0}, ScanType::OpGreaterThan, 12345);
  scan_1->execute();
  EXPECT_EQ(scan_1->get_output()->row_count(), static_cast<size_t>(0));

  // Scan_1 produced an empty result.
  auto scan_2 = std::make_shared<opossum::TableScan>(scan_1, ColumnID{1}, ScanType::OpEquals, 456.7);
  scan_2->execute();

  EXPECT_EQ(scan_2->get_output()->row_count(), static_cast<size_t>(0));
}

TEST_F(OperatorsTableScanTest, ScanOnWideDictionarySegment) {
  // 2**8 + 1 values require a data type of 16bit.
  const auto table_wrapper_dict_16 = get_table_op_with_n_dict_entries((1 << 8) + 1);
  auto scan_1 = std::make_shared<opossum::TableScan>(table_wrapper_dict_16, ColumnID{0}, ScanType::OpGreaterThan, 200);
  scan_1->execute();

  EXPECT_EQ(scan_1->get_output()->row_count(), static_cast<size_t>(57));

  // 2**16 + 1 values require a data type of 32bit.
  const auto table_wrapper_dict_32 = get_table_op_with_n_dict_entries((1 << 16) + 1);
  auto scan_2 =
      std::make_shared<opossum::TableScan>(table_wrapper_dict_32, ColumnID{0}, ScanType::OpGreaterThan, 65500);
  scan_2->execute();

  EXPECT_EQ(scan_2->get_output()->row_count(), static_cast<size_t>(37));
}

TEST_F(OperatorsTableScanTest, ScanOnReferenceSegmentWithNullValue) {
  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {104};
  tests[ScanType::OpNotEquals] = {100, 102, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124};
  tests[ScanType::OpLessThan] = {100, 102};
  tests[ScanType::OpLessThanEquals] = {100, 102, 104};
  tests[ScanType::OpGreaterThan] = {106, 108, 110, 112, 114, 116, 118, 120, 122, 124};
  tests[ScanType::OpGreaterThanEquals] = {104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124};

  for (const auto& test : tests) {
    auto scan_1 =
        std::make_shared<TableScan>(_table_wrapper_even_dict, ColumnID{0} /* "a" */, ScanType::OpGreaterThan, -10);
    scan_1->execute();

    auto scan_2 = std::make_shared<TableScan>(scan_1, ColumnID{1}, test.first, 104);
    scan_2->execute();

    ASSERT_COLUMN_EQ(scan_2->get_output(), ColumnID{1}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, HandlesInvalidRowID) {
  auto table = load_table("src/test/tables/int_float_filtered2.tbl", 1);

  auto positions = std::make_shared<PosList>();
  positions->emplace_back(RowID{ChunkID{0}, INVALID_CHUNK_OFFSET});
  auto reference_segment = std::make_shared<ReferenceSegment>(table, ColumnID{0}, positions);
  auto reference_table = std::make_shared<Table>();
  reference_table->add_column_definition("int", "int", true);
  reference_table->get_chunk(ChunkID{0})->add_segment(reference_segment);

  auto table_wrapper = std::make_shared<TableWrapper>(std::move(reference_table));
  table_wrapper->execute();

  auto scan = std::make_shared<TableScan>(table_wrapper, ColumnID{0} /* "a" */, ScanType::OpGreaterThan, -10);
  scan->execute();
  EXPECT_EQ(scan->get_output()->row_count(), 0);
}

TEST_F(OperatorsTableScanTest, EmptyResultScanReturnsEmptyChunk) {
  auto scan_1 = std::make_shared<TableScan>(_table_wrapper, ColumnID{0}, ScanType::OpGreaterThan, 90000);
  scan_1->execute();

  EXPECT_EQ(scan_1->get_output()->row_count(), 0);
}

TEST_F(OperatorsTableScanTest, ScanOnValueSegmentOnColumnWithOnlyNull) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {};
  tests[ScanType::OpNotEquals] = {};
  tests[ScanType::OpLessThan] = {};
  tests[ScanType::OpLessThanEquals] = {};
  tests[ScanType::OpGreaterThan] = {};
  tests[ScanType::OpGreaterThanEquals] = {};

  auto table_with_nulls = std::make_shared<Table>();
  table_with_nulls->add_column("Null", "int", true);
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({NULL_VALUE});

  const auto table_wrapper_value_segment = std::make_shared<TableWrapper>(std::move(table_with_nulls));
  table_wrapper_value_segment->execute();

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(table_wrapper_value_segment, ColumnID{0}, test.first, 4);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{0}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanOnDictSegmentOnColumnWithOnlyNull) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {};
  tests[ScanType::OpNotEquals] = {};
  tests[ScanType::OpLessThan] = {};
  tests[ScanType::OpLessThanEquals] = {};
  tests[ScanType::OpGreaterThan] = {};
  tests[ScanType::OpGreaterThanEquals] = {};

  auto table_with_nulls = std::make_shared<Table>();
  table_with_nulls->add_column("Null", "int", true);
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({NULL_VALUE});

  table_with_nulls->compress_chunk(ChunkID{0});

  const auto table_wrapper_value_segment = std::make_shared<TableWrapper>(std::move(table_with_nulls));
  table_wrapper_value_segment->execute();

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(table_wrapper_value_segment, ColumnID{0}, test.first, 4);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{0}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanForNullOnValueSegment) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {};
  tests[ScanType::OpNotEquals] = {2, 3};
  tests[ScanType::OpLessThan] = {};
  tests[ScanType::OpLessThanEquals] = {};
  tests[ScanType::OpGreaterThan] = {};
  tests[ScanType::OpGreaterThanEquals] = {};

  auto table_with_nulls = std::make_shared<Table>();
  table_with_nulls->add_column("Null", "int", true);
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({2});
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({3});

  const auto table_wrapper_value_segment = std::make_shared<TableWrapper>(std::move(table_with_nulls));
  table_wrapper_value_segment->execute();

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(table_wrapper_value_segment, ColumnID{0}, test.first, NULL_VALUE);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{0}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanForNullOnDictSegment) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {};
  tests[ScanType::OpNotEquals] = {2, 3};
  tests[ScanType::OpLessThan] = {};
  tests[ScanType::OpLessThanEquals] = {};
  tests[ScanType::OpGreaterThan] = {};
  tests[ScanType::OpGreaterThanEquals] = {};

  auto table_with_nulls = std::make_shared<Table>();
  table_with_nulls->add_column("Null", "int", true);
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({2});
  table_with_nulls->append({NULL_VALUE});
  table_with_nulls->append({3});

  table_with_nulls->compress_chunk(ChunkID{0});

  const auto table_wrapper_value_segment = std::make_shared<TableWrapper>(std::move(table_with_nulls));
  table_wrapper_value_segment->execute();

  for (const auto& test : tests) {
    auto scan = std::make_shared<TableScan>(table_wrapper_value_segment, ColumnID{0}, test.first, NULL_VALUE);
    scan->execute();

    ASSERT_COLUMN_EQ(scan->get_output(), ColumnID{0}, test.second);
  }
}

TEST_F(OperatorsTableScanTest, ScanForNullOnReferenceSegment) {
  // We do not need to check for a non existing value, because that happens automatically when we scan the second chunk.

  auto tests = std::map<ScanType, std::vector<AllTypeVariant>>{};
  tests[ScanType::OpEquals] = {};
  tests[ScanType::OpNotEquals] = {2, 4};
  tests[ScanType::OpLessThan] = {};
  tests[ScanType::OpLessThanEquals] = {};
  tests[ScanType::OpGreaterThan] = {};
  tests[ScanType::OpGreaterThanEquals] = {};

  auto table_with_nulls = std::make_shared<Table>();

  table_with_nulls->add_column("Not_null", "int", true);
  table_with_nulls->add_column("Null", "int", true);
  table_with_nulls->append({1, NULL_VALUE});
  table_with_nulls->append({2, 2});
  table_with_nulls->append({3, NULL_VALUE});
  table_with_nulls->append({4, 3});

  const auto table_wrapper = std::make_shared<TableWrapper>(std::move(table_with_nulls));
  table_wrapper->execute();

  auto scan_1 = std::make_shared<TableScan>(table_wrapper, ColumnID{0}, ScanType::OpGreaterThanEquals, 1);
  scan_1->execute();

  for (const auto& test : tests) {
    auto scan_2 = std::make_shared<TableScan>(scan_1, ColumnID{1}, test.first, NULL_VALUE);
    scan_2->execute();

    ASSERT_COLUMN_EQ(scan_2->get_output(), ColumnID{0}, test.second);
  }
}

}  // namespace opossum
