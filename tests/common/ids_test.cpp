#include "db/common/ids.h"

#include <gtest/gtest.h>

#include <sstream>
#include <type_traits>
#include <unordered_set>

namespace db {
namespace {

TEST(StrongIdTest, DefaultIsInvalid) {
  PageId p;
  EXPECT_FALSE(p.IsValid());
  EXPECT_EQ(p, PageId::Invalid());
  EXPECT_TRUE(PageId(0).IsValid());
}

TEST(StrongIdTest, ComparesByValue) {
  EXPECT_EQ(TxnId(5), TxnId(5));
  EXPECT_NE(TxnId(5), TxnId(6));
  EXPECT_LT(LSN(1), LSN(2));
  EXPECT_GT(PageId(9), PageId(3));
}

TEST(StrongIdTest, IsZeroCost) {
  static_assert(sizeof(PageId) == sizeof(uint32_t));
  static_assert(sizeof(LSN) == sizeof(uint64_t));
  static_assert(std::is_trivially_copyable_v<PageId>);
}

TEST(StrongIdTest, DistinctKindsDoNotConvert) {
  // The whole point of the wrapper: these must not compile.
  static_assert(!std::is_convertible_v<PageId, FrameId>);
  static_assert(!std::is_convertible_v<uint32_t, PageId>);
  static_assert(!std::is_convertible_v<PageId, uint32_t>);
}

TEST(StrongIdTest, Hashable) {
  std::unordered_set<PageId> set{PageId(1), PageId(2), PageId(1)};
  EXPECT_EQ(set.size(), 2u);
}

TEST(StrongIdTest, Prints) {
  std::ostringstream os;
  os << PageId(42) << " " << PageId::Invalid();
  EXPECT_EQ(os.str(), "42 <invalid>");
}

}  // namespace
}  // namespace db
