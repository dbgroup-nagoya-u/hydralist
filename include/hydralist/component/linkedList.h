#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H
#include <cassert>
#include <chrono>
#include <climits>
#include <iostream>
#include <thread>

#include "listNode.h"
#define cpu_relax() asm volatile("pause\n" : : : "memory")

enum Operation { lt, gt };

template <class K>
class LinkedList
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  using ListNode_t = ListNode<K>;

 private:
  ListNode_t *head;

 public:
  ListNode_t *
  initialize()
  {
    ListNode_t *head = new (ListNode_t);
    ListNode_t *tail = new (ListNode_t);

    head->insert(0, 0);
    head->setNext(tail);
    head->setPrev(nullptr);
    head->setMin(0);
    head->setMax(ULLONG_MAX);

    tail->insert(ULLONG_MAX, 0);
    tail->setNumEntries(MAX_ENTRIES);  // This prevents merge of tail
    tail->setNext(nullptr);
    tail->setPrev(tail);
    tail->setMin(ULLONG_MAX);
    this->head = head;

    return head;
  }

  bool
  insert(K key, Val_t value, ListNode_t *head)
  {
    int retryCount = 0;
  restart:
    ListNode_t *cur = head;

    while (1) {
      if (cur->getMin() > key) {
        cur = cur->getPrev();
        continue;
      }
      if (!cur->checkRange(key)) {
        cur = cur->getNext();
        continue;
      }
      break;
    }

    if (!cur->writeLock()) {
      if (retryCount > 20) {
        for (int i = 0; i < retryCount * 5; i++) {
          cpu_relax();
          std::atomic_thread_fence(std::memory_order_seq_cst);
        }
      }
      retryCount++;
      goto restart;
    }
    if (cur->getDeleted()) {
      cur->writeUnlock();
      goto restart;
    }
    if (!cur->checkRange(key)) {
      cur->writeUnlock();
      goto restart;
    }
    bool ret = cur->insert(key, value);
    cur->writeUnlock();
    return ret;
  }

  bool
  update(K key, Val_t value, ListNode_t *head)
  {
    int retryCount = 0;
  restart:
    ListNode_t *cur = head;

    while (1) {
      if (cur->getMin() > key) {
        cur = cur->getPrev();
        continue;
      }
      if (!cur->checkRange(key)) {
        cur = cur->getNext();
        continue;
      }
      break;
    }

    if (!cur->writeLock()) {
      if (retryCount > 20) {
        for (int i = 0; i < retryCount * 7; i++) {
          cpu_relax();
          std::atomic_thread_fence(std::memory_order_seq_cst);
        }
      }
      retryCount++;
      goto restart;
    }
    if (cur->getDeleted()) {
      cur->writeUnlock();
      goto restart;
    }
    if (!cur->checkRange(key)) {
      cur->writeUnlock();
      goto restart;
    }
    bool ret = cur->update(key, value);
    cur->writeUnlock();
    return ret;
  }

  bool
  remove(K key, ListNode_t *head)
  {
  restart:
    ListNode_t *cur = head;

    while (1) {
      if (cur->getMin() > key) {
        cur = cur->getPrev();
        continue;
      }
      if (!cur->checkRange(key)) {
        cur = cur->getNext();
        continue;
      }
      break;
    }
    if (!cur->writeLock()) {
      if (head->getPrev() != nullptr) head = head->getPrev();
      goto restart;
    }
    if (cur->getDeleted()) {
      if (head->getPrev() != nullptr) head = head->getPrev();
      cur->writeUnlock();
      goto restart;
    }
    if (!cur->checkRange(key)) {
      if (head->getPrev() != nullptr) head = head->getPrev();
      cur->writeUnlock();
      goto restart;
    }

    bool ret = cur->remove(key);
    cur->writeUnlock();
    return ret;
  }

  bool
  probe(K key, ListNode_t *head)
  {
  restart:
    ListNode_t *cur = head;

    while (1) {
      if (cur->getMin() > key) {
        cur = cur->getPrev();
        continue;
      }
      if (!cur->checkRange(key)) {
        cur = cur->getNext();
        continue;
      }
      break;
    }

    version_t readVersion = cur->readLock();
    // Concurrent Update
    if (!readVersion) {
      goto restart;
    }
    if (cur->getDeleted()) {
      goto restart;
    }
    if (!cur->checkRange(key)) {
      goto restart;
    }
    bool ret = false;
    ret = cur->probe(key);
    if (!cur->readUnlock(readVersion)) goto restart;
    return ret;
  }

  bool
  lookup(K key, Val_t &value, ListNode_t *head)
  {
    int retryCount = 0;
  restart:
    ListNode_t *cur = head;
    [[maybe_unused]] int count = 0;

    while (1) {
      if (cur->getMin() > key) {
        cur = cur->getPrev();
        continue;
      }
      if (!cur->checkRange(key)) {
        cur = cur->getNext();
        continue;
      }
      break;
    }

    version_t readVersion = cur->readLock();
    // Concurrent Update
    if (!readVersion) {
      if (retryCount > 20) {
        for (int i = 0; i < retryCount * 5; i++) {
          cpu_relax();
          std::atomic_thread_fence(std::memory_order_seq_cst);
        }
      }
      retryCount++;
      goto restart;
    }
    if (cur->getDeleted()) {
      goto restart;
    }
    if (!cur->checkRange(key)) {
      goto restart;
    }
    bool ret = false;
    ret = cur->lookup(key, value);
    if (!cur->readUnlock(readVersion)) goto restart;
    return ret;
  }

  uint64_t
  scan(K startKey, size_t range, std::vector<Val_t> &rangeVector, ListNode_t *head)
  {
  restart:
    ListNode_t *cur = head;
    rangeVector.clear();
    // Find the start Node
    while (1) {
      if (cur->getMin() > startKey) {
        cur = cur->getPrev();
        continue;
      }
      if (!cur->checkRange(startKey)) {
        cur = cur->getNext();
        continue;
      }
      break;
    }
    bool end = false;
    assert(rangeVector.size() == 0);
    while (rangeVector.size() < range && !end) {
      version_t readVersion = cur->readLock();
      // Concurrent Update
      if (!readVersion) goto restart;
      if (cur->getDeleted()) {
        goto restart;
      }
      end = cur->scan(startKey, range, rangeVector, readVersion);
      if (!cur->readUnlock(readVersion)) goto restart;
      cur = cur->getNext();
    }
    return rangeVector.size();
  }

  void
  print(ListNode_t *head)
  {
    ListNode_t *cur = head;
    while (cur->getNext() != nullptr) {
      cur->print();
      cur = cur->getNext();
    }
    std::cout << "\n";
    return;
  }

  uint32_t
  size(ListNode_t *head)
  {
    ListNode_t *cur = head;
    int count = 0;
    while (cur->getNext() != nullptr) {
      count++;
      cur = cur->getNext();
    }
    return count;
  }

  ListNode_t *
  getHead()
  {
    return head;
  }
};

#endif  //_LINKEDLIST_H
