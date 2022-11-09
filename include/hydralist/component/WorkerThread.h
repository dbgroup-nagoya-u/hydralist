#ifndef HYDRALIST_WORKERTHREAD_H
#define HYDRALIST_WORKERTHREAD_H
#include <assert.h>

#include <queue>

#include "Combiner.h"
#include "Oplog.h"
#include "SearchLayer.h"
#include "common.h"
#include "hydralist/HydraList.h"

template <class K>
std::vector<SearchLayer<K> *> g_perNumaSlPtr(MAX_NUMA);

template <class K>
class WorkerThread
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  using OpStruct_t = OpStruct<K>;

 private:
  boost::lockfree::spsc_queue<std::vector<OpStruct_t *> *, boost::lockfree::capacity<10000>>
      *workQueue;
  int workerThreadId;
  int activeNuma;
  unsigned long logDoneCount;
  std::queue<std::pair<uint64_t, void *>> *freeQueue;

 public:
  unsigned long opcount;

  WorkerThread(int id, int activeNuma)
  {
    this->workerThreadId = id;
    this->activeNuma = activeNuma;
    this->workQueue = &g_workQueue[workerThreadId];
    this->logDoneCount = 0;
    this->opcount = 0;
    if (id == 0) freeQueue = new std::queue<std::pair<uint64_t, void *>>;
  }

  bool
  applyOperation()
  {
    std::vector<OpStruct_t *> *oplog = workQueue->front();
    int numaNode = workerThreadId % activeNuma;
    SearchLayer<K> *sl = g_perNumaSlPtr<K>[numaNode];
    uint8_t hash = static_cast<uint8_t>(workerThreadId / activeNuma);
    bool ret = false;
    for (auto opsPtr : *oplog) {
      OpStruct_t &ops = *opsPtr;
      if (ops.hash != hash) continue;
      opcount++;
      if (ops.op == OpStruct_t::insert)
        sl->insert(ops.key, ops.listNodePtr);
      else if (ops.op == OpStruct_t::remove) {
        sl->remove(ops.key, ops.listNodePtr);
        if (workerThreadId == 0) {
          std::pair<uint64_t, void *> removePair;
          removePair.first = logDoneCount;
          removePair.second = ops.listNodePtr;
          freeQueue->push(removePair);
          ret = true;
        }
      } else
        assert(0);
    }
    workQueue->pop();
    logDoneCount++;
    return ret;
  }

  bool
  isWorkQueueEmpty()
  {
    return !workQueue->read_available();
  }

  unsigned long
  getLogDoneCount()
  {
    return logDoneCount;
  }

  void
  freeListNodes(uint64_t removeCount)
  {
    assert(workerThreadId == 0 && freeQueue != NULL);
    if (freeQueue->empty()) return;
    while (!freeQueue->empty()) {
      std::pair<uint64_t, void *> removePair = freeQueue->front();
      if (removePair.first < removeCount) {
        free(removePair.second);
        freeQueue->pop();
      } else
        break;
    }
  }
};

template <class K>
extern std::vector<WorkerThread<K> *> g_WorkerThreadInst;

#endif  // HYDRALIST_WORKERTHREAD_H
