#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <limits>
#include <ostream>

namespace db {

// A typed wrapper around an integer identifier. Each ID kind gets its own Tag,
// so a PageId cannot be passed where a FrameId is expected, and IDs cannot be
// mixed with plain integers by accident. The wrapper is zero-cost: same size
// and layout as the underlying integer.
//
// A default-constructed ID is invalid (max value of Rep), so "no page" or
// "no transaction" never collides with a real ID 0.
template <typename Tag, typename Rep>
class StrongId {
 public:
  using ValueType = Rep;
  static constexpr Rep kInvalidValue = std::numeric_limits<Rep>::max();

  constexpr StrongId() = default;
  constexpr explicit StrongId(Rep value) : value_(value) {}

  static constexpr StrongId Invalid() { return StrongId(); }

  constexpr Rep value() const { return value_; }
  constexpr bool IsValid() const { return value_ != kInvalidValue; }

  constexpr auto operator<=>(const StrongId&) const = default;

  friend std::ostream& operator<<(std::ostream& os, StrongId id) {
    if (!id.IsValid()) return os << "<invalid>";
    return os << id.value_;
  }

 private:
  Rep value_ = kInvalidValue;
};

struct PageIdTag {};
struct FrameIdTag {};
struct TableIdTag {};
struct IndexIdTag {};
struct TxnIdTag {};
struct LsnTag {};

using PageId = StrongId<PageIdTag, uint32_t>;    // page number within the database file
using FrameId = StrongId<FrameIdTag, uint32_t>;  // slot in the buffer pool
using TableId = StrongId<TableIdTag, uint32_t>;
using IndexId = StrongId<IndexIdTag, uint32_t>;
using TxnId = StrongId<TxnIdTag, uint64_t>;
using LSN = StrongId<LsnTag, uint64_t>;  // log sequence number

}  // namespace db

template <typename Tag, typename Rep>
struct std::hash<db::StrongId<Tag, Rep>> {
  size_t operator()(db::StrongId<Tag, Rep> id) const noexcept {
    return std::hash<Rep>{}(id.value());
  }
};
