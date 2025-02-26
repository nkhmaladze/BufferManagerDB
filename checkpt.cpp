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
      this->buf_mgr = new BufferManager(disk_mgr, rep_pol);
      file_name = "testrel1.rel";
      file_id = catalog->addEntry(file_name, nullptr, nullptr, nullptr, 
          HeapFileT, INVALID_FILE_ID, file_name);
      this->buf_mgr->createFile(file_id);
    }

    /*
     * Clean up and deallocates all objects initilalized by the constructor. The
     * unix file is explicitly removed.
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
};



/*
 * Tests allocatePage method.
 */
SUITE(allocatePageCkPt){

  /*
   * First the test allocates 3 pages in the buffer pool and checks state.
   */
  TEST_FIXTURE(TestFixture,allocatePage) {

    //invalid file_id is given
    PRINT("TEST: allocatePage: allocate 3 pages\n");
    std::pair<Page*, PageId> page_pair;
    page_pair = this->buf_mgr->allocatePage(file_id);
    page_pair = this->buf_mgr->allocatePage(file_id);
    page_pair = this->buf_mgr->allocatePage(file_id);

    // check size of file on disk (should have 3 pages allocated
    CHECK_EQUAL(3, this->disk_mgr->getSize(file_id));

    // check buffer pool states: should have 3 valid and pinned pages
    checkBufferState(3, 3, 0);

#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }
}

/*
 * Tests releasePage method.
 */
SUITE(releasePageCkPt){

  /*
   * test basic releasePage functionality: 
   *    allocate and release two pages (one with dirty bit set), 
   *    check BP state as go
   */
  TEST_FIXTURE(TestFixture, releasePage){

    std::pair<Page*, PageId> page_pair, page_pair2;

    PRINT("TEST: releasePage test\n");
    page_pair = this->buf_mgr->allocatePage(file_id);
    // one valid pinned page in the Buffer Pool
    checkBufferState(1, 1, 0);
    // release the page (undirty)
    this->buf_mgr->releasePage(page_pair.second, false);
    // one valid unpinned page
    checkBufferState(1, 0, 0);
    // allocate another page in the Buffer Pool
    page_pair2 = this->buf_mgr->allocatePage(file_id);
    // two valid, one pinned one unpinned
    checkBufferState(2, 1, 0);
    // release the page (dirty)
    this->buf_mgr->releasePage(page_pair2.second, true);
    // two valid, neither pinned, one dirty
    checkBufferState(2, 0, 1);
    
#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }

}


/*
 * Tests setDirty and flushPage methods.
 */
SUITE(setDirtyAndFlushPageCkPt){

/*
 * allocate a page, write to it, setDirty, flush, read from
 * disk to test that flushPage was successful
 */
  TEST_FIXTURE(TestFixture,setDirtyAndFlushPage){

    std::pair<Page*, PageId> page_pair;

    PRINT("TEST: set dirty and flush test\n");
    page_pair = this->buf_mgr->allocatePage(file_id);
    checkBufferState(1, 1, 0);

    // write 7 to every byte of the page
    memset(page_pair.first->getData(), 7, PAGE_SIZE);

    // set the dirty bit
    this->buf_mgr->setDirty(page_pair.second);
    checkBufferState(1, 1, 1);

    // flush page (should write to disk and clear the dirty bit
    this->buf_mgr->flushPage(page_pair.second);
    checkBufferState(1, 1, 0);

    Page* flushed_page = new Page();
    //check if page actually flushed to disk properly
    this->disk_mgr->readPage(page_pair.second, flushed_page);
    CHECK((flushed_page->getData())[0] == 7);

#ifdef BMGR_DEBUG
    printBufferState();
#endif

    // clean-up
    delete flushed_page;
  }
}

/*
 * Tests getPage method.
 */
SUITE(getPageCkPt){

  /*
   * Test some getPage functionality: This just tests the pincount > 1 Can't
   * yet test getPage read from disk until we trigger replacement and kick a
   * page out of the buffer pool
   */
  TEST_FIXTURE(TestFixture,getPage){

    std::pair<Page*, PageId> page_pair;

    PRINT("TEST: getPage: pin multiple times and check pin count\n");

    // allocating 3 pages but we are only manipulating one so
    // we are not keeping all pair return values in separate
    // variables
    page_pair = this->buf_mgr->allocatePage(file_id);
    page_pair = this->buf_mgr->allocatePage(file_id);
    page_pair = this->buf_mgr->allocatePage(file_id);
    checkBufferState(3, 3, 0);

    //check if the returned Page pointer is the same as allocated one.
    for (std::uint32_t i = 0; i < 3; i++){
      CHECK_EQUAL(page_pair.first, this->buf_mgr->getPage(page_pair.second));
    }
    //check current buffer state
    checkBufferState(3, 3, 0);

    // release first page a few time
    this->buf_mgr->releasePage(page_pair.second, false);
    this->buf_mgr->releasePage(page_pair.second, false);
    checkBufferState(3, 3, 0);
    this->buf_mgr->releasePage(page_pair.second, false);
    checkBufferState(3, 3, 0);
    buf_mgr->releasePage(page_pair.second, true);
    checkBufferState(3, 2, 1);

#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }
}

// a separate test fixture for the bufMapCkPt suite
class TestFixtureBufMap{

  public:

    PageId page_id;
    FileId file_id;
    PageNum page_num;
    FrameId frame_id;
    BufferMap *bufmap;
    bool found;

    TestFixtureBufMap(){
      bufmap = new BufferMap();
      page_id = {4,0};  // made-up file_id 4, page_num 0
      file_id = 0;
      page_num = 0;
      frame_id = 0;
      found = false;
    }

    ~TestFixtureBufMap(){
      delete bufmap;
    }
};

/*
 * Tests bufferMap independently of Buffer Manager class.
 */
SUITE(bufMapCkPt) {

  TEST_FIXTURE(TestFixtureBufMap,bufMapCkPt){

    // we are not using a testfixture type wrapper here
    // just for simplicity

    PRINT("TEST: bufMap: test basic BufferMap functionality\n");
    // test insert some elements in the BufferMap
    this->frame_id = 0;
    for(PageNum i = 0; i < 3; i++) {
      this->page_id.page_num = i;
      this->bufmap->insert(page_id, frame_id);
      this->frame_id++;
    }

    // let's see if contains finds them
    for(PageNum i = 0; i < 3; i++) {
      this->page_id.page_num = i;
      this->found = this->bufmap->contains(page_id);
      CHECK_EQUAL(this->found, true);
    }

    // let's get the FrameId of the pages we inserted
    for(PageNum i = 0; i < 3; i++) {
      this->page_id.page_num = i;
      this->frame_id = this->bufmap->get(page_id);
      CHECK_EQUAL(this->frame_id, i);
    }

    // lets see if it throws an exception on a duplicate insert
    CHECK_THROW(bufmap->insert(this->page_id, this->frame_id), 
        PageAlreadyLoadedBufMgr);

    // lets see if it correctly inserts page with same page_num but
    // a different file_id:
    this->page_id.file_id = 6;
    this->frame_id++;
    this->bufmap->insert(this->page_id, this->frame_id);

    // let's remove page 1 of the orginal file:
    this->page_id.page_num = 1;
    this->page_id.file_id = 4;
    this->bufmap->remove(page_id);

    // let's see if remove throws an exception if page not in bufmap
    CHECK_THROW(this->bufmap->remove(this->page_id), PageNotFoundBufMgr);

    // let's see if get throws an exception if the page is not in bufmap
    CHECK_THROW(this->bufmap->get(this->page_id), PageNotFoundBufMgr);
  }
}

/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittests -s <suite_name> -h help\n";
  std::cout << "Available Suites: " <<
      "allocatePageCkPt, releasePageCkPt, setDirtyAndFlushPageCkPt,\n"<<
      "getPageCkPt, bufMapCkPt" << std::endl;
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
