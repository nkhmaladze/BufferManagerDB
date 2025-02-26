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
SUITE(allocatePage){

  /*
   * First the test create a file in the diskmanager. Then the exception cases
   * for allocatePage() are tested, i.e.
   * 1) Checks InvalidFileIdDiskMgr by attempting to allocatePage
   *    with a FileId that doesn't exist in the DiskManager.
   * 2) Checks InsufficientSpaceBufMgr by first allocating
   *    BUF_SIZE pages to the buf_pool, and then another page, which would
   *    exceed the buffer pool capacity.
   * Finally, the test checks that the size of the file to which the BUF_SIZE
   * pages were allocated is equal to BUF_SIZE.
   */
  TEST_FIXTURE(TestFixture,allocatePage){

    //invalid file_id is given
    PRINT("TEST: allocatePage execptions tests\n");
    CHECK_THROW(this->buf_mgr->allocatePage(file_id+1),
        InvalidFileIdDiskMgr);
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      this->buf_mgr->allocatePage(file_id);
    }

    //check inmemory size before the exception
    CHECK_EQUAL(BUF_SIZE,disk_mgr->getSize(file_id));
    //buffer pool size is exceeded
    CHECK_THROW(this->buf_mgr->allocatePage(file_id),
        InsufficientSpaceBufMgr);
    //check inmemory size after exception
    CHECK_EQUAL(BUF_SIZE,disk_mgr->getSize(file_id));
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);

#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }
}

/*
 * Tests deallocatePage method.
 */
SUITE(deallocatePage){

  /*
   * The test begins by creating a single file in the catalog, and allocating a
   * page to it. Then, each exception case is checked, i.e.  1) Checks
   * InvalidFileIdDiskMgr case by calling deallocatePage() on a PageId where
   * the FileId field is invalid.  2) Checks InvalidPageNumDiskMgr case by
   * calling deallocatePage() on a PageId where the PageNum is invalid.  3)
   * Checks the PagePinnedBufMgr case by calling deallocatePage() on a valid
   * PageId that currently has a pin_count of 1 in the buffer pool.  Finally,
   * the test releases the page, and properly deallocates it. The final check
   * is that the size of the file in the DiskManager is back to zero.
   */
  TEST_FIXTURE(TestFixture, deallocatePage){

    std::pair<Page*, PageId> page_pair = this->buf_mgr->allocatePage(file_id);

    PRINT("TEST: deallocatePage tests\n");
    //check if InvalidFileIdDiskMgr is thrown.
    CHECK_THROW(this->buf_mgr->deallocatePage({page_pair.second.file_id+1,
        page_pair.second.page_num}),SwatDBException);
    //check if InvalidPageNumDiskMgr is thrown.
    CHECK_THROW(this->buf_mgr->deallocatePage({page_pair.second.file_id,
        page_pair.second.page_num+1}),SwatDBException);
    //check if PagePinnedBufMgr is thrown.
    CHECK_THROW(this->buf_mgr->deallocatePage(page_pair.second),
        PagePinnedBufMgr);
    this->buf_mgr->releasePage(page_pair.second, false);
    this->buf_mgr->deallocatePage(page_pair.second);
    //check inmemory size
    CHECK_EQUAL(0,disk_mgr->getSize(file_id));
    //check current buffer state
    checkBufferState(0, 0, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }

}

/*
 * Test releasePage method.
 */
SUITE(releasePage){

  /*
   * Test releasePage function.
   * The test begins by creating a single file in the catalog. A single page is
   * allocated, and then released. Then, each exception case is checked, i.e.
   * 1) Checks PageNotPinnedBufMgr by attempting to release the
   *    page that was already unpined in the earlier releasePage() call.
   * 2) Checks PageNotPinnedBufMgr again, except this time after
   *    deallocating the page.
   */
  TEST_FIXTURE(TestFixture,releasePage){

    std::pair<Page*, PageId> page_pair = this->buf_mgr->allocatePage(file_id);

    PRINT("TEST: releasePage tests\n");
    //check if PageNotPinnedBufMgr is thrown.
    this->buf_mgr->releasePage(page_pair.second, false);
    CHECK_THROW(this->buf_mgr->releasePage(page_pair.second, false),
        PageNotPinnedBufMgr);
    this->buf_mgr->deallocatePage(page_pair.second);
    //check if PageNotFoundBufMgr is thrown.
    CHECK_THROW(this->buf_mgr->releasePage(page_pair.second, false),
        PageNotFoundBufMgr);
    //check current buffer state
    checkBufferState(0, 0, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }
}

/*
 * Tests setDirty and flushPage methods.
 */
SUITE(setDirtyAndFlushPage){

/*
 * First adds a file entry to the catalog, and create the file through the
 * BufferManager. Checks the exception case for setDirty by calling setDirty()
 * on a PageId not in the bufferpool (checks that
 * PageNotFoundBufMgr is thrown). Checks the same exception
 * case for flushPage(), by attempting to call it on a PageId not in the
 * bufferpool (checks that PageNotFoundBufferManaerException is thrown).
 * Then, the test fills the bufferpool by allocatingPage() BUF_SIZE times.
 * For each of the allocated pages...
 * 1) A unique array of chars is initialized.
 * 2) The page is set to dirty with setDirty().
 * 3) The page is flushed to disk with fushPage().
 * Finally, the test checks that each page's data is correct on disk by calling
 * readPage() on the diskmanager.
 */
  TEST_FIXTURE(TestFixture,setDirtyAndFlushPage){

    std::vector<std::pair<Page*,PageId>> allocated_pages;

    PRINT("TEST: set dirty and flush tests\n");
    CHECK_THROW(this->buf_mgr->setDirty({0,0}),
        PageNotFoundBufMgr);
    CHECK_THROW(this->buf_mgr->flushPage({0,0}),
        PageNotFoundBufMgr); //allocate BUF_SIZE number of pages
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      allocated_pages.push_back(this->buf_mgr->allocatePage(file_id));
    }
    //fill each page with an array of specific char,
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      memset(allocated_pages.at(i).first->getData(), i%128, PAGE_SIZE);
      // now add its page number to begining
      sprintf(allocated_pages.at(i).first->getData(),"%d ",
          allocated_pages.at(i).second.page_num);
      this->buf_mgr->setDirty(allocated_pages.at(i).second);
      this->buf_mgr->flushPage(allocated_pages.at(i).second);
    }
    Page* flushed_page = new Page();
    //check if pages are actually flushed to disk properly
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      disk_mgr->readPage(allocated_pages.at(i).second, flushed_page);
      std::uint32_t temp_pagenum;
      sscanf(flushed_page->getData(), "%d", &temp_pagenum);
      //printf(" %d %d\n", temp_pagenum,
      //    allocated_pages.at(i).second.page_num);
      CHECK(temp_pagenum == allocated_pages.at(i).second.page_num);
    }

    //check current buffer state
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);
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
SUITE(getPage){

  /*
   * First adds a file entry to the catalog, and creates the file through the
   * BufferManager. Allocates a page. Checks the exception case for getPage by
   * calling getPage() on inalid PageId (checks that InvalidPageIdBufMgr is
   * thrown). Calls getPage on the allocated page multiple times and check if
   * the Page* returned is the same.
   */
  TEST_FIXTURE(TestFixture,getPage1){
    std::pair<Page*, PageId> page_pair = this->buf_mgr->allocatePage(file_id);

    PRINT("TEST: getPage1: single page, check pin count, check exceptions\n");
    CHECK_THROW(this->buf_mgr->getPage({page_pair.second.file_id+1,
        page_pair.second.page_num}),InvalidPageIdBufMgr);
    //check if InvalidPageIdBufMgr is thrown.
    CHECK_THROW(this->buf_mgr->getPage({page_pair.second.file_id,
        page_pair.second.page_num+1}),InvalidPageIdBufMgr);
    //check if the returned Page pointer is the same as allocated one.
    for (std::uint32_t i = 0; i < BUF_SIZE; i ++){
      CHECK_EQUAL(page_pair.first, this->buf_mgr->getPage(page_pair.second));
    }
    //check current buffer state
    checkBufferState(1, 1, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }

  /*
   * Test getPage in more sophiscated way. First adds a file entry to the ,
   * catalog and create the file through the BufferManager. Allocates BUF_SIZE
   * pages and fill the buffer with the allocated pages. by calling getPage and
   * fill each Page with an array of specific char. Calls getPage on each page
   * again and check if the data written to each page is consistent.
   */
  TEST_FIXTURE(TestFixture,getPage2){
    std::vector<PageId> allocated_pages;

    Page *temp_page = nullptr;
    char *temp_data = nullptr;

    PRINT("TEST: getPage2: fill buffer pool, check data on pages\n");
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      allocated_pages.push_back(disk_mgr->allocatePage(file_id));
    }

    // fill each page with an array of specific char
    // then write to each page of half of the allocated pages with
    // its page num (this ensures a unique value for EVERY page)
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      temp_page = this->buf_mgr->getPage(allocated_pages.at(i));
      memset(temp_page->getData(), i%128, PAGE_SIZE);
      sprintf(temp_page->getData(),"%d ",allocated_pages[i].page_num);
    }
    //check if written data is consistent
    //note: this will pin each page again
    temp_data = new char[PAGE_SIZE];
    for(std::uint32_t i =0; i < BUF_SIZE; i++){
      memset(temp_data, i%128, PAGE_SIZE);
      sprintf(temp_data,"%d ",allocated_pages[i].page_num);
      temp_page = this->buf_mgr->getPage(allocated_pages.at(i));
      CHECK(!memcmp(temp_data,temp_page->getData(),PAGE_SIZE));
    }
    //check current buffer state
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif

    delete []temp_data;
  }

  /*
   * Tests getPage and releasePage. First adds a file entry to the catalog,
   * and create the file through the BufferManager. Allocates BUF_SIZE*2 pages
   * through the DiskManager and fill the buffer with half of the
   * allocated pages. by calling getPage and fill each Page with an array
   * of specific char.  Releases all pages currently in memory and calls
   * getPage on the first half of the allocated pages to check exception
   * is thrown. Releases all pages currently in memory and calls getPage
   * on the first half of the allocated pages again.
   * Checks if the data is still consistent
   */
  TEST_FIXTURE(TestFixture,getPage3){
    std::vector<PageId> allocated_pages;
    Page *temp_page = nullptr;
    char *temp_data = nullptr;

    PRINT("TEST: getPage3: getPage,releasePage, exceptions.  Fill 1/2 BP.\n");
    PRINT("      release, getPage on 1st half again in reverse order");
    //allocate twice BUF_SIZE pages
    for (std::uint32_t i =0; i < (BUF_SIZE*2); i++){
      allocated_pages.push_back(disk_mgr->allocatePage(file_id));
    }

    // fill each page of half of the allocated pages with an array of
    // specific char
    // make sure they are all unique by writing page number to start
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      temp_page = this->buf_mgr->getPage(allocated_pages.at(i));
      memset(temp_page->getData(), i%128, PAGE_SIZE);
      sprintf(temp_page->getData(),"%d ", allocated_pages.at(i).page_num);
    }
    // release every page in buffer pool and read latter half of the allocated
    // pages
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      this->buf_mgr->releasePage(allocated_pages.at(i), true);
      temp_page = this->buf_mgr->getPage(allocated_pages.at(i+BUF_SIZE));
      sprintf(temp_page->getData(),"%d ",
          allocated_pages.at(i+BUF_SIZE).page_num);
    }

    //check that the first half of the allocated pages are not in buffer pool
    //also check InsufficientSpaceBufMgr is thrown
    for(std::uint32_t i =0; i < BUF_SIZE; i++){
      CHECK_THROW(this->buf_mgr->getPage(allocated_pages.at(i)),
          InsufficientSpaceBufMgr);
    }

    //release every page in buffer pool and read first half of the allocated
    //pages again, but in reverse order. check that data is consistent
    temp_data = new char[PAGE_SIZE];
    for (std::uint32_t i =0; i < BUF_SIZE; i++){
      this->buf_mgr->releasePage(allocated_pages.at(2*BUF_SIZE-i-1), true);
      temp_page = this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE-i-1));
      std::uint32_t temp_pagenum;
      sscanf(temp_page->getData(), "%d", &temp_pagenum);
      CHECK(temp_pagenum == allocated_pages.at(BUF_SIZE-i-1).page_num);
    }

    //check current buffer state
    checkBufferState(BUF_SIZE, BUF_SIZE, 0);
#ifdef BMGR_DEBUG
    printBufferState();
#endif

    delete [] temp_data;
  }

}


/*
 * Tests removeFile
 */
SUITE(removeFile){

  /*
   * VERY simple removeFile() test.
   * create file, allocates five pages, releases five pages,
   * calls removeFile().
   * Does not test exception cases (see further removeFile tests)
   */
  TEST_FIXTURE(TestFixture, removeFile1){
    //create vector for page allocation
    std::vector<std::pair<Page*, PageId>> allocated_pages;

    PRINT("TEST: removeFile1: create, allocate 5 pgs, release pgs, remove\n");
    //allocate 5 pages
    for(int i=0; i<5; i++){
      allocated_pages.push_back(this->buf_mgr->allocatePage(file_id));
    }

    //release the pages
    for(int i=0; i<5; i++){
      this->buf_mgr->releasePage(allocated_pages.at(i).second, false);
    }
    //check current buffer state
    checkBufferState( 5, 0, 0);
    //remove file
    this->buf_mgr->removeFile(file_id);
#ifdef BMGR_DEBUG
    printBufferState();
#endif
  }


  /*
   * Tests BufferManager's removeFile() method, including exception cases.  The
   * test first adds two files to the catalog, file1 and file2, and then calls
   * the BufferManager's createFile() function on them. Then the test allocates
   * 5 pages for each file, alternating allocating a page for file1 and file2
   * (the reason for this is to ensure that the pages for a file don't make up
   * one contiguous block of memory). The test then checks the PagePinnedBufMgr
   * by attempting to remove file1 and file2, both of which have all of their
   * pages pinned. Following this, each allocated page is released. No error is
   * expected as file2 is removed with the BufferManager's removeFile() method.
   * Following this, the following exception cases are checked for each page
   * that belonged to file2, now removed:
   * 1) InvalidPageIdBufMgr is checked by calling getPage() on
   *    the removed page.
   * 2) SwatDBException is checked by calling deallocatePage() on the removed
   *    page.
   * 3) PageNotFoundBufMgr is checked by calling releasePage() on
   *    the removed page.
   * Following this, the test calls getPage() on the 4th allocated page to
   * file1.  The test does this so as to test exception cases for removed pages
   * which have been pinned multiple times (i.e. had their pin count go between
   * 0 and 1 more than once). The test checks that PagePinnedBufMgr is thrown
   * when the BufferManager calls removeFile(). Then the test releases the
   * page, removes file1, and performs the same checks for exception cases as
   * it did with file2's pageIds after it was removed.
   *
   * NOTE: BUF_SIZE must be at least 10
   */

  TEST_FIXTURE(TestFixture, removeFile2){
    //initialize DBMS objects, and add two relation entries in the catalog
    //create a second file
    std::string file_name2 = "testrel2.rel";
    FileId fid2 = catalog->addEntry(file_name2, nullptr, nullptr, nullptr, 
        HeapFileT, INVALID_FILE_ID, file_name2);

    PRINT("TEST: removeFile2: 2 files, checks pinning, checks exceptions\n");
    //call createFile for each of the catalog's entries
    this->buf_mgr->createFile(fid2);

    //keep track of allocated pages for each file in separate vectors
    std::vector<std::pair<Page*, PageId>> allocated_pages_1;
    std::vector<std::pair<Page*, PageId>> allocated_pages_2;

    //alternate allocating pages for each file
    for(int i=0; i<10; i++){
      if (i%2==0){
        allocated_pages_1.push_back(this->buf_mgr->allocatePage(file_id));
      }
      else{
        allocated_pages_2.push_back(this->buf_mgr->allocatePage(fid2));
      }
    }

    //check that the files with pinned pages can't be removed
    //this->buf_mgr->removeFile(file_id);
    CHECK_THROW(this->buf_mgr->removeFile(file_id),
        PagePinnedBufMgr);
    CHECK_THROW(this->buf_mgr->removeFile(fid2),
        PagePinnedBufMgr);

    //unpin all
    for(int i=0; i<10; i++){
      if(i<5){
        this->buf_mgr->releasePage(allocated_pages_1.at(i).second, false);
      }
      else{
        this->buf_mgr->releasePage(allocated_pages_2.at(i-5).second, false);
      }
    }
    //check that file2 can be removed
    this->buf_mgr->removeFile(fid2);
    //check that the pages no longer exist in the buffer pool
    //i.e. getPage, deallocatePage, and releasePage should all throw exceptions
    for(int i=0; i<5; i++){
      CHECK_THROW(this->buf_mgr->getPage(allocated_pages_2.at(i).second),
          InvalidPageIdBufMgr);
      CHECK_THROW(this->buf_mgr->deallocatePage(allocated_pages_2.at(i).second),
          SwatDBException);
      CHECK_THROW(this->buf_mgr->releasePage(allocated_pages_2.at(i).second,
            false),
          PageNotFoundBufMgr);
    }
    //re-pin a page from file1 by calling getPage() on one of the ids
    //it should still be in the buffer pool
    this->buf_mgr->getPage(allocated_pages_1.at(4).second);
    //check that the file can't be removed due to this pin
    CHECK_THROW(this->buf_mgr->removeFile(file_id),
        PagePinnedBufMgr);
    //finally release, and removeFile
    this->buf_mgr->releasePage(allocated_pages_1.at(4).second, false);
    this->buf_mgr->removeFile(file_id);
    //perform identical checks as with the removal of file2
    for(int i=0; i<5; i++){
      CHECK_THROW(this->buf_mgr->getPage(allocated_pages_1.at(i).second),
          InvalidPageIdBufMgr);
      CHECK_THROW(this->buf_mgr->deallocatePage(allocated_pages_1.at(i).second),
          SwatDBException);
      CHECK_THROW(this->buf_mgr->releasePage(allocated_pages_1.at(i).second,
            false),
          PageNotFoundBufMgr);
    }

#ifdef BMGR_DEBUG
    printBufferState();
#endif
    //clean up
    remove(file_name2.data());
  }

  /*
   * Checks that removing a file does not affect other pages in the buffer
   * pool, and that the pages from the removed file no longer take up space in
   * the buffer pool. (i.e., new pages can be allocated in their place).
   *
   *  - First, the test adds two file entries to the catalog, testfile1 and
   *    testrel2.rel, and calls BufferManager->createFile() on them. 
   *  - Then 5 pages are allocated to each file, alternating between 
   *    allocatingPage to testfile1 and testrel2.rel. 
   *  - Following this, each of testfile1's pages are released, and the 
   *    file is removed. The test then checks that the removal of this file 
   *    and its pages did not affect the still pinned pages of testrel2.rel. 
   *  - First, the test checks that testrel2.rel has capacity and
   *    size of 5 according to the DiskManager. 
   *  - Then, BUF_SIZE-5 more pages are allocated to testrel2.rel in the 
   *    buffer pool. We expect no exception here, because the buffer pool 
   *    should only contain the 5 pages allocated to testrel2.rel. 
   *  -  Finally, tests that one more allocatePage() causes the
   *     BufferManager to throw InsufficientSpaceBufMgr.
   *
   * NOTE: BUF_SIZE must be at least 10
   */
  TEST_FIXTURE(TestFixture, removeFile3){

    std::string file_name2 = "testrel2.rel";
    FileId fid2 = catalog->addEntry(file_name2, nullptr, nullptr, nullptr, 
        HeapFileT, INVALID_FILE_ID, file_name2);

    PRINT("TEST: removeFile3\n");
    this->buf_mgr->createFile(fid2);
    //keep track of allocated pages for each file in separate vectors
    std::vector<std::pair<Page*, PageId>> allocated_pages_1;
    std::vector<std::pair<Page*, PageId>> allocated_pages_2;

    //alternate allocating pages for each file
    //all pages here will be pinned.
    for(int i=0; i<10; i++){
      if (i%2==0){
        allocated_pages_1.push_back(this->buf_mgr->allocatePage(file_id));
      }
      else{
        allocated_pages_2.push_back(this->buf_mgr->allocatePage(fid2));
      }
    }

    //unpin all of file1's pages
    for(int i=0; i<5; i++){
      this->buf_mgr->releasePage(allocated_pages_1.at(i).second, false);
    }

    //remove testfile1
    this->buf_mgr->removeFile(file_id);

    //check that file2's disk data is unaffected
    CHECK_EQUAL(5, disk_mgr->getSize(fid2));
    CHECK_EQUAL(5, disk_mgr->getCapacity(fid2));

    //We should now only have 5 pinned pages in the buffer pool. The deallocated
    //pages should be replaced if we call allocatePage() enough times.
    //No insufficient space errors should be thrown here.
    for(int i=0; i<((int) BUF_SIZE)-5; i++){
      this->buf_mgr->allocatePage(fid2);
    }

    //at this point, we've reached max buffer capacity. Therefore a new allocate
    //should result in an InsufficientSpaceBufMgr thrown
    CHECK_THROW(this->buf_mgr->allocatePage(fid2),
        InsufficientSpaceBufMgr);

#ifdef BMGR_DEBUG
    printBufferState();
#endif
    //clean up
    remove(file_name2.data());
  }
}


/*
 * An empty SUITE for students to add additional tests. Students may add tests
 * to other existing SUITE as well.
 */
SUITE(studentTests){
  //TODO: add tests

}

/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittests -s <suite_name> -h help\n";
  std::cout << "Available Suites: " <<
      "allocatePage, deallocatePage, releasePage, setDirtyAndFlushPage,\n"<<
      "getPage, removeFile, studentTests" << std::endl;
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
