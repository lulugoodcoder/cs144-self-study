#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  int64_t modeVal = 1ULL << 32;
  uint32_t tmp = (n + (uint64_t)zero_point.raw_value_) % (modeVal);
  return Wrap32(tmp);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t MOD = 1ULL << 32;
  // Step 1: compute raw difference modulo 2^32
  uint32_t w = raw_value_;
  uint32_t z = zero_point.raw_value_;
  int64_t wrapped_diff = (static_cast<int64_t>(w) - static_cast<int64_t>(z) + MOD) % MOD;
  
  uint64_t candidate = wrapped_diff + (checkpoint / MOD) * MOD;
  uint64_t next = candidate + MOD;
  uint64_t prev = (candidate >= MOD) ? candidate - MOD : candidate;

  uint64_t closest = candidate;

  // check previous window
  if ((checkpoint > prev ? checkpoint - prev : prev - checkpoint) <
      (checkpoint > closest ? checkpoint - closest : closest - checkpoint)) {
      closest = prev;
  }

  // check next window
  if ((checkpoint > next ? checkpoint - next : next - checkpoint) <
      (checkpoint > closest ? checkpoint - closest : closest - checkpoint)) {
      closest = next;
  }

  return closest;
}
