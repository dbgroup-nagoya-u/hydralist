//
// Created by ajit on 3/25/19.
//

#ifndef HYDRALIST_HYDRALISTAPI_H
#define HYDRALIST_HYDRALISTAPI_H
#include "component/HydraListImpl.h"
#include "component/common.h"

template <class K>
class HydraList
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  using HydraListImpl_t = HydraListImpl<K>;

 private:
  HydraListImpl_t *hl;

 public:
  HydraList(int numa) { hl = new HydraListImpl_t(numa); }
  ~HydraList() { delete hl; }
  bool
  insert(K key, Val_t val)
  {
    return hl->insert(key, val);
  }
  bool
  update(K key, Val_t val)
  {
    return hl->update(key, val);
  }
  Val_t
  lookup(K key)
  {
    return hl->lookup(key);
  }
  Val_t
  remove(K key)
  {
    return hl->remove(key);
  }
  uint64_t
  scan(K startKey, int range, std::vector<Val_t> &result)
  {
    return hl->scan(startKey, range, result);
  }
  void
  registerThread()
  {
    hl->registerThread();
  }
  void
  unregisterThread()
  {
    hl->unregisterThread();
  }
};

#endif  // HYDRALIST_HYDRALISTAPI_H
