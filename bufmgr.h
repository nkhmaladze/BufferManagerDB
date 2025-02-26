/**
 * @file bufmgr.h
 * @author Nikoloz Khmaladze & Omar Ebied
 * @date 2025-02-09
 * 
 * Defines the interface for the BufferManager class.
 * This class manages the buffer pool, which is an in-memory cache of pages 
 * from disk.  The BufferManager handles page allocation, deallocation, 
 * retrieval, and replacement, providing an abstraction layer between the 
 * higher-level database operations and the DiskManager.  It utilizes a 
 * ReplacementPolicy to determine which pages to evict when the buffer pool 
 * is full.  This file declares the public interface of the BufferManager, 
 * including methods for interacting with pages and files.
 */

#ifndef _SWATDB_BUFMGER_H_
#define  _SWATDB_BUFMGER_H_

/**
 * \file bufmgr.h: BufferManager class: defines Buffer Manager interface
 */


#include <utility>
#include <unordered_map>
#include <mutex>
#include <list>
#include <queue>

#include "swatdb_types.h"
#include "page.h"           // need for alignment of Page object
#include "bm_buffermap.h"   // BufferMap class
#include "bm_frame.h"       // Frame class
                            


// can't just include bm_replacement.h or circular dependenices with .h file
// (one shortcomming of #pragma once)
class ReplacementPolicy;
class DiskManager;


/**
 * THIS STRUCT IS FOR DEBUGGING ONLY.
 * Struct that represents the state of the buffer pool.
 */
struct BufferState {

  /**
   * The total number of pages in the buffer pool.
   */
  std::uint32_t total;

  /**
   * The number of valid pages in the buffer pool.
   */
  std::uint32_t valid;

  /**
   * The number of pinned pages in the buffer pool.
   */
  std::uint32_t pinned;

  /**
   * The number of unpinned pages in the buffer pool.
   */
  std::uint32_t unpinned;

  /**
   * The number of dirty pages in the buffer pool.
   */
  std::uint32_t dirty;

  struct ReplacementStats {

    /**
     * Which Replacement Policy has been called
     */
    RepType rep_type;
    /**
     * The number of times the replacement policy has been called.
     */
    std::uint64_t rep_calls;
   
    /**
     * The number of times getPage or allocatePage have been called.
     */
    std::uint64_t new_page_calls;

    /**
     * Running average of the number of frames checked in replacement policy
     * before a frame eligible for replacement is found.
     */
    double avg_frames_checked;

    /**
     * The number of pages that have ref_bit set in the buffer pool.
     */
    std::uint32_t ref_bit;

    /**
     * The current position of the clock hand.
     */
    std::uint32_t clock_hand;
  } replace_stats;

};


/**
 * SwatDb BufferManager Class.
 * BufferManager manages in memory space of DBMS at page level granularity.
 * At higher level, pages of data could be allocated, deallocated, retrieved
 * to memory and fliushed to disk, using various methods.
 */
class BufferManager {

  public:

    /**
     * @brief BufferManager constructor. Initializes the buf_pool and
     *    frame_table, and stores a pointer to SwatDB's DiskManager.
     *
     * @pre disk_mgr points to an initialized DiskManager object.
     * @post A BufferManager object will be initialized with an empty
     *    buffer pool. disk_mgr is set to the given DiskManager* and
     *    clock_hand is set to 0.
     *
     * @param disk_mgr A pointer to SwatDB's DiskManager object.
     *    (DiskManager*).
     *
     * @throw InvalidPolicyBufMgr If the replacement policy type is invalid.
     */
    BufferManager(DiskManager *disk_mgr, RepType rep_type);

    /**
     * @brief BufferManager destructor.
     *
     * @pre None.
     * @post Every valid and dirty Page in buffer pool is written to disk.
     */
    ~BufferManager();

    /**
     * @brief Allocates a Page for the file of given FileId. The Page is
     *    allocated both in the buffer pool, and on disk.
     *
     * @pre A valid FileId is provided and there is a free Page on disk or
     *      there is enough space in Unix file. There is also free space in the
     *      buffer pool, or a Page which can be evicted from the buffer pool.
     * @post A Frame is allocated, and a corresponding Page in the buffer pool
     *       is allocated. The Frame's page_id is set, valid is set to true,
     *       and the pin_count is set to 1. The PageId is added to the
     *       BufferMap.  Finally, a pair of a pointer to the allocated Page and
     *       PageI       d is returned.
     *
     * @param file_id A FileId to which a Page should be allocated.
     * @return std::pair of Page* and PageId of the allocated Page.
     *
     * @throw InsufficientSpaceBufMgr If there is not enough space in buffer
     *        pool.
     * @throw InvalidFileIdDiskMgr If file_id not valid.
     * @throw InsufficientSpaceDiskMgr If there is not enough space in the
     *        Unix file.
     */
    std::pair<Page*,PageId> allocatePage(FileId file_id);

    /**
     * @brief Removes the Page of the given PageId from the buffer pool,
     *    and deallocates the Page from the appopriate file on disk.
     *
     * @pre A valid PageId of an unpinned Page is provided as a parameter.
     * @post  If the Page is in the buffer pool, the Frame is reset, and the
     *        Page is removed from the buffer pool. The Page is deallocated
     *        from disk.
     *
     * @param pageId PageId of the Page to be deallocated
     *
     * @throw PagePinnedBufMgr If the Page is pinned.
     * @throw InvalidPageNumDiskMg If page_id.page_num is invalid
     *        (from DiskManager layer)
     * @throw InvalidFileIdDiskMgr If page_id.file_id is invalid
     *        (from DiskManager layer)
     */
    void deallocatePage(PageId page_id);


    /**
     * @brief Gets Page by page_id, pins the Page, and returns a pointer
     *        to the Page object.
     *
     * @pre A PageId of an allocated Page is provided as a parameter and buffer
     *      pool is not full of pinned pages.
     * @post If the page_id is in buf_map, the Page is pinned and its pointer
     *       is returned. Else, a Frame is allocated in the buffer pool
     *       according to the page replacement policy, the Page is read from
     *       disk_mgr into the buffer pool, and the page_id, pin count, and
     *       valid bit are set.  pin_count is incremented by a successful
     *       getPage.  Page* is returned.
     *
     * @param page_id A PageId corresponding to the pointer to be returned.
     * @return Pointer to the Page with page_id.
     *
     * @throw InvalidPageIdBufMgr If page_id is not valid.
     * @throw InsufficientSpaceBufMgr If buffer pool is full.
     *
     * @throw InvalidFileIdDiskMgr from DiskManager if page_id.file_id invalid
     * @throw InvalidPageNumDiskMgr from DiskManger if page_id.page_num invalid
     * @throw DiskErrorDiskMgr frome DiskManager if file operation fails.
     */
    Page* getPage(PageId page_id);

    /**
     * @brief Unpins a Page in the buffer pool.
     *
     * @pre A PageId of a pinned Page is provided as input. The Page is in the
     *      buffer pool and is pinned by the executing thread/process.
     * @post The pin count of the Page is decremented. ref_bit is set to true.
     *       Dirty bit is set if the dirty parameter is true.
     *
     * @param page_id PageId of the Page to be released.
     *
     * @throw PageNotPinnedBufMgr If Page is not pinned. (pin_count is 0).
     * @throw PageNotFoundBufMgr If page_id is not in buf_map.
     */
    void releasePage(PageId page_id, bool dirty);

    /**
     * @brief Set the Page of the given PageId dirty.
     *
     * @pre A PageId of a pinned Page is provided as input.
     * @post The Page is set dirty.
     *
     * @param page_id PageId of the Page to set dirty.
     *
     * @throw PageNotFoundBufMgr If page_id is not in the buffer pool.
     */
    void setDirty(PageId page_id);

    /**
     * @brief Flushes the Page of the given PageId to disk.
     *
     * @pre A PageId of a pinned Page is provided as input.
     * @post If the Page is set dirty, the Page is written to disk through
     *    the disk_mgr. Page is still pinned.
     *
     * @param page_id PageId of the Page to set dirty.
     *
     * @throw PageNotFoundBufMgr If page_id not in buf_map.
     * @throw InvalidFileIdDiskMgr If page_id.file_id not valid.
     * @throw InvalidPageNumDiskMgr If page_id.page_num not valid.
     */
    void flushPage(PageId page_id);

    /**
     * @brief Calls createFile() method on the DiskManager to create new Unix
     *        file that corresponds to the given FileId.
     *
     * @pre FileId is valid.
     * @post Unix file that corresponds to the file_id is created.
     *
     * @param file_id FileId of the file to be created.
     *
     * @see DiskManager::createFile()
     */
    void createFile(FileId file_id);

    /**
     * @brief Calls removeFile() method on the DiskManager. Checks that none
     *        of the file's pages are pinned in the buffer pool. Removes any of
     *        the file's pages from the buffer pool before removing from disk.
     *
     * @pre A valid FileId is given as a parameter. None of the file's pages
     *      are pinned in the buffer pool.
     * @post If the file has pages in the buffer pool, the corresponding frames
     *       are reset and pages are removed from buf_map. The file is removed
     *       from disk via DiskManager->removeFile().
     *
     * @param file_id FileId of the file to be removed.
     *
     * @throw PagePinnedBufMgr If the are pinned pages of file_id.
     *
     * @see DiskManager::removeFile()
     */
    void removeFile(FileId file_id);


    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Returns the current state of the buffer pool.
     * @see BufferState
     */
    BufferState getBufferState();

   /**
    * @brief Return the amount of unpinned pages in the buffer pool
    */
    std::uint32_t getNumUnpinned();

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints Frame state of every Frame in the buffer pool, including
     *        pin count, valid bit, dirty bit, and ref_bit. If Page is valid,
     *        PageId is printed.
     */
    void printAllFrames();

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints Frame state of every valid Frame in the buffer pool,
     *        including PageId, pin count, valid bit, dirty bit, and ref_bit.
     */
    void printValidFrames();

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints Frame state of given FrameId, including pin count, valid
     *        bit, dirty bit, and ref_bit. If Page is valid, PageId is printed.
     */
    void printFrame(FrameId frame_id);

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints Frame state of given PageId, including FrameId, pin count,
     *        valid bit, dirty bit, and ref_bit. If Page is not in the buffer
     *        map, prints "Page Not Found".
     */
    void printPage(PageId page_id);

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints current buffer state, including total number of pages,
     *        number of valid pages, number of pinned pages, number of dirty
     *        pages, number of pages whose ref bit is set and the current clock
     *        hand position.
     */
    void printBufferState();


    /**
     * @brief This method is for performance tests.
     *        Prints number of calls to replacment policy, average check on
     *        replacement calls, lru/mru queue/stack usage.
     */
    void printReplacementStats();

  private:
    /**
     * A wrapper for std::unordered_map<PageId, FrameId> that maps PageIds to
     * Frame indices in buf_pool. Has different methods from
     * std::unordered_map.  Have get(), contains(), insert(), and remove()
     * methods.
     */
    BufferMap buf_map;

    /**
     * Array of Frame objects. Frames store metadata about each Page in the
     * buffer pool.
     */
    Frame frame_table[BUF_SIZE];

    /**
     * Array of Page objects. Represents the buffer pool.
     */
    Page buf_pool[BUF_SIZE];

    /**
     * Pointer to SwatDB's DiskManager. Used for reading, writing, allocating,
     * and deallocating pages to disk.
     */
    DiskManager* disk_mgr;

    /**
     * Pointer to SwatDB's Replacement Policy. Used for determining which
     * frame to remove.
     */
    ReplacementPolicy *replacement_pol;

      /**
     * @brief Allocates a free Frame using the current replacement policy.
     *
     * @pre None.
     * @post A FrameId of an available Frame is returned. If the Frame was
     *       previously valid (containing a Page), the corresponding entry in
     *       buf_map is removed. The Frame's valid, dirty, and pin_count fields
     *       are reset.
     *
     * @return FrameId of the allocated Frame.
     */
    FrameId _allocateFrame();


    /**
     * @brief this is a helper method to print one frame
     *        Prints frame state of given FrameId, including pin count, valid
     *        bit, dirty bit, and ref_bit. If Page is valid, PageId is printed.
     */
    void _printFrameHelper(FrameId frame_id);


};

#endif
