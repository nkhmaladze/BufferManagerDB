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

/*
 * Unit tests for Clock Replacement algorithm class.
 */
// uncomment BMGR_DEBUG definition to print out a summary of each test
#define BMGR_DEBUG 1
#ifdef BMGR_DEBUG
#define PRINT(s) std::cout << s
#else
#define PRINT(s)
#endif


/*
 * TestFixture Class for initializing and cleaning up objects. Any test called
 * with this class as TEST_FIXTURE has access to any public and protected data
 * members and methods of the object as if they were local variables and
 * helper functions of test functions. The constructor is called at the start
 * of the test to initialize the data members and destructor is called at the
 * end of the test to deallocate/free memory associated with the objects.
 * It is possible to declare another custom class and associate with tests via
 * TEST_FIXTURE. It is also possible to add more data members and functions.
 * Students are encouraged to do so as they see fit so long as they are careful
 * not to cause any naming conflicts with other tests.
 */

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
      RepType rep_pol = ClockT;
      this->buf_mgr = new BufferManager(disk_mgr, rep_pol);
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
    std::vector<PageId> fillBufferPool(std::uint32_t extra, 
        std::vector<Page *> *page_data)
    {
      std::vector<PageId> allocated_pages;
      for (std::uint32_t i =0; i < BUF_SIZE+extra; i++){
        allocated_pages.push_back(disk_mgr->allocatePage(file_id));
      }
      // fill each page with an array of specific char,
      // make sure that each page has unique content by writing
      // its page number to starting bytes
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



SUITE(clockTests){
  /**
   * Pins every page in the buffer pool and then unpins some of those pages.
   * Gets two new pages and checks that the correct page is replaced according
   * to the clock algorithm.
   */
  TEST_FIXTURE(TestFixture, basicTest){
    Page *extra_page;
    std::vector<Page *> page_data;

    PRINT("TEST: Fill buffer pool, unpin some pages, get 2 new check\n");
    PRINT("      correct 2 are replaced according to clock algorithm\n"); 

    // fills buffer pool and creates two extra pages that can be added
    std::vector<PageId> allocated_pages = this->fillBufferPool(2, &page_data);
    // unpin page at frame 2 and frame BUF_SIZE-2
    this->buf_mgr->releasePage(allocated_pages.at(2), true);
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE-2), true);

    // get new page, will call replacement policy as all frames are valid
    extra_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE));
    // should select page at frame 2 for eviction
    CHECK_EQUAL(extra_page, page_data.at(2));

    // release 2 more pages, they should not be chosen for eviction as their
    // ref_bit will be true
    this->buf_mgr->releasePage(allocated_pages.at(0), true);
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE/2), true);
    // get new page, will call replacement policy as all frames are valid
    extra_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE+1));
    // should select page at frame BUF_SIZE-2 for eviction
    CHECK_EQUAL(extra_page, page_data.at(BUF_SIZE-2));
    // this->buf_mgr->printBufferState();
    
#ifdef BMGR_DEBUG
    printBufferState();
#endif 
  }


  /**
   * Pins every page in the buffer pool and then unpins all of them in opposite
   * order. Then, gets BUF_SIZE - 1 new pages and checks they were added in
   * increasing order in buf pool. Exemplifies difference betweeen LRU and
   * Clock, because frames replaced will actually be the most recently used.
   */
  TEST_FIXTURE(TestFixture, clockOrderTest){
    Page *temp_page;
    std::vector<Page *> page_data;

    PRINT("TEST: pins every page, unpins in opposite order, lots of\n");
    PRINT("      getPages should fill in clock hand order NOT LRU order\n"); 

    // fills buffer pool and creates BUF_SIZE - 1 extra pages that can be added
    std::vector<PageId> allocated_pages = 
      this->fillBufferPool(BUF_SIZE - 1, &page_data);
    // releases pages in opposite order
    for(std::uint32_t i = BUF_SIZE - 1; i > 0; i--){
      this->buf_mgr->releasePage(allocated_pages.at(i), true);
    }
    // should add back pages in increasing order from the clock_hand
    for (std::uint32_t i = 1; i < BUF_SIZE; i++){
      temp_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE - 1 + i));
      CHECK_EQUAL(temp_page, page_data.at(i));
    }
#ifdef BMGR_DEBUG
    printBufferState();
#endif 
  }


  /**
   * Performs a sequential scan of BUF_SIZE + 1 different pages a set amount
   * of times. For clock algorithm, after the first scan, a page must be
   * be replaced on every call to getPage.  After the first full rotation
   * of the clock hand, it should find a replacement page immediately on
   * next invocation of the algorithm (next frame should have clear ref bit
   * and be unpinned)
   */
  TEST_FIXTURE(TestFixture, sequentialScanTest){
    Page *temp_page, *first_page;
    std::vector<Page *> page_data; // vector of pointers parallel to buf pool
                                   
    PRINT("TEST: perfoms N sequental scans of BUF_SIZE + 1 pages\n");
    PRINT("      after first scan, a page must be replaced on every getPage\n");
    // point to each page in buf pool
    // ex. page_data.at(0) points to the first page in the buf pool
    // fills buffer pool and creates two extra pages that can be added
    std::vector<PageId> allocated_pages;
    for (std::uint32_t i =0; i < BUF_SIZE + 1; i++){
      allocated_pages.push_back(disk_mgr->allocatePage(file_id));
    }
    // pin BUF_SIZE pages in buf pool from the free list.
    // unpin all of them except for page zero
    page_data.push_back(this->buf_mgr->getPage(allocated_pages.at(0)));
    // page zero is not released
    for (std::uint32_t i = 1; i < BUF_SIZE; i++){
      page_data.push_back(this->buf_mgr->getPage(allocated_pages.at(i)));
      this->buf_mgr->releasePage(allocated_pages.at(i), false);
      // all other pages are released and unpinned, ref_bit = 1
    }
    //getting the last page of the sequential scan, must replace at first
    // unpinned page, which should be the page at frame 1
    first_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE));
    this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE), false);

    CHECK_EQUAL(first_page, page_data.at(1));

    // i is keeping track of the relevant index of allocated_pages
    // scan_num = the amount of sequential scans of size BUF_SIZE + 1
    for(std::uint32_t scan_num = 1; scan_num < 5; scan_num++){
    // iterate through buffer pool from clock_hand to frame BUF_SIZE
      for(std::uint32_t i = 1; i < BUF_SIZE - scan_num ; i++){
        temp_page = this->buf_mgr->getPage(allocated_pages.at(i));
        this->buf_mgr->releasePage(allocated_pages.at(i), false);
        CHECK_EQUAL(temp_page, page_data.at(i + scan_num));
      }
     // at BUF_SIZE - scan_num reach end of buf pool
     // always want to start at the beginning of unpinned frame list (1)
      int c = 1; //corresponds with expected page_data position
      for(std::uint32_t i = BUF_SIZE - scan_num; i < BUF_SIZE + 1; i++){
        temp_page = this->buf_mgr->getPage(allocated_pages.at(i));
        this->buf_mgr->releasePage(allocated_pages.at(i), false);
        CHECK_EQUAL(temp_page, page_data.at(c));
        c++;
      }
    }
#ifdef BMGR_DEBUG
    printBufferState();
#endif 
  }


  /*
   * First adds a file entry to the catalog, and create the file through the
   * BufferManager. Allocates BUF_SIZE+5 pages and fill the buffer with the
   * allocated pages. Unpins 5 pages that are regularly spaced out.
   * Gets the last 5 allocated page into the buffer pool and checks if the
   * unpinned pages are evicted in proper order (in the direction of the
   * clockhand). Checks the final buffer pool state.
   */
  TEST_FIXTURE(TestFixture, replace5Frames){
    std::vector<PageId> allocated_pages;
    std::vector<Page*> page_data;
    Page* temp_page;

    PRINT("TEST: fills buffer pool, unpins 5 pages regularly spaced out\n");
    PRINT("      checks that 5 evictions are in propoer clock hand order\n");
    //allocate BUF_SIZE+5 number of pages
    allocated_pages = this->fillBufferPool(5, &page_data);

    //release pages in the released_pages.
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      if (i%(BUF_SIZE/6)==0 && i >0 && i < 6*(BUF_SIZE/6)){
        this->buf_mgr->releasePage(allocated_pages.at(i), true);
      }
    }

    //check if pages are evicted in order as the clockhand sweeps through the
    //buffer
    for(std::uint32_t i = 1; i < 6; i++){
      temp_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE-1+i));
      CHECK_EQUAL(page_data.at(i*(BUF_SIZE/6)), temp_page);

    //std::cout << "------------\nBP State:\n----------" << std::endl;
    //buf_mgr->printBufferState();
    //std::cout << "--------------" << std::endl;
    }

    //check current buffer state
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);

#ifdef BMGR_DEBUG
    printBufferState();
#endif 
  }

}




/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittests -s <suite_name> -h help\n";
  std::cout << "Available Suites: " <<
      "clockTests" << std::endl;
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
