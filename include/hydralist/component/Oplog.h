#ifndef _OPLOG_H_
#define _OPLOG_H_
#include <algorithm>
#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <vector>

#include "common.h"
#include "ordo_clock.h"

boost::lockfree::spsc_queue<std::vector<OpStruct *> *, boost::lockfree::capacity<10000>>
    g_workQueue[MAX_NUMA * WORKER_THREAD_PER_NUMA];
std::atomic<int> numSplits;
int combinerSplits = 0;
std::atomic<unsigned long> curQ;

class Oplog
{
 private:
  std::mutex qLock[2];
  std::vector<OpStruct *> oplog1;
  std::vector<OpStruct *> oplog2;
  std::vector<std::vector<OpStruct *>> op_{oplog1, oplog2};
  static inline thread_local Oplog *perThreadLog = nullptr;

 public:
  Oplog(){};

  void
  resetQ(int qnum)
  {
    op_[qnum].clear();
  }

  std::vector<OpStruct *> *
  getQ(int qnum)
  {
    return &op_[qnum];
  }

  static Oplog *
  getPerThreadInstance()
  {
    return perThreadLog;
  }
  static void
  setPerThreadInstance(Oplog *ptr)
  {
    perThreadLog = ptr;
  }

  static Oplog *getOpLog();

  static void
  enqPerThreadLog(OpStruct::Operation op, Key_t key, uint8_t hash, void *listNodePtr)
  {
    Oplog *perThreadLog = getOpLog();
    perThreadLog->enq(op, key, hash, listNodePtr);
  }

  void
  enq(OpStruct::Operation op, Key_t key, uint8_t hash, void *listNodePtr)
  {
    OpStruct *ops = new OpStruct;
    ops->op = op;
    ops->key = key;
    ops->hash = static_cast<uint8_t>(hash % WORKER_THREAD_PER_NUMA);
    ops->listNodePtr = listNodePtr;
    ops->ts = ordo_get_clock();
    unsigned long qnum = curQ % 2;
    while (!qLock[qnum].try_lock()) {
      std::atomic_thread_fence(std::memory_order_acq_rel);
      qnum = curQ % 2;
    }
    op_[qnum].push_back(ops);
    qLock[qnum].unlock();
    // std::atomic_fetch_add(&numSplits, 1);
  }

  void
  lock(int qnum)
  {
    qLock[qnum].lock();
  }
  void
  unlock(int qnum)
  {
    qLock[qnum].unlock();
  }
};

std::set<Oplog *> g_perThreadLog;

Oplog *
Oplog::getOpLog()
{
  Oplog *perThreadLog = Oplog::getPerThreadInstance();
  if (!g_perThreadLog.count(perThreadLog)) {
    perThreadLog = new Oplog;
    g_perThreadLog.insert(perThreadLog);
    setPerThreadInstance(perThreadLog);
  }
  return perThreadLog;
}

#endif
