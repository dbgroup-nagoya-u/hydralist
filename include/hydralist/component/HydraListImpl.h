//
// Created by ajit on 3/8/19.
//

#ifndef HYDRALIST_HYDRALIST_H
#define HYDRALIST_HYDRALIST_H

#include <zconf.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "Combiner.h"
#include "SearchLayer.h"
#include "WorkerThread.h"
#include "common.h"
#include "linkedList.h"
#include "numa-config.h"
#include "numa.h"
#include "ordo_clock.h"
#include "threadData.h"

// Temp class. Will be replaced by ART
// Uses a sorted array for search

#ifdef HYDRALIST_ENABLE_STATS
#define acc_sl_time(x) (curThreadData->sltime += x)
#define acc_dl_time(x) (curThreadData->dltime += x)
#define hydralist_reset_timers() \
  total_sl_time = 0;             \
  total_dl_time = 0;
#define hydralist_start_timer()  \
  {                              \
    unsigned long __b_e_g_i_n__; \
    __b_e_g_i_n__ = read_tsc();
#define hydralist_stop_timer(tick)      \
  (tick) += read_tsc() - __b_e_g_i_n__; \
  }
#else
#define acc_sl_time(x)
#define acc_dl_time(x)
#define hydralist_start_timer()
#define hydralist_stop_timer(tick)
#define hydralist_reset_timers()
#endif

std::set<ThreadData *> g_threadDataSet;
std::atomic<bool> g_globalStop;
std::atomic<bool> g_combinerStop;
thread_local ThreadData *curThreadData = NULL;
volatile bool threadInitialized[MAX_NUMA];
volatile bool slReady[MAX_NUMA];
std::mutex g_threadDataLock;
uint64_t g_removeCount;
volatile std::atomic<bool> g_removeDetected;

template <class K>
extern std::vector<SearchLayer<K> *> g_perNumaSlPtr;
extern std::set<ThreadData *> g_threadDataSet;

template <class K>
using DataLayer = LinkedList<K>;

template <class K>
class HydraListImpl
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  using ListNode_t = ListNode<K>;
  using OpStruct_t = OpStruct<K>;
  using SearchLayer_t = SearchLayer<K>;
  using CombinerThread_t = CombinerThread<K>;

 private:
  SearchLayer_t sl;
  DataLayer<K> dl;
  std::vector<std::thread *> wtArray;  // CurrentOne but there should be numa number of threads
  std::thread *combinerThread;
  static inline thread_local int threadNumaNode = -1;
  void
  createWorkerThread(int numNuma)
  {
    for (int i = 0; i < numNuma * WORKER_THREAD_PER_NUMA; i++) {
      threadInitialized[i % numNuma] = false;
      std::thread *wt = new std::thread(workerThreadExec, i, numNuma);
      wtArray.push_back(wt);
      pinThread(wt, i % numNuma);
      threadInitialized[i % numNuma] = true;
    }
  }

  void
  createCombinerThread()
  {
    combinerThread = new std::thread(combinerThreadExec, totalNumaActive);
  }

  ListNode_t *
  getJumpNode(K &key)
  {
    int numaNode = getThreadNuma();
    SearchLayer_t &sl = *g_perNumaSlPtr<K>[numaNode];
    if (sl.isEmpty()) return dl.getHead();
    auto *jumpNode = reinterpret_cast<ListNode_t *>(sl.lookup(key));
    if (jumpNode == nullptr) jumpNode = dl.getHead();
    return jumpNode;
  }

  static inline int totalNumaActive = 0;
  std::atomic<uint32_t> numThreads;

 public:
  explicit HydraListImpl(int numNuma)
  {
    assert(numNuma <= NUM_SOCKET);
    totalNumaActive = numNuma;
    g_perNumaSlPtr<K>.resize(totalNumaActive);
    dl.initialize();
    g_globalStop = false;
    g_combinerStop = false;
    createWorkerThread(numNuma);
    printf("Worker threads created\n");
    createCombinerThread();
    printf("Combiner thread created\n");
    for (int i = 0; i < totalNumaActive; i++) {
      while (slReady[i] == false)
        ;
    }
    hydralist_reset_timers()
  }

  ~HydraListImpl()
  {
    g_globalStop = true;
    for (auto &t : wtArray) t->join();
    combinerThread->join();

    printf("sl size: %u\n", g_perNumaSlPtr<K>[0] -> size());
    for (int i = 0; i < totalNumaActive; i++) delete g_perNumaSlPtr<K>[i];
    std::cout << "Total splits: " << numSplits << std::endl;
    std::cout << "Combiner splits: " << combinerSplits << std::endl;
#ifdef HYDRALIST_ENABLE_STATS
    std::cout << "total_dl_time: " << total_dl_time / numThreads / 1000 << std::endl;
    std::cout << "total_sl_time: " << total_sl_time / numThreads / 1000 << std::endl;
#endif
  }

  bool
  insert(K &key, Val_t val)
  {
    uint64_t clock = ordo_get_clock();
    curThreadData->read_lock(clock);
    ListNode_t *jumpNode;
    [[maybe_unused]] uint64_t ticks = 0;
    bool ret;
    hydralist_start_timer();
    jumpNode = getJumpNode(key);
    hydralist_stop_timer(ticks);
    acc_sl_time(ticks);

    hydralist_start_timer();
    ret = dl.insert(key, val, jumpNode);
    hydralist_stop_timer(ticks);
    acc_dl_time(ticks);
    curThreadData->read_unlock();
    return ret;
  }

  bool
  update(K &key, Val_t val)
  {
    uint64_t clock = ordo_get_clock();
    curThreadData->read_lock(clock);
    bool ret = dl.update(key, val, getJumpNode(key));
    curThreadData->read_unlock();
    return ret;
  }

  bool
  remove(K &key)
  {
    uint64_t clock = ordo_get_clock();
    curThreadData->read_lock(clock);
    bool ret = dl.remove(key, getJumpNode(key));
    curThreadData->read_unlock();
    return ret;
  }

  void
  registerThread()
  {
    int threadId = numThreads.fetch_add(1);
    auto td = new ThreadData(threadId);
    g_threadDataLock.lock();
    g_threadDataSet.insert(td);
    g_threadDataLock.unlock();
    curThreadData = td;
    std::atomic_thread_fence(std::memory_order_acq_rel);
  }

  void
  unregisterThread()
  {
    if (curThreadData == NULL) return;
    [[maybe_unused]] int threadId = curThreadData->getThreadId();
    curThreadData->setfinish();
#ifdef HYDRALIST_ENABLE_STATS
    total_dl_time.fetch_add(curThreadData->dltime);
    total_sl_time.fetch_add(curThreadData->sltime);
#endif
  }

  Val_t
  lookup(K &key)
  {
    uint64_t clock = ordo_get_clock();
    curThreadData->read_lock(clock);
    Val_t val{};
    [[maybe_unused]] uint64_t ticks;
    ListNode_t *jumpNode;
    jumpNode = getJumpNode(key);
    dl.lookup(key, val, jumpNode);
    curThreadData->read_unlock();
    return val;
  }

  uint64_t
  scan(K &startKey, int range, std::vector<Val_t> &result)
  {
    return dl.scan(startKey, range, result, getJumpNode(startKey));
  }

  static SearchLayer_t *
  createSearchLayer()
  {
    return new SearchLayer_t;
  }

  static int
  getThreadNuma()
  {
    int chip;
    int core;
    if (threadNumaNode == -1) {
      tacc_rdtscp(&chip, &core);
      threadNumaNode = chip;
      if (threadNumaNode > 8) assert(0);
    }
    if (totalNumaActive <= threadNumaNode)
      return 0;
    else
      return threadNumaNode;
  }

  static void
  workerThreadExec(int threadId, int activeNuma)
  {
    WorkerThread<K> wt(threadId, activeNuma);
    g_WorkerThreadInst<K>[threadId] = &wt;
    if (threadId < activeNuma) {
      while (threadInitialized[threadId] == 0)
        ;
      slReady[threadId] = false;
      assert(numa_node_of_cpu(sched_getcpu()) == threadId);
      g_perNumaSlPtr<K>[threadId] = createSearchLayer();
      g_perNumaSlPtr<K>[threadId] -> setNuma(threadId);
      slReady[threadId] = true;
      printf("Search layer %d\n", threadId);
    }
    int count = 0;
    uint64_t lastRemoveCount = 0;
    while (!g_combinerStop) {
      usleep(200);
      while (!wt.isWorkQueueEmpty()) {
        count++;
        wt.applyOperation();
      }
      if (threadId == 0 && lastRemoveCount != g_removeCount) {
        wt.freeListNodes(g_removeCount);
      }
    }
    while (!wt.isWorkQueueEmpty()) {
      count++;
      if (wt.applyOperation() && !g_removeDetected) {
        g_removeDetected.store(true);
      }
    }
    // If worker threads are stopping that means there are no more
    // user threads
    if (threadId == 0) {
      wt.freeListNodes(ULLONG_MAX);
    }
    g_WorkerThreadInst<K>[threadId] = NULL;

    printf("Worker thread: %d Ops: %lu\n", threadId, wt.opcount);
  }

  static uint64_t
  gracePeriodInit(std::vector<ThreadData *> &threadsToWait)
  {
    uint64_t curTime = ordo_get_clock();
    for (auto td : g_threadDataSet) {
      if (td->getFinish()) {
        g_threadDataLock.lock();
        g_threadDataSet.erase(td);
        g_threadDataLock.unlock();
        free(td);
        continue;
      }
      if (td->getRunCnt() % 2) {
        if (ordo_gt_clock(td->getLocalClock(), curTime))
          continue;
        else
          threadsToWait.push_back(td);
      }
    }
    return curTime;
  }

  static void
  waitForThreads(std::vector<ThreadData *> &threadsToWait, uint64_t gpStartTime)
  {
    for (size_t i = 0; i < threadsToWait.size(); i++) {
      if (threadsToWait[i] == NULL) continue;
      ThreadData *td = threadsToWait[i];
      if (td->getRunCnt() % 2 == 0) continue;
      if (ordo_gt_clock(td->getLocalClock(), gpStartTime)) continue;
      while (td->getRunCnt() % 2) {
        usleep(1);
        std::atomic_thread_fence(std::memory_order::memory_order_acq_rel);
      }
    }
  }

  static void
  broadcastDoneCount(uint64_t removeCount)
  {
    g_removeCount = removeCount;
  }

  static void
  combinerThreadExec(int activeNuma)
  {
    CombinerThread_t ct;
    int count = 0;
    while (!g_globalStop) {
      std::vector<OpStruct<K> *> *mergedLog = ct.combineLogs();
      if (mergedLog != nullptr) {
        count++;
        ct.broadcastMergedLog(mergedLog, activeNuma);
      }
      uint64_t doneCountWt = ct.freeMergedLogs(activeNuma, false);
      std::vector<ThreadData *> threadsToWait;
      if (g_removeDetected && doneCountWt != 0) {
        uint64_t gpStartTime = gracePeriodInit(threadsToWait);
        waitForThreads(threadsToWait, gpStartTime);
        broadcastDoneCount(doneCountWt);
        g_removeDetected.store(false);
      } else
        usleep(100);
    }
    // TODO fix this
    int i = 20;
    while (i--) {
      usleep(200);
      std::vector<OpStruct<K> *> *mergedLog = ct.combineLogs();
      if (mergedLog != nullptr) {
        count++;
        ct.broadcastMergedLog(mergedLog, activeNuma);
      }
    }
    while (!ct.mergedLogsToBeFreed()) ct.freeMergedLogs(activeNuma, true);
    g_combinerStop = true;
    printf("Combiner thread: Ops: %d\n", count);
  }

  void
  pinThread(std::thread *t, int numaId)
  {
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    for (int i = 0; i < NUM_SOCKET; i++) {
      for (int j = 0; j < SMT_LEVEL; j++) {
        CPU_SET(OS_CPU_ID[numaId][i][j], &cpuSet);
      }
    }
    int rc = pthread_setaffinity_np(t->native_handle(), sizeof(cpu_set_t), &cpuSet);
    assert(rc == 0);
  }

  static unsigned long
  tacc_rdtscp(int *chip, int *core)
  {
    unsigned long a, d, c;
    __asm__ volatile("rdtscp" : "=a"(a), "=d"(d), "=c"(c));
    *chip = (c & 0xFFF000) >> 12;
    *core = c & 0xFFF;
    return ((unsigned long)a) | (((unsigned long)d) << 32);
    ;
  }

#ifdef HYDRALIST_ENABLE_STATS
  std::atomic<uint64_t> total_sl_time;
  std::atomic<uint64_t> total_dl_time;
#endif
};

#endif  // HYDRALIST_HYDRALIST_H
