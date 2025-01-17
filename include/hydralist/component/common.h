//
// Created by ajit on 3/7/19.
//

#ifndef HYDRALIST_COMMON_H
#define HYDRALIST_COMMON_H
#include <cstdint>
typedef uint64_t Val_t;
#define MAX_NUMA 8
#define WORKER_THREAD_PER_NUMA 1

template <class K>
class OpStruct
{
 public:
  enum Operation { insert, remove };
  Operation op;
  K key;
  uint8_t hash;
  void *listNodePtr;
  uint64_t ts;
  bool
  operator<(const OpStruct &ops) const
  {
    return (ts < ops.ts);
  }
};

#endif  // HYDRALIST_COMMON_H
