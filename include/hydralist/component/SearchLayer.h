#ifndef HYDRALIST_SEARCHLAYER_H
#define HYDRALIST_SEARCHLAYER_H

#include "Tree.h"
#include "common.h"
#include "numa.h"

template <class K>
class ArtRowexIndex
{
 private:
  Key minKey;
  K curMin;
  ART_ROWEX::Tree *idx;
  uint32_t numInserts = 0;
  int numa;

 public:
  ArtRowexIndex()
  {
    idx = new ART_ROWEX::Tree(
        [](TID tid, Key &key) { key.setInt(*reinterpret_cast<uint64_t *>(tid)); });
    minKey.setInt(0);
    curMin = ULLONG_MAX;
  }
  ~ArtRowexIndex() { delete idx; }
  void
  setNuma(int numa)
  {
    this->numa = numa;
  }
  void
  setKey(Key &k, uint64_t key)
  {
    k.setInt(key);
  }
  bool
  insert(K key, void *ptr)
  {
    auto t = idx->getThreadInfo();
    Key k;
    setKey(k, key);
    idx->insert(k, reinterpret_cast<uint64_t>(ptr), t);
    if (key < curMin) curMin = key;
    numInserts++;
    return true;
  }
  bool
  remove(K key, void *ptr)
  {
    auto t = idx->getThreadInfo();
    Key k;
    setKey(k, key);
    idx->remove(k, reinterpret_cast<uint64_t>(ptr), t);
    numInserts--;
    return true;
  }
  // Gets the value of the key if present or the value of key just less than/greater than key
  void *
  lookup(K key)
  {
    if (key <= curMin) return nullptr;
    auto t = idx->getThreadInfo();
    Key endKey;
    setKey(endKey, key);

    auto result = idx->lookupNext(endKey, t);
    return reinterpret_cast<void *>(result);
  }

  // Art segfaults if range operation is done when there are less than 2 keys
  bool
  isEmpty()
  {
    return (numInserts < 2);
  }
  uint32_t
  size()
  {
    return numInserts;
  }
};

template <class K>
using SearchLayer = ArtRowexIndex<K>;

#endif  // HYDRALIST_SEARCHLAYER_H
