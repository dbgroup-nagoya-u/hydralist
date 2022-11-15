//
// Created by ajit on 6/10/19.
//

#ifndef HYDRALIST_BITSET_H
#define HYDRALIST_BITSET_H

#include <assert.h>

#include <cstdint>

namespace hydra
{
class bitset
{
 private:
  uint64_t bits[2];
  bool
  testBit(int pos, uint64_t &bits)
  {
    return (bits & (1UL << (pos))) != 0;
  }
  void
  setBit(int pos, uint64_t &bits)
  {
    bits |= 1UL << pos;
  }
  void
  resetBit(int pos, uint64_t &bits)
  {
    bits &= ~(1UL << pos);
  }

 public:
  void
  clear()
  {
    bits[0] = 0;
    bits[1] = 0;
  }
  void
  set(int index)
  {
    setBit(index, bits[index / 64]);
  }
  void
  reset(int index)
  {
    resetBit(index, bits[index / 64]);
  }
  bool
  test(int index)
  {
    return testBit(index, bits[index / 64]);
  }
  bool
  operator[](int index)
  {
    return test(index);
  }
  uint64_t
  to_ulong()
  {
    return bits[0];
  }
  uint64_t
  to_ulong(int index)
  {
    return bits[index];
  }
};
};  // namespace hydra

#endif  // HYDRALIST_BITSET_H
