#include "hydralist/HydraList.h"

#include <gtest/gtest.h>

/*######################################################################################
 * Global constants
 *####################################################################################*/
#define RAND_NUMS_SIZE 1000000
int randNums[RAND_NUMS_SIZE];

/*######################################################################################
 * Fixture definition
 *####################################################################################*/
template <class K>
class hydraListTest : public ::testing::Test
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

  using HydraList_t = HydraList<K>;

  static void
  insert(int threadId, int n, int MAX_THREAD, HydraList_t *hl)
  {
    int range = n / MAX_THREAD;
    hl->registerThread();
    for (int i = range * threadId; i < range * (threadId + 1); i++) {
      if (i == 0) continue;
      ASSERT_EQ(true, hl->insert(i, i));
    }
    hl->unregisterThread();
  }

  static void
  concRemove(int threadId, int n, int MAX_THREAD, HydraList_t *hl)
  {
    int range = n / MAX_THREAD;
    hl->registerThread();
    for (int i = range * threadId; i < range * (threadId + 1); i++) {
      if (i == 0) continue;
      ASSERT_EQ(true, hl->remove(i));
    }
    hl->unregisterThread();
  }

  static void
  generateRandomNums(int n)
  {
    std::set<int> testSet;
    int count = 0;
    while (count < n) {
      int r = rand() % RAND_NUMS_SIZE;
      if (testSet.count(r)) continue;
      testSet.insert(r);
      randNums[count++] = r;
    }
  }

  static void
  insert1(int threadId, int n, int MAX_THREAD, HydraList_t *hl)
  {
    int range = n / MAX_THREAD;
    hl->registerThread();
    for (int i = range * threadId; i < range * (threadId + 1); i++) {
      ASSERT_EQ(true, hl->insert(randNums[i], randNums[i]));
    }
    hl->unregisterThread();
  }

  static void
  concLookup(int threadId, int n, int MAX_THREAD, HydraList_t *hl)
  {
    int range = n / MAX_THREAD;
    hl->registerThread();
    for (int i = range * threadId; i < range * (threadId + 1); i++) {
      ASSERT_EQ(randNums[i], hl->lookup(randNums[i]));
    }
    hl->unregisterThread();
  }
};

using TestTargets = ::testing::Types<uint64_t>;

TYPED_TEST_SUITE(hydraListTest, TestTargets);

TYPED_TEST(hydraListTest, sanityTest)
{
  typename TestFixture::HydraList_t hl(1);
  hl.registerThread();
  ASSERT_EQ(true, hl.insert(10, 10));
  ASSERT_EQ(10, hl.lookup(10));
  hl.unregisterThread();
}

TYPED_TEST(hydraListTest, splitTest)
{
  int n = 10000;
  typename TestFixture::HydraList_t *hl = new typename TestFixture::HydraList_t(1);
  hl->registerThread();

  for (int i = 1; i < n; i++) {
    ASSERT_EQ(true, hl->insert(i, i));
  }
  // waiting for combiner and worker thread to catch up;
  sleep(1);
  for (int i = 1; i < n; i++) {
    ASSERT_EQ(i, hl->lookup(i));
  }
  hl->unregisterThread();
  delete hl;
}

TYPED_TEST(hydraListTest, concurrentInsert)
{
  int n = 100000;
  typename TestFixture::HydraList_t *hl = new typename TestFixture::HydraList_t(1);
  int MAX_THREAD = 200;
  std::thread *threads[MAX_THREAD];
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i] = new std::thread(TestFixture::insert, i, n, MAX_THREAD, hl);
  }
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i]->join();
  }
  sleep(1);
  hl->registerThread();
  for (int i = 1; i < n; i++) {
    if (hl->lookup(i) != i) std::cout << i << std::endl;
    ASSERT_EQ(i, hl->lookup(i));
  }
  hl->unregisterThread();
  delete hl;
}

TYPED_TEST(hydraListTest, remove)
{
  int n = 100000;
  typename TestFixture::HydraList_t *hl = new typename TestFixture::HydraList_t(1);
  int MAX_THREAD = 8;
  std::thread *threads[MAX_THREAD];
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i] = new std::thread(TestFixture::insert, i, n, MAX_THREAD, hl);
  }
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i]->join();
  }
  sleep(1);
  hl->registerThread();
  for (int i = 1; i < n; i++) {
    if (hl->lookup(i) != i) std::cout << i << std::endl;
    ASSERT_EQ(i, hl->lookup(i));
  }
  for (int i = n - 1; i >= 1; i--) {
    bool ret = hl->remove(i);
    ASSERT_EQ(true, ret);
  }
  hl->unregisterThread();
  delete hl;
}

TYPED_TEST(hydraListTest, concurrentRemove)
{
  int n = 100000;
  typename TestFixture::HydraList_t *hl = new typename TestFixture::HydraList_t(1);
  int MAX_THREAD = 8;
  std::thread *threads[MAX_THREAD];
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i] = new std::thread(TestFixture::insert, i, n, MAX_THREAD, hl);
  }
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i]->join();
  }
  sleep(1);
  hl->registerThread();
  for (int i = 1; i < n; i++) {
    if (hl->lookup(i) != i) std::cout << i << std::endl;
    ASSERT_EQ(i, hl->lookup(i));
  }
  hl->unregisterThread();
  std::thread *threads1[MAX_THREAD];
  for (int i = 0; i < MAX_THREAD; i++) {
    threads1[i] = new std::thread(TestFixture::concRemove, i, n, MAX_THREAD, hl);
  }
  for (int i = 0; i < MAX_THREAD; i++) {
    threads1[i]->join();
  }
  hl->registerThread();
  for (int i = 1; i < n; i++) {
    ASSERT_NE(i, hl->lookup(i));
  }
  hl->unregisterThread();
  delete hl;
}

TYPED_TEST(hydraListTest, scan)
{
  int n = 100000;
  typename TestFixture::HydraList_t *hl = new typename TestFixture::HydraList_t(1);
  int MAX_THREAD = 4;
  std::thread *threads[MAX_THREAD];
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i] = new std::thread(TestFixture::insert, i, n, MAX_THREAD, hl);
  }
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i]->join();
  }
  sleep(1);
  hl->registerThread();
  int range = 100;
  std::vector<Val_t> result(range);
  uint64_t startKey = 100;

  uint64_t resultSize = hl->scan(startKey, range, result);
  ASSERT_EQ(resultSize, range);
  for (uint64_t i = 0; i < resultSize; i++) {
    ASSERT_EQ(result[i], startKey + i);
  }
  hl->unregisterThread();
  delete hl;
}

TYPED_TEST(hydraListTest, concLookup)
{
  int n = 100000;
  typename TestFixture::HydraList_t *hl = new typename TestFixture::HydraList_t(1);
  int MAX_THREAD = 8;
  std::thread *threads[MAX_THREAD];
  TestFixture::generateRandomNums(n * 2);

  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i] = new std::thread(TestFixture::insert1, i, n, MAX_THREAD, hl);
  }
  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i]->join();
    delete threads[i];
  }

  for (int i = 0; i < MAX_THREAD; i++) {
    if (i > MAX_THREAD / 2)
      threads[i] = new std::thread(TestFixture::insert1, i + MAX_THREAD, n, MAX_THREAD, hl);
    else
      threads[i] = new std::thread(TestFixture::concLookup, i, n, MAX_THREAD, hl);
  }

  for (int i = 0; i < MAX_THREAD; i++) {
    threads[i]->join();
    delete threads[i];
  }
  delete hl;
}

int
main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
