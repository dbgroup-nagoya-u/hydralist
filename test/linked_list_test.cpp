#include <gtest/gtest.h>

#include <climits>
#include <thread>
#include <vector>

#include "hydralist/component/Oplog.h"
#include "hydralist/component/VersionedLock.h"
#include "hydralist/component/linkedList.h"

/*######################################################################################
 * Global constants
 *####################################################################################*/

/*######################################################################################
 * Fixture definition
 *####################################################################################*/
template <class K>
class linkedListTest : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
  }

  void
  TearDown() override
  {
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  using LinkedList_t = LinkedList<K>;
  using ListNode_t = ListNode<K>;

  static void
  multithreadInsert(
      LinkedList_t *ll, int threadId, int numInserts, int MAX_THREADS, std::vector<int> &nums)
  {
    int range = numInserts / MAX_THREADS;
    for (volatile int i = range * threadId; i < (range * (threadId + 1)); i++) {
      ll->insert(nums[i], nums[i], ll->getHead());
    }
  }

  static void
  multithreadRemove(
      LinkedList_t *ll, int threadId, int numInserts, int MAX_THREADS, std::vector<int> *numsPtr)
  {
    std::vector<int> &nums = *numsPtr;
    int range = numInserts / MAX_THREADS;
    for (volatile int i = range * threadId; i < range * (threadId + 1); i++) {
      uint64_t x = nums[i];
      ll->remove(x, ll->getHead());
    }
  }
};

using TestTargets = ::testing::Types<uint64_t, uint32_t>;

TYPED_TEST_SUITE(linkedListTest, TestTargets);

TYPED_TEST(linkedListTest, initializeTest)
{
  typename TestFixture::LinkedList_t testList;
  ASSERT_NE(nullptr, testList.initialize());  // NOLINT
  ASSERT_EQ(1, testList.getHead()->getNumEntries());
  ASSERT_NE(nullptr, testList.getHead()->getNext());
}

TYPED_TEST(linkedListTest, insertRemoveTest)
{
  typename TestFixture::LinkedList_t testList;
  typename TestFixture::ListNode_t *head = testList.initialize();

  Val_t value = 100;
  testList.insert(100, value, testList.getHead());
  testList.insert(200, value, head);
  testList.insert(50, value, head);
  testList.insert(500, value, head);
  ASSERT_EQ(true, testList.lookup(100, value, head));
  ASSERT_EQ(true, testList.lookup(200, value, head));
  ASSERT_EQ(true, testList.lookup(500, value, head));
  ASSERT_EQ(true, testList.lookup(50, value, head));
  ASSERT_EQ(5, head->getNumEntries());
  ASSERT_EQ(true, testList.lookup(0, value, head));
  ASSERT_EQ(false, testList.insert(100, value, head));

  testList.remove(100, head);
  testList.remove(50, head);
  ASSERT_EQ(false, testList.lookup(50, value, head));
  ASSERT_EQ(false, testList.lookup(100, value, head));
  ASSERT_EQ(true, testList.lookup(500, value, head));
  ASSERT_EQ(3, head->getNumEntries());
}

TYPED_TEST(linkedListTest, splitTest)
{
  typename TestFixture::LinkedList_t testList;
  typename TestFixture::ListNode_t *head = testList.initialize();

  int testsize = 200;
  std::set<int> randset;
  std::vector<int> randnums;

  for (int i = 0; i < testsize; i++) {
    int key = rand();
    if (!randset.count(key)) {
      randset.insert(key);
      randnums.push_back(key);
    }
  }

  for (auto x : randnums) {
    ASSERT_EQ(true, testList.insert(x, x, head));
  }
  for (auto x : randnums) {
    Val_t y;
    ASSERT_EQ(true, testList.lookup(x, y, head));
  }
  for (auto x : randnums) {
    ASSERT_EQ(true, testList.remove(x, head));
  }
}

TYPED_TEST(linkedListTest, multithread1)
{
  int MAX_THREADS = 8;
  int numInserts = 10000;
  std::vector<int> nums(numInserts);
  typename TestFixture::LinkedList_t ll;
  ll.initialize();
  for (int i = 0; i < numInserts; i++) {
    nums[i] = rand();
    if (nums[i] == 0 || static_cast<size_t>(nums[i]) == ULLONG_MAX) i--;
  }
  std::thread *threads[MAX_THREADS];
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i] = new std::thread(TestFixture::multithreadInsert, &ll, i, numInserts, MAX_THREADS,
                                 std::ref(nums));
  }
  for (int i = 0; i < MAX_THREADS; i++) threads[i]->join();

  Val_t val;
  for (int i = 0; i < numInserts; i++) {
    ASSERT_EQ(true, ll.lookup(nums[i], val, ll.getHead()));
  }
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i] =
        new std::thread(TestFixture::multithreadRemove, &ll, i, numInserts, MAX_THREADS, &nums);
  }
  for (int i = 0; i < MAX_THREADS; i++) threads[i]->join();
}

TYPED_TEST(linkedListTest, scanTest)
{
  int MAX_THREADS = 1;
  int numInserts = 10000;
  std::vector<int> nums(numInserts);
  typename TestFixture::LinkedList_t ll;
  ll.initialize();
  for (int i = 1; i < numInserts + 1; i++) {
    nums[i] = i;
  }
  std::thread *threads[MAX_THREADS];
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i] = new std::thread(TestFixture::multithreadInsert, &ll, i, numInserts, MAX_THREADS,
                                 std::ref(nums));
  }
  for (int i = 0; i < MAX_THREADS; i++) threads[i]->join();

  std::vector<Val_t> result(100);
  uint64_t resultSize = ll.scan(100, 100, result, ll.getHead());
  ASSERT_EQ(resultSize, 100);
  for (uint32_t i = 0; i < resultSize; i++) {
    ASSERT_EQ(result[i], 100 + i);
  }
}

TYPED_TEST(linkedListTest, sanitytest)
{
  VersionedLock lock;
  std::vector<int> tvector;
  lock.write_lock();
  tvector.push_back(1);
  tvector.push_back(2);
  tvector.push_back(3);
  lock.write_unlock();
  version_t ver = lock.read_lock();
  ASSERT_EQ(4, ver);
  bool success = lock.read_unlock(ver);
  ASSERT_EQ(true, success);
}

TYPED_TEST(linkedListTest, InterleaveRW)
{
  VersionedLock lock;
  std::vector<int> tvector;
  version_t ver = lock.read_lock();
  ASSERT_EQ(2, ver);
  lock.write_lock();
  tvector.push_back(1);
  tvector.push_back(2);
  tvector.push_back(3);
  lock.write_unlock();
  bool success = lock.read_unlock(ver);
  ASSERT_EQ(false, success);
  ver = lock.read_lock();
  ASSERT_EQ(4, ver);
  success = lock.read_unlock(ver);
  ASSERT_EQ(true, success);
}

// TODO Write lock unit test again since api has changed
int
main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
