#include "linkedList.h"

#include <cassert>
#include <climits>
#include <iostream>
// std::atomic<int> numSplits;

#define cpu_relax() asm volatile("pause\n" : : : "memory")
ListNode *
LinkedList::initialize()
{
  ListNode *head = new (ListNode);
  ListNode *tail = new (ListNode);

  head->setNext(tail);
  head->setPrev(nullptr);
  std::string minString = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
  std::string maxString = "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
  Key_t max;
  max.setFromString(maxString);
  Key_t min;
  min.setFromString(minString);
  head->setMin(min);
  head->insert(min, 0);
  head->setPrefixLength(0);
  tail->insert(max, 0);
  tail->setNumEntries(MAX_ENTRIES);  // This prevents merge of tail
  tail->setNext(nullptr);
  tail->setPrev(tail);
  tail->setMin(max);
  tail->setPrefixLength(0);
  this->head = head;

  return head;
}

bool
LinkedList::insert(Key_t &key, Val_t value, ListNode *head)
{
  int retryCount = 0;
restart:
  ListNode *cur = head;

  while (1) {
    int cmp = cur->checkRange(key);
    if (cmp == -1) {
      cur = cur->getPrev();
    } else if (cmp == 1) {
      cur = cur->getNext();
    } else
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
  bool ret = cur->insert(key, value);
  cur->writeUnlock();
  return ret;
}

bool
LinkedList::update(Key_t &key, Val_t value, ListNode *head)
{
  int retryCount = 0;
restart:
  ListNode *cur = head;

  while (1) {
    int cmp = cur->checkRange(key);
    if (cmp == -1) {
      cur = cur->getPrev();
    } else if (cmp == 1) {
      cur = cur->getNext();
    } else
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

  bool ret = cur->update(key, value);
  cur->writeUnlock();
  return ret;
}

bool
LinkedList::remove(Key_t &key, ListNode *head)
{
restart:
  ListNode *cur = head;
  while (1) {
    int cmp = cur->checkRange(key);
    if (cmp == -1) {
      cur = cur->getPrev();
    } else if (cmp == 1) {
      cur = cur->getNext();
    } else
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

  bool ret = cur->remove(key);
  cur->writeUnlock();
  return ret;
}

bool
LinkedList::probe(Key_t &key, ListNode *head)
{
restart:
  ListNode *cur = head;
  // int count = 0;
  // if(cur->getMin() > key)
  //    std::atomic_fetch_add(&numSplits, 1);
  while (1) {
    int cmp = cur->checkRange(key);
    if (cmp == -1) {
      cur = cur->getPrev();
    } else if (cmp == 1) {
      cur = cur->getNext();
    } else
      break;
  }

  version_t readVersion = cur->readLock();
  // Concurrent Update
  if (!readVersion) goto restart;
  if (cur->getDeleted()) {
    goto restart;
  }
  bool ret = false;
  ret = cur->probe(key);
  if (!cur->readUnlock(readVersion)) goto restart;
  return ret;
}

bool
LinkedList::lookup(Key_t &key, Val_t &value, ListNode *head)
{
  int retryCount = 0;
restart:
  ListNode *cur = head;
  int count = 0;
  // if(cur->getMin() > key)
  //    std::atomic_fetch_add(&numSplits, 1);

  while (1) {
    int cmp = cur->checkRange(key);
    if (cmp == -1) {
      cur = cur->getPrev();
    } else if (cmp == 1) {
      cur = cur->getNext();
    } else
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
  bool ret = false;
  ret = cur->lookup(key, value);
  if (!cur->readUnlock(readVersion)) goto restart;
  return ret;
}

void
LinkedList::print(ListNode *head)
{
  ListNode *cur = head;
  while (cur->getNext() != nullptr) {
    cur->print();
    cur = cur->getNext();
  }
  std::cout << "\n";
  return;
}

uint32_t
LinkedList::size(ListNode *head)
{
  ListNode *cur = head;
  int count = 0;
  while (cur->getNext() != nullptr) {
    count++;
    cur = cur->getNext();
  }
  return count;
}

ListNode *
LinkedList::getHead()
{
  return head;
}

uint64_t
LinkedList::scan(Key_t &startKey, int range, std::vector<Val_t> &rangeVector, ListNode *head)
{
restart:
  ListNode *cur = head;
  rangeVector.clear();
  // Find the start Node
  while (1) {
    int cmp = cur->checkRange(startKey);
    if (cmp == -1) {
      cur = cur->getPrev();
    } else if (cmp == 1) {
      cur = cur->getNext();
    } else
      break;
  }
  bool end = false;
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
