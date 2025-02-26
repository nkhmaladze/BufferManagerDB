#include <string>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

#include <UnitTest++/UnitTest++.h>
#include <UnitTest++/TestReporterStdout.h>
#include <UnitTest++/TestRunner.h>

#include "swatdb_exceptions.h"
#include "diskmgr.h"
#include "bufmgr.h"
#include "page.h"
#include "catalog.h"
#include "file.h"

// uncomment BMGR_DEBUG definition to print out a summary of each test
#define BMGR_DEBUG 1
#ifdef BMGR_DEBUG
#define PRINT(s) std::cout << s
#else
#define PRINT(s) 
#endif

/*
 * Unit tests for BufferManager class.
 */

/*
 * TestFixture Class for initializing and cleaning up objects. Any test called
 * with this class as TEST_FIXTURE has access to any public and protected data
 * members and methods of the object as if they were local variables and
 * helper functions of test functions. The constructor is called at the start
 * of the test to initialize the data members and destructor is called at the
 * end of the test to deallocate/free memory associated with the objects.
 * It is possible to declare another custom class and associate with tests via
 * TEST_FIXTURE. It is also possible to add more data members and functions.
 * Students are encouraged to do so as they see fit so lon as they are careful
 * not to cause any naming conflicts with other tests.
 */
static RepType rep_pol = ClockT;
class TestFixture{

  public:
    /*
     * Public data members that tests have access to as if they are local
     * variables.
     */
    Catalog* catalog;
    DiskManager* disk_mgr;
    BufferManager* buf_mgr;
    std::string file_name;
    FileId file_id;

    TestFixture(){

      /*
       * Initializes all variables needed for testing. All DBMS objects are
       * created and initialized. A file, named "testre.rel" is added through
       * the buf_mgr.
       */
      catalog = new Catalog();
      disk_mgr = new DiskManager(catalog);
      //std::cout << "before constructor" << std::endl;
      this->buf_mgr = new BufferManager(disk_mgr, rep_pol);
    //  std::cout << "after constructor" << std::endl;
      file_name = "testrel1.rel";
      file_id = catalog->addEntry(file_name, nullptr, nullptr, nullptr, 
          HeapFileT, INVALID_FILE_ID, file_name);
      this->buf_mgr->createFile(file_id);
    }

    /*
     * Clean up and deallocates all objects initilalized by the constructor.
     * The unix file is explicitly removed.
     */
    ~TestFixture(){
      delete this->buf_mgr;
      delete disk_mgr;
      delete catalog;
      remove(file_name.data());
    }

    /**
     * helper function to print out buffer pool and replacement
     * policy state
     */
    void printBufferState() {
      std::cout << "\nBuffer Pool State:\n--------------------"
        << std::endl;
      this->buf_mgr->printBufferState();
      std::cout << "--------------------" << std::endl;
    }


    /*
     * Helper function for checking the buffer state of the buffer pool. Only
     * the number of valid pages, pinned pages, and dirty pages are checked as
     * other state variables are either constant or may change depending on the
     * implementation details
     */
    void checkBufferState(std::uint32_t valid,
        std::uint32_t pinned, std::uint32_t dirty){
      BufferState cur_buf = this->buf_mgr->getBufferState();
      CHECK_EQUAL(valid, cur_buf.valid);
      CHECK_EQUAL(pinned, cur_buf.pinned);
      CHECK_EQUAL(dirty, cur_buf.dirty);
    }

    /*
     * Helper function for allocating BUF_SIZE pages and bringing them all
     * into the buffer pool.
     */
    std::vector<PageId> 
      fillBufferPool(std::uint32_t extra, std::vector<Page *> *page_data){
      std::vector<PageId> allocated_pages;
      for (std::uint32_t i =0; i < BUF_SIZE+extra; i++){
        allocated_pages.push_back(disk_mgr->allocatePage(file_id));
      }

      for (std::uint32_t i =0; i < BUF_SIZE; i++){
        page_data->push_back(this->buf_mgr->getPage(allocated_pages.at(i)));
        // fill each page with an array of specific char
        memset(page_data->at(i)->getData(), i%128, PAGE_SIZE);
        // make sure that each page has unique content by writing
        // its page number to starting bytes
        sprintf(page_data->at(i)->getData(),"%d ",allocated_pages[i].page_num);
      }
      return allocated_pages;
    }
};



SUITE(replacementTests){


  /**
   * Pins all the pages in the buffer pool and attempts to pin another, checks
   * that insufficient space error is thrown.
   */
TEST_FIXTURE(TestFixture, exceptionTest){

  std::vector<PageId> allocated_pages;
  std::vector<Page *> page_data;
  // fills buffer pool and creates one extra pages that can be added
  allocated_pages = this->fillBufferPool(2, &page_data);

  PRINT("TEST: Pins all the pages in the buffer pool and attempts to pin\n");
  PRINT("      another, checks that insufficient space error is throw\n");

  // checks if InsufficientSpaceBufMgr is thrown when buffer is full
  CHECK_THROW(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE)),
      InsufficientSpaceBufMgr);

  // sanity check, release and add again, make sure error is still thrown.

  // release and get 3 pages
  for(std::uint32_t i = 1; i < 4 ; i++){
    this->buf_mgr->releasePage(allocated_pages.at(i), true);
    this->buf_mgr->getPage(allocated_pages.at(i));
  }

  CHECK_THROW(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE + 1)),
      InsufficientSpaceBufMgr);

#ifdef BMGR_DEBUG
    printBufferState();
#endif

}
  /*
   * Unpins one page and checks that it replaces that singular page when
   * getPage is called.
   */
  TEST_FIXTURE(TestFixture, basicTest){
    std::vector<PageId> allocated_pages;
    Page *last_page;

  PRINT("TEST: Pin all pages in buffer pool, unpin one, check that\n");
  PRINT("      it is the page replaced.\n");

    for (std::uint32_t i =0; i < BUF_SIZE+1; i++){
      allocated_pages.push_back(disk_mgr->allocatePage(file_id));
    }
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      last_page = this->buf_mgr->getPage(allocated_pages.at(i));
    }

    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE-1), false);
    Page *temp_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE));

    CHECK_EQUAL(temp_page, last_page);
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }


  /*
   * Unpins one page and frees one page, checks that replacement returns free
   * page first, regardless of order they were unpinned in or order they are in
   * frame_table.
   */
  TEST_FIXTURE(TestFixture, basicFreeTest){
    std::vector<PageId> allocated_pages;
    std::vector<Page *> page_data;

    PRINT("TEST: Unpins one page and frees one page, check that page\n");
    PRINT("      freed is the one returned by replacement policy\n");

    allocated_pages = this->fillBufferPool(2, &page_data);

    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE-1), true);
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE-2), true);
    this->buf_mgr->deallocatePage(allocated_pages.at(BUF_SIZE-2));

    CHECK_EQUAL(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE)), 
        page_data.at(BUF_SIZE-2));


    this->buf_mgr->releasePage(allocated_pages.at(2), true);
    this->buf_mgr->deallocatePage(allocated_pages.at(2));
    this->buf_mgr->releasePage(allocated_pages.at(1), true);

    CHECK_EQUAL(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE+1)), 
        page_data.at(2));

#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }


  /*
   * Gets/pins every page in the bufferpool 5 more times
   * then fills each page with array of specific char. Unpins some pages
   * different number of times such that only one page is completely unpinned.
   * Gets the last allocated page into the buffer pool and checks if the
   * unpinned page is evicted. Checks the final buffer pool state.
   */
  TEST_FIXTURE(TestFixture, checkEvicted){
    std::vector<PageId> allocated_pages;
    Page* temp_page;
    Page* evicted_page;

    PRINT("TEST: gets/pins every page 5 times, write, upins some num times\n");
    PRINT("      only 1 w/pincount 0, allocate, check unpinned page evicted\n");

    //allcoate BUF_SIZE number of pages in memory
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      allocated_pages.push_back(this->buf_mgr->allocatePage(file_id).second);
    }
    //allocate one more page through disk manager
    allocated_pages.push_back(disk_mgr->allocatePage(file_id));

    // pin the page 5 more times and fill each page with array of
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      for (std::uint32_t j =0;j < 5 ; j++){
          temp_page = this->buf_mgr->getPage(allocated_pages.at(i));
      }
      sprintf(temp_page->getData(),"%d ",
          allocated_pages.at(i).page_num);
      // Page that will be eventually evicted.
      if (i == 6){
        evicted_page = temp_page;
      }
    }

    // Release pages, but for different times. Release the first page one time,
    // release the second page twice, etc. At the end, there should be a single
    // page that is completely unpinned.
    for(std::uint32_t i = 0; i < 7; i++){
      // After 6 pages are released different number of times.
      // Only the 6th page (evicted page) is completely unpinned.
      for(std::uint32_t j = 0; j < i; j++){
        this->buf_mgr->releasePage(allocated_pages.at(i), true);
      }
    }

    // get the last allocated page and check if unpinned page is evicted.
    // this is just checking that the pointers to the Pages in the
    // buffer pool are the same address (which BP frame of memory was
    // replaced)
    CHECK_EQUAL(evicted_page, 
        this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE)));
    checkBufferState(BUF_SIZE, BUF_SIZE, 5);
#ifdef BMGR_DEBUG
    printBufferState();
#endif

  }

  /*
   * First adds a file entry to the catalog, and create the file through the
   * BufferManager. Allocates BUF_SIZE+1 pages through the DiskManager and fill
   * the buffer with BUF_SIZE number of the allocated pages by calling getPage
   * and fills each Page with an array of specific char. Releases two pages,
   * but deallocates only one of them. Get last allocated page and check that
   * the the deallocated page of the 2 released pages was evicted.  Checks the
   * final buffer state.
   */
  TEST_FIXTURE(TestFixture, checkDeallocate){

    std::vector<PageId> allocated_pages;
    Page *last_page;
    std::vector<Page *> page_data;

    PRINT("TEST: get allocated pages, then release 2 deallocate 1\n");
    PRINT("      get another page: check that deallocated page evicted\n");
    // fills buffer pool and creates one extra pages that can be added
    allocated_pages = this->fillBufferPool(1, &page_data);

    // checks if InsufficientSpaceBufMgr is thrown when buffer is full
    CHECK_THROW(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE)),
        InsufficientSpaceBufMgr);

    // release 2 pages but only deallocate the second to last allocated one.
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE - 1), true);
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE - 2), true);
    this->buf_mgr->deallocatePage(allocated_pages.at(BUF_SIZE - 1));

    // get the last allocated page
    last_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE));

    // checks that dealllocated page was evicted.
    // checks that new page is taken from the free list
    CHECK_EQUAL(page_data.at(BUF_SIZE - 1), last_page);
    CHECK(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE - 2)) 
        != last_page);
    // checks that 2nd to last page in vector is not flushed
    // as the deallocated page is evicted by the clock algorithm
    // returns true if they are different
    Page *new_page = new Page();
    disk_mgr->readPage(allocated_pages.at(BUF_SIZE-2), new_page);
    CHECK(memcmp(this->buf_mgr->getPage(
        allocated_pages.at(BUF_SIZE-2))->getData(), new_page->getData(),
        PAGE_SIZE));

    // check current buffer state
    checkBufferState(BUF_SIZE, BUF_SIZE, 1);
#ifdef BMGR_DEBUG
    printBufferState();
#endif

    delete new_page;
  }

  /*
   * First adds a file entry to the catalog, and create the file through the
   * BufferManager. Allocates BUF_SIZE+1 pages through the DiskManager and fill
   * the buffer with BUF_SIZE number of the allocated pages by calling getPage
   * and fills each Page with an array of specific char. Release a page, check
   * it is one replaced by next getpage to page not in buffer pool.  Relase
   * another page, then get page replaced, check it is read in and replaces
   * the only release page.  Checks the final buffer state.
   */
  TEST_FIXTURE(TestFixture, checkFlush){
    std::vector<PageId> allocated_pages;
    std::vector<Page *> page_data;

    PRINT("TEST: fill BP & write, release 1 page, get page not in. chk it\n");
    PRINT("      replaces & writes released pg. Repeat w/pg just replaced.\n");

    // fills buffer pool and creates one extra pages that can be added
    allocated_pages = this->fillBufferPool(1, &page_data);

    // checks if InsufficientSpaceBufMgr is thrown when buffer is full
    CHECK_THROW(this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE)),
        InsufficientSpaceBufMgr);

    char *new_data = new char[PAGE_SIZE];
    memcpy(new_data, page_data.at(BUF_SIZE - 1)->getData(), PAGE_SIZE);
    // release 2 pages but only deallocate the second to last allocated one.
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE - 1), true);

    // get the last allocated page
    this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE));

    this->buf_mgr->releasePage(allocated_pages.at(1), true);

    this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE - 1));

    // checks that 2nd to last page in vector is not flushed
    // as the deallocated page is evicted by the clock algorithm
    // returns true if they are different
    Page *new_page = new Page();
    disk_mgr->readPage(allocated_pages.at(BUF_SIZE-1), new_page);

    //std::cout << memcmp(new_data, new_page->getData(),PAGE_SIZE) << std::endl;
    CHECK(!memcmp(new_data, new_page->getData(),PAGE_SIZE));

    // check current buffer state
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif
    delete[] new_data;
    delete new_page;
  }

}




/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittests -h help\n";

}
/*
 * The main program either run all tests or tests a specific SUITE, given its
 * name via command line argument option 's'. If no argument is given by
 * argument option 's', main runs all tests by default. If invalid argument is
 * given by option 's', 0 test is run
 */
int main(int argc, char** argv){
  const char* suite_name;
  bool test_all = true;
  int c;

  //check for suite_name argument if provided
  while ((c = getopt (argc, argv, "hs:")) != -1){
    switch(c) {
      case 'h': usage();
                exit(1);
      case 's': suite_name = optarg;
                test_all  = false;
                break;
      default: printf("optopt: %c\n", optopt);

    }
  }

  //run all tests
  if (test_all){
    return UnitTest::RunAllTests();
  }

  //run the SUITE of the given suite name
  UnitTest::TestReporterStdout reporter;
  UnitTest::TestRunner runner(reporter);
  return runner.RunTestsIf(UnitTest::Test::GetTestList(), suite_name,
      UnitTest::True(), 0);
}
