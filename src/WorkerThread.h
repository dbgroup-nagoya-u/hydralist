#ifndef HYDRALIST_WORKERTHREAD_H
#define HYDRALIST_WORKERTHREAD_H
#include <assert.h>

#include <queue>

#include "Combiner.h"
#include "HydraList.h"
#include "Oplog.h"
#include "common.h"

class WorkerThread
{
 private:
  boost::lockfree::spsc_queue<std::vector<OpStruct *> *, boost::lockfree::capacity<10000>>
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
    std::vector<OpStruct *> *oplog = workQueue->front();
    int numaNode = workerThreadId % activeNuma;
    SearchLayer *sl = g_perNumaSlPtr[numaNode];
    uint8_t hash = static_cast<uint8_t>(workerThreadId / activeNuma);
    bool ret = false;
    for (auto opsPtr : *oplog) {
      OpStruct &ops = *opsPtr;
      if (ops.hash != hash) continue;
      opcount++;
      if (ops.op == OpStruct::insert)
        sl->insert(ops.key, ops.listNodePtr);
      else if (ops.op == OpStruct::remove) {
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

extern std::vector<WorkerThread *> g_WorkerThreadInst;

#endif  // HYDRALIST_WORKERTHREAD_H
