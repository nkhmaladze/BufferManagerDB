#include <string>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <exception>
#include <stdlib.h>

#include "swatdb_types.h"
#include "swatdb_exceptions.h"
#include "diskmgr.h"
#include "bufmgr.h"
#include "page.h"
#include "catalog.h"
#include "file.h"
#include "swatdb.h"
#include "filemgr.h"

/* some example test functions: */
void allocatePageTest();
void clockReplacementTest();
void printTutorial();

/* some helper functions: */
/* initDB returns one valid fileID in the system */
FileId initDB();
void cleanUp();
bool checkBufferState(std::uint32_t valid,
    std::uint32_t pinned, std::uint32_t dirty);

/* we are using global variables for the main swatdb objects
 * (this is one reasonable use of global variables)
 * These are static to limit their scope to this file (important!)
 */
// these are just for convenience: they can all be
// obtained through the main swatDB object:
static  DiskManager* disk_mgr = nullptr;
static  BufferManager* buf_mgr = nullptr;
static  Catalog *catalog = nullptr;

/***************************
 * test some buffer manager interface functionality
 */
int main(){

  // each test creates a fresh version of SwatDB...you could design
  // tests to take an already initialized swatDB object and start
  // their test from there, but it is often more complicated to do  this
  printTutorial();
  std::cout << "*** Passed printTutorial!" << std::endl;

  allocatePageTest();
  std::cout << "*** Passed allocatePageTest!" << std::endl;

  // a more complicated test for testing clock
  // replacemnt algorithm, you can comment it out to run
  /*
  */
  clockReplacementTest();
  std::cout << "*** Passed clockReplacementTest!" << std::endl;

  std::cout << "*** Passed all tests!" << std::endl;

  return 0;
}

/*************************************
 * Function which demostrates some BufferManager print/debugging methods
 */
void printTutorial(){

  FileId file_id;
  std::pair<Page*, PageId> new_page;

  // init swatDB: returns one valid fileId in the system
  file_id = initDB();

  std::cout
      << "--------\n"
      << "Here's what the BufferManager looks like right after initializing:"
      << std::endl;
  buf_mgr->printBufferState();

  /* uncomment when have allocatePage and releasePage implemented */
  std::cout << "--------\nNow let's allocate a page, and print state again:"
      << std::endl;
  new_page = buf_mgr->allocatePage(file_id);
  buf_mgr->printBufferState();

  std::cout << "--------\nYou can also print out the info of a single page:"
      << std::endl;
  buf_mgr->printPage(new_page.second);
  buf_mgr->releasePage(new_page.second,false);
  /* */


  std::cout << "--------\n"
      << "You can also use printFrame(),printAllFrames() and "
      << "printValidFrames() methods. \nGive them a try :)\n"
      << "--------"
      << std::endl;

  // note: this calls buffer manager removeFile
  cleanUp();
}

/*********************************
 * Tests allocatePage. First the test create a file in the diskmanager.
 * Then the exception cases for allocatePage() are tested, i.e.
 * 1) Checks InvalidFileIdDiskMgr by attempting to allocatePage
 *    with a FileId that doesn't exist in the DiskManager.
 * 2) Checks InsufficientSpaceBufMgr by first allocating
 *    BUF_SIZE pages to the buf_pool, and then another page, which would
 *    exceed the buffer pool capacity.
 * Finally, the test checks that the size of the file to which the BUF_SIZE
 * pages were allocated is equal to BUF_SIZE.
 */
void allocatePageTest() {

  FileId file_id;
  std::vector<FileId> rel_fids;

  // init swatDB: returns one valid fileId in the system
  file_id = initDB();

 // check that invalid fileID exception thrown
  try{
    rel_fids = catalog->getFileIds();
    // try to allocte a page in a non-existant file ID
    buf_mgr->allocatePage(rel_fids.size());
    cleanUp();  // clean-up DB state if throw exception
    throw std::runtime_error("Expected InvalidFileIdDiskMgr exception\n");
  }
  catch(InvalidFileIdDiskMgr& e){ /* expected behavior */ }

  // fill the buffer pool
  for (std::uint32_t i =0; i < BUF_SIZE; i++){
    buf_mgr->allocatePage(file_id);
  }

  // check size of the file on disk before the exception
  // (should match number of pages allocated)
  std::uint32_t temp = disk_mgr->getSize(file_id);
  if (temp != BUF_SIZE){
    cleanUp();
    throw std::runtime_error("Expected " + std::to_string(BUF_SIZE) +
        ", but got " + std::to_string(temp));
  }

  // check exception thrown when allocating pages beyond the
  // buffer pool size and all pages are pinned
  try{
    buf_mgr->allocatePage(file_id);
    cleanUp();
    throw std::runtime_error("Expected InsufficientSpaceBufMgr exception\n");
  } catch (SwatDBException &e) { /* expected behavior */  }

  // an example of debug print statement you may want to call
  // std::cout<<"Final Buffer State:\n ";
  // buf_mgr->printBufferState();

  // check buffer state
  if(!checkBufferState(BUF_SIZE, BUF_SIZE, 0)){
    cleanUp();
    throw std::runtime_error("Allocate page test failed.");
  }
  cleanUp();
}

/************************************
 * This is an example of a more complicated test that is testing
 * the correctness of the clock replacement algorithm (that the
 * pages are replaced in the correct order based on the clock alg)
 */
void clockReplacementTest() {

  std::vector<PageId> allocated_pages;
  std::vector<Page*> released_pages;
  Page* temp_page;
  FileId file_id;

  // init swatDB: returns one valid fileId in the system
  file_id = initDB();

  // try to allocte a page in a non-existant file ID
  // init the file on disk, by allocating BUF_SIZE+5 number of pages to it
  for (std::uint32_t i =0; i < BUF_SIZE+5; i++){
    allocated_pages.push_back(disk_mgr->allocatePage(file_id));
  }

  // Call getPage to fill the buffer pool with allocated pages from
  // this file.  Also add 5 these pages to the release list
  // (these will be the 5 possible canidates for replacement).
  for (std::uint32_t i =0; i < BUF_SIZE; i++){
    temp_page = buf_mgr->getPage(allocated_pages.at(i));
    // Buffer is divided by 6, but pages at either ends are excluded.
    if (i%(BUF_SIZE/6)==0 && i >0 && i < 6*(BUF_SIZE/6)){
      released_pages.push_back(temp_page);
    }
  }

  // release pages in the release list.
  for (std::uint32_t i =0; i < BUF_SIZE; i++){
    if (i%(BUF_SIZE/6)==0 && i >0 && i < 6*(BUF_SIZE/6)){
      buf_mgr->releasePage(allocated_pages.at(i), false);
    }
  }

  // check if pages are evicted in order as the clockhand sweeps
  // through the buffer pool
  for(std::uint32_t i = 0; i < 5; i++){
    // temp_page = buf_mgr->allocatePage(allocated_pages.at(BUF_SIZE+i));
    temp_page = buf_mgr->getPage(allocated_pages.at(BUF_SIZE+i));
    if(released_pages.at(i)!=temp_page){
      std::cout << "Pages not evicted in the proper order."
          << std::endl;
      std::cout << "Expected " << released_pages.at(i)
          << "but got " << temp_page << std::endl;
      std::cout<<"Final Buffer State:\n ";
      buf_mgr->printBufferState();
      //clean up DBMS objects
      cleanUp();
      throw std::runtime_error("Clock replacement test failed.");
    }
  }

  bool pass = checkBufferState(BUF_SIZE, BUF_SIZE, 0);
  if(!pass){
    cleanUp();
    throw std::runtime_error("Clock replacement test failed.");
  }
  cleanUp();
}

/*******************  Helper Functions for test code ***********/
/************************
 * Helper Function: initializes SwatDB with a couple Relation file
 * to use for testing (feel free to add more or fewer files for
 * testing, BUT CREATE AT LEAST ONE).  Initializes all global
 * variables corresponding to main layers of SwatDB.
 *
 * @returns one valid FileID in the system
 */
FileId initDB() {

  FileId file_id;

  try {
    catalog = new Catalog();
    disk_mgr = new DiskManager(catalog);
    buf_mgr = new BufferManager(disk_mgr, ClockT);
    // create a couple test relations/files: feel free to add more)
    // (make sure to delete the files in cleanUp if you add more)
    file_id = catalog->addEntry("Rel1", nullptr, nullptr,
        nullptr, HeapFileT, INVALID_FILE_ID, "testrel1.rel");
    buf_mgr->createFile(file_id);

    file_id = catalog->addEntry("Rel2", nullptr, nullptr,
        nullptr, HeapFileT, INVALID_FILE_ID, "testrel2.rel");
    buf_mgr->createFile(file_id);

  } catch(SwatDBException& e){
    cleanUp();
    throw std::runtime_error("Something is messed up with initDB\n");
  }
  return file_id;
}

/***********************
 * Helper Function: clean-up's up all swatdb state
 * all relation files on disk should be removed after this call
 */
void cleanUp() {


  delete buf_mgr;
  buf_mgr = nullptr;
  delete disk_mgr;
  disk_mgr = nullptr;
  delete catalog;
  catalog = nullptr;

  remove("testrel1.rel");
  remove("testrel2.rel");

  // system("ls -l testrel*.db");
}
/***********************
 * Helper function for checking the buffer state of the buffer pool. Only the
 * number of valid pages, pinned pages, and dirty pages are checked as other
 * state variables are either constant or may change depending on the
 * implementation details
 *
 * returns true if the given values match given buf_mgr state,
 * else returns false
 */
bool checkBufferState(std::uint32_t valid,
    std::uint32_t pinned, std::uint32_t dirty){

  BufferState cur_buf = buf_mgr->getBufferState();
  bool pass = true;
  if(valid!=cur_buf.valid){
    std::cout<<"Expected valid bit " << cur_buf.valid
        << " but got " << valid << std::endl;
    pass = false;
  }
  if(pinned!=cur_buf.pinned){
    std::cout<<"Expected 'pinned' to be " << cur_buf.pinned
        << " but got " << pinned << std::endl;
    pass = false;
  }
  if(dirty!=cur_buf.dirty){
    std::cout<<"Expected dirty bit " << cur_buf.dirty
        << " but got " << dirty << std::endl;
    pass = false;
  }
  if(!pass){
    std::cout<<"Final Buffer State:\n ";
    buf_mgr->printBufferState();
  }
  return pass;
}
