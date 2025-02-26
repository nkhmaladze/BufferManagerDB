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
 * Unit tests for BufferManager class.
 */

static int TOTAL_SCANS = 50;

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
       * created and set to nullptr. A file, named "testre.rel" is added through
       * the buf_mgr.
       */
      this->catalog = nullptr;
      this->disk_mgr = nullptr;
      this->buf_mgr = nullptr;
      this->file_name = "";
      // file_id = catalog->addEntry(file_name, nullptr, nullptr, HeapFileT,
      //     file_name);
      // buf_mgr->createFile(file_id);
    }

    /*
     * Clean up and deallocates all objects initilalized by the constructor.
     * The unix file is explicitly removed.
     */
    ~TestFixture(){
    }

    /**
     * Acts as pseudo constructor for creating individual policy buf managers
     * within functions
     */
    void initialize(RepType rep_type){
      this->catalog = new Catalog();
      this->disk_mgr = new DiskManager(this->catalog);
      this->buf_mgr = new BufferManager(this->disk_mgr, rep_type);
      this->file_name = "testrel1.rel";
      this->file_id = catalog->addEntry(this->file_name, nullptr, nullptr, 
          nullptr, HeapFileT, INVALID_FILE_ID, this->file_name);
      this->buf_mgr->createFile(file_id);

    }
    /**
     * Acts as pseudo destructor for destroying individual policy buf managers
     * within functions.
     * Clean up and deallocates all objects initilalized by the constructor.
     * The unix file is explicitly removed.
     */
    void terminate(){
      delete this->buf_mgr;
      delete this->disk_mgr;
      delete this->catalog;
      remove(this->file_name.data());
    }

    /*
     * Helper function for allocating BUF_SIZE pages and bringing them all
     * into the buffer pool. Gets all pages and releases them, while also
     * allocating some amount of extra pages that are not brought into the
     * buffer pool
     */
    std::vector<PageId> fillBufferPool(std::uint32_t extra){
      std::vector<PageId> allocated_pages;
      for (std::uint32_t i =0; i < BUF_SIZE+extra; i++){
        allocated_pages.push_back(disk_mgr->allocatePage(file_id));
        if(i < BUF_SIZE){
          this->buf_mgr->getPage(allocated_pages.at(i));
          this->buf_mgr->releasePage(allocated_pages.at(i), false);
        }
      }
      return allocated_pages;
    }

      /** START OF TEST FUNCTIONS */


    /*
     * Helper function for checking the buffer state of the buffer pool. Only
     * the number of valid pages, pinned pages, and dirty pages are checked as
     * other state variables are either constant or may change depending on the
     * implementation details
     */
    void sequentialScanTest(RepType rep_type){

      std::cout << std::endl << "Straight Sequential Scan Test: " << std::endl;
      this->initialize(rep_type);
      // point to each page in buf pool
      // ex. page_data.at(0) points to the first page in the buf pool
      // fills buffer pool and creates two extra pages that can be added
      std::vector<PageId> allocated_pages;
      // allocates one extra page
      allocated_pages = fillBufferPool(20);


      // i is keeping track of the relevant index of allocated_pages
      // scan_num = the amount of sequential scans of size BUF_SIZE + 1
      for(int scan_num = 1; scan_num < TOTAL_SCANS + 1; scan_num++){
        for(std::uint32_t i = 0; i < BUF_SIZE+20; i++){
          this->buf_mgr->getPage(allocated_pages.at(i));
          this->buf_mgr->releasePage(allocated_pages.at(i), false);
        }
      }
      this->buf_mgr->printReplacementStats();
      this->terminate();

    }

    /**
     * Pins every page in the buffer pool and then unpins all of them in
     * order. Then repins every other frame. This will stress MRU and LRU which
     * will have to search their stack/queue to remove these frames.
     */
    void repinTest(RepType rep_type){
      std::cout << "Repin Test: " << std::endl;
      this->initialize(rep_type);
      // fills buffer pool and creates BUF_SIZE - 1 extra pages that can 
      // be added
      std::vector<PageId> allocated_pages = this->fillBufferPool(0);

      // repin every other page, except for frame 0
      for (std::uint32_t i = 1; i < BUF_SIZE/2; i++){

        this->buf_mgr->getPage(allocated_pages.at(2*i));
      }
      this->buf_mgr->printReplacementStats();
      this->terminate();
    }

    /**
     * Imitates a random access method, for example a nonclustered index access
     * pattern. Seeds random function to time of day then access BUF_SIZE * 4
     * randomly determined frames out of BUF_SIZE * 2 options. Gets and
     * releases page immediately.
     */
    void independentRandomTest(RepType rep_type){
      this->initialize(rep_type);
      std::cout <<  "Independent Random Test: " << std::endl;
      std::vector<PageId> allocated_pages;
      //allocate BUF_SIZE extra pages, fill and release all pages in buf pool
      allocated_pages = this->fillBufferPool(BUF_SIZE/2);
      //get a random page out of (3 * BUF_SIZE) / 2 potential pages
      //do this BUF_SIZE * 4 times 
      std::srand(time(nullptr));
      std::uint32_t rand_num;
      for(std::uint32_t i = 0; i < BUF_SIZE * 4; i ++){
        rand_num = std::rand() % (BUF_SIZE * 3 / 2);
        this->buf_mgr->getPage(allocated_pages.at(rand_num));
        this->buf_mgr->releasePage(allocated_pages.at(rand_num), false);
      }
      this->buf_mgr->printReplacementStats();
      this->terminate();

    }

    /**
     * Pin and unpin all frames. Then, repin all frames, and unpin 10 randomly
     * selected frames. Then pin and unpin BUF_SIZE /2 new pages. All policies
     * will be called the same amount of times but can compare overhead usage
     */
     void pinnedTest(RepType rep_type){
       this->initialize(rep_type);
       std::cout <<  "Pinned Test: " <<  std::endl;
       std::vector<PageId> allocated_pages;
       //allocate BUF_SIZE extra pages, fill and release all pages in buf pool
       allocated_pages = this->fillBufferPool(BUF_SIZE/2);
       // repin all frames but 10
       for (std::uint32_t i =0; i < BUF_SIZE; i++){
         this->buf_mgr->getPage(allocated_pages.at(i));
       }


       //randomly unpin 10 pages, and store map of unpinned so as to not unpin
       //an already unpinned page
       std::srand(time(nullptr));
       std::uint32_t rand_num;
       rand_num = std::rand() % BUF_SIZE;
       std::unordered_map<std::uint32_t, bool> already_chosen;
       for (int i = 0; i < 10; i ++){
         if(already_chosen.find(rand_num) != already_chosen.end()){
           rand_num = std::rand() % BUF_SIZE;
           i --;
         }
         else{
           this->buf_mgr->releasePage(allocated_pages.at(rand_num), false);
           already_chosen.insert({rand_num, true});
           rand_num = std::rand() % BUF_SIZE;
         }
       }

       //get BUF_SIZE / 2 new pages in sequential order
       for(std::uint32_t i = BUF_SIZE; i < (3 * BUF_SIZE)/2; i++){
         this->buf_mgr->getPage(allocated_pages.at(i));
         this->buf_mgr->releasePage(allocated_pages.at(i), false);
       }
       this->buf_mgr->printReplacementStats();
       this->terminate();
     }

     /**
      * Pin and unpin all pages in the buffer pool. Sets random seed to time of
      * day. Gets BUF_SIZE / 2 pages and repeatedly accesses 3 pages, and the
      * rest are randomly selected. MRU is expected to perform poorly on this
      * test. Imitates getting leaf pages from a B+ tree, where the higher
      * branch nodes are called more often.
      */
    void hierarchicalTest(RepType rep_type){
      this->initialize(rep_type);

      std::cout <<  "Hierarchical Test: " <<  std::endl;
      std::vector<PageId> allocated_pages;
      //allocate BUF_SIZE extra pages, fill and release all pages in buf pool
      allocated_pages = this->fillBufferPool(BUF_SIZE/2);

      std::srand(time(nullptr));
      std::uint32_t rand_num;

      //gets leaf pages Buf_Size / 2 times
      for(int scan_num = 1; scan_num < TOTAL_SCANS*25; scan_num++){
        //gets root node every iteration
        this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE/2));
        this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE/2), false);
        //randomly choose one of two branch nodes
        rand_num = std::rand() % 2;
        if(rand_num){
          this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE/4));
          this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE/4), false);
        }
        else{
          this->buf_mgr->getPage(allocated_pages.at((3 *BUF_SIZE)/4));
          this->buf_mgr->releasePage(allocated_pages.at((3 *BUF_SIZE)/4),false);
        }
        //get a random "leaf" page
        rand_num = std::rand() %(BUF_SIZE / 2);
        this->buf_mgr->getPage(allocated_pages.at(BUF_SIZE + rand_num));
        this->buf_mgr->releasePage(allocated_pages.at(BUF_SIZE + rand_num),
            false);
      }

      this->buf_mgr->printReplacementStats();
      this->terminate();

    }

};



SUITE(clockTests){

  TEST_FIXTURE(TestFixture, clockSmallSequentialScan){
    std::cout << std::endl << "CLOCK SUITE TESTS: " << std::endl;
    // straight sequential scan is performed 5 times
    this->sequentialScanTest(ClockT);
  }
  TEST_FIXTURE(TestFixture, clockRepin){
    this->repinTest(ClockT);
  }
  TEST_FIXTURE(TestFixture, clockIndependentRandom){
    this->independentRandomTest(ClockT);
  }
  TEST_FIXTURE(TestFixture, clockPinned){
    this->pinnedTest(ClockT);
  }
  TEST_FIXTURE(TestFixture, clockHierarchical){
    this->hierarchicalTest(ClockT);
  }
}

SUITE(randomTests){

  TEST_FIXTURE(TestFixture, randomSmallSequentialScan){
    std::cout << std::endl << "RANDOM SUITE TESTS: " << std::endl;
    // straight sequential scan is performed 5 times
    this->sequentialScanTest(RandomT);
  }
  TEST_FIXTURE(TestFixture, randomRepin){
    this->repinTest(RandomT);
  }
  TEST_FIXTURE(TestFixture, randomIndependentRandom){
    this->independentRandomTest(RandomT);
  }
  TEST_FIXTURE(TestFixture, randomPinned){
    this->pinnedTest(RandomT);
  }
  TEST_FIXTURE(TestFixture, randomHierarchical){
    this->hierarchicalTest(RandomT);
  }

}

/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittests -s <suite_name> -h help\n";

  std::cout << "Available Suites: " << "clockTests, randomTests" << std::endl;

}

/*
 * The main program either run all tests or tests a specific SUITE, given its
 * name via command line argument option 's'. If no argument is given by
 * argument option 's', main runs all tests by default. If invalid argument is
 * given by option 's', 0 test is run
 */
int main(int argc, char** argv){

  const char* suite_name;
  int c;
  bool test_all = true; /* if true: run all Clock and Random tests */

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
