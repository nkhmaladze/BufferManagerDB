/**
 * @file bufmgr.cpp
 * @author Nikoloz Khmaladze & Omar Ebied
 * @date 2025-02-09
 * 
 * Implementation of the BufferManager class.
 * This file contains the implementation of the BufferManager, which manages
 * the buffer pool (an in-memory cache of disk pages).  It handles page
 * allocation, deallocation, retrieval, and replacement, interacting with
 * the DiskManager for disk I/O and using a ReplacementPolicy to determine
 * which pages to evict.  This file defines the methods declared in bufmgr.h.
 */

#include "bufmgr.h"
#include "bm_frame.h"
#include "bm_buffermap.h"
#include "bm_policies.h"
#include "bm_replacement.h"
#include "swatdb_exceptions.h"
#include "diskmgr.h"
#include "file.h"
#include "page.h"
#include "catalog.h"
#include "math.h"


/**
 * SwatDb BufferManager Class.
 * BufferManager manages in memory space of DBMS at page level granularity.
 * At higher level, pages of data could be allocated, deallocated, retrieved
 * to memory and fliushed to disk, using various methods.
 */

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
BufferManager::BufferManager(DiskManager *disk_mgr, RepType rep_type){
  
  this->disk_mgr = disk_mgr;
  switch(rep_type) {
    case ClockT:{
      this->replacement_pol = new Clock(this->frame_table);
      break; }
    case RandomT: {
      this->replacement_pol = new Random(this->frame_table);
      break; }


    default:
      throw InvalidPolicyBufMgr(); // no replacement policy defined
  }
}

/**
 * @brief BufferManager destructor.
 *
 * @pre None.
 * @post Every valid and dirty Page in buffer pool is written to disk.
 */
BufferManager::~BufferManager(){
  for (FrameId i = 0; i < BUF_SIZE; ++i) {
    if (frame_table[i].valid && frame_table[i].dirty) {
      disk_mgr->writePage(frame_table[i].page_id, &buf_pool[i]);
      frame_table[i].dirty = false;
    }
  }
  delete replacement_pol;  // Don't forget to delete the replacement policy!
}

  /**
 * @brief Allocates a Page for the file of given FileId. The Page is
 *    allocated both in the buffer pool, and on disk.
 *
 * @pre A valid FileId is provided and there is a free Page on disk or there
 *    is enough space in Unix file. There is also free space in the buffer
 *    pool, or a Page which can be evicted from the buffer pool.
 * @post A Frame is allocated, and a corresponding Page in the buffer pool
 *    is allocated. The Frame's page_id is set, valid is set to true, and
 *    the pin_count is set to 1. The PageId is added to the BufferMap.
 *    Finally, a pair of a pointer to the allocated Page and PageId
 *    is returned.
 *
 * @param file_id A FileId to which a Page should be allocated.
 * @return std::pair of Page* and PageId of the allocated Page.
 *
 * @throw InvalidFileIdDiskMgr If file_id not valid.
 * @throw InsufficientSpaceBufMgr If there is not enough space in buffer
 *    pool.
 * @throw InsufficientSpaceDiskMgr If there is not enough space in the
 *    Unix file.
 */
std::pair<Page*, PageId> BufferManager::allocatePage(FileId file_id){
  BufferState state = getBufferState();
  if(state.unpinned == 0){
    throw InsufficientSpaceBufMgr();
  }

  PageId page_id = disk_mgr->allocatePage(file_id); 
  FrameId frame_id = _allocateFrame();
  
  Frame &frame = frame_table[frame_id];
  frame.page_id = page_id;
  frame.valid = true;
  frame.pin_count = 1;
  frame.dirty = false;

  buf_map.insert( page_id, frame_id );
  replacement_pol->pin( frame_id );

  Page *ptr = &(buf_pool[frame_id]);

  return std::pair<Page*, PageId>(ptr, page_id);
}


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
FrameId BufferManager:: _allocateFrame(){
  FrameId frame_id = replacement_pol->replace();

  Frame &tmp = frame_table[frame_id];

  if( tmp.valid ){
    buf_map.remove( tmp.page_id );
  }

  tmp.valid = false;
  tmp.dirty = false;
  tmp.pin_count = 0;

  return frame_id;
}


/**
 * @brief Removes the Page of the given PageId from the buffer pool,
 *    and deallocates the Page from the appopriate file on disk.
 *
 * @pre A valid PageId of an unpinned Page is provided as a parameter.
 * @post  If the Page is in the buffer pool, the Frame is reset, and the
 *    Page is removed from the buffer pool. The Page is deallocated from
 *    disk.
 *
 * @param pageId PageId of the Page to be deallocated
 *
 * @throw PagePinnedBufMgr If the Page is pinned.
 * @throw InvalidPageNumDiskMgr	If page_id.page_num is invalid
 *        (from DiskManager layer)
 * @throw InvalidFileIdDiskMgr if page_id.file_id is invalid
 *        (from DiskManager layer)
 */
void BufferManager::deallocatePage(PageId page_id){
  
  if( buf_map.contains( page_id ) ){
    FrameId frame_id = buf_map.get( page_id );
    Frame &frame = frame_table[frame_id];
    
    if (frame.pin_count > 0) {
        throw PagePinnedBufMgr(page_id);
    }
    
    frame.valid = false;
    frame.dirty = false;
    frame.pin_count = 0;
    buf_map.remove(page_id);
    replacement_pol->freeFrame(frame_id);
  }
  
  disk_mgr->deallocatePage(page_id);
 }


/**
 * @brief Gets Page by page_id, pins the Page, and returns a pointer
 *    to the Page object.
 *
 * @pre A PageId of an allocated Page is provided as a parameter and buffer
 *    pool is not full of pinned pages.
 * @post If the page_id is in buf_map, the Page is pinned and its pointer is
 *    returned. Else, a Frame is allocated in the buffer pool according
 *    to the page replacement policy, the Page is read from disk_mgr into
 *    the buffer pool, and the page_id, pin count, and valid bit are set.
 *    pin_count is incremented by a successful getPage. 
 *    Page* is returned.
 *
 * @param page_id A PageId corresponding to the pointer to be returned.
 * @return Pointer to the Page with page_id.
 *
 * @throw InvalidPageIdBufMgr If page_id is not valid.
 * @throw InsufficientSpaceBufMgr If buffer pool is full.
 *
 */
Page* BufferManager::getPage(PageId page_id) {

  if( buf_map.contains(page_id) ){
    FrameId tmp = buf_map.get(page_id);
    Frame &frame = frame_table[tmp];
    frame.pin_count++;
    return &buf_pool[tmp];
  }

  BufferState state = getBufferState();
  if( state.unpinned == 0 ){
    throw InsufficientSpaceBufMgr();
  }

  FrameId tmp = replacement_pol->replace();
  Frame &frame = frame_table[tmp];

  if( frame.valid && frame.dirty ) {
      disk_mgr->writePage(frame.page_id, &buf_pool[tmp]);
      frame.dirty = false;
  }

  if( frame.valid ){
    buf_map.remove(frame.page_id);
  }

  try{
    disk_mgr->readPage(page_id, &buf_pool[tmp]);
  }catch (InvalidFileIdDiskMgr &e){
    throw InvalidPageIdBufMgr(page_id);
  }catch (InvalidPageNumDiskMgr &e) {
    throw InvalidPageIdBufMgr(page_id);
  }

  frame.page_id = page_id;
  frame.valid = true;
  frame.pin_count = 1;
  frame.dirty = false;

  buf_map.insert(page_id, tmp);
  replacement_pol->pin(tmp);

  return &buf_pool[tmp];  
}

/**
 * @brief Unpins a Page in the buffer pool.
 *
 * @pre A PageId of a pinned Page is provided as input. The Page is in the
 *    buffer pool and is pinned by the executing thread/process.
 * @post The pin count of the Page is decremented. ref_bit is set to true.
 *    Dirty bit is set if the dirty parameter is true.
 *
 * @param page_id PageId of the Page to be released.
 *
 * @throw PageNotPinnedBufMgr If Page is not pinned.
 *    (pin_count is 0).
 * @throw PageNotFoundBufMgr If page_id is not in buf_map.
 */
void BufferManager::releasePage(PageId page_id, bool dirty){
  if( !buf_map.contains( page_id ) ){
    throw PageNotFoundBufMgr(page_id);
  }

  FrameId tmp = buf_map.get(page_id);
  Frame *frame = &frame_table[tmp];

  if( frame->pin_count == 0 ){
    throw PageNotPinnedBufMgr(page_id);
  }

  if( dirty ){
    frame->dirty = true;
  }

  frame->pin_count--;
  
  if( frame->pin_count == 0 ){
    replacement_pol->unpin(tmp);
  }

}

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
void BufferManager::setDirty(PageId page_id){
  
  if( !buf_map.contains( page_id ) ){
    throw PageNotFoundBufMgr(page_id);
  }

  FrameId tmp = buf_map.get(page_id);
  Frame *frame = &frame_table[tmp];
  frame->dirty = true;

}

/**
 * @brief Flushes the Page of the given PageId to disk.
 *
 * @pre A PageId of a pinned Page is provided as input.
 * @post If the Page is set dirty, the Page is written to disk through
 *    the disk_mgr. Page is still pinned.
 *
 * @param page_id PageId of the Page to set dirty.
 *
 * @throw PageNotFoundBufMgr If page_id not in buf_map
 * @throw InvalidFileIdDiskMgr If page_id.file_id not valid.
 * @throw InvalidPageNumDiskMgr If page_id.page_num not valid.
 */
void BufferManager::flushPage(PageId page_id){
  
  if( !buf_map.contains( page_id ) ){
    throw PageNotFoundBufMgr(page_id);
  }

  FrameId tmp = buf_map.get(page_id);
  Frame *frame = &frame_table[tmp];

  if( frame->dirty ){
    disk_mgr->writePage(page_id, &buf_pool[tmp]);
    frame->dirty = false;
  }
}

/**
 * @brief Calls createFile() method on the DiskManager to create new Unix
 *    file that corresponds to the given FileId
 *
 * @pre FileId is valid.
 * @post Unix file that corresponds to the file_id is created.
 *
 * @param file_id FileId of the file to be created.
 *
 * @see DiskManager::createFile()
 */
void BufferManager::createFile (FileId file_id){
  return this->disk_mgr->createFile(file_id);
}

/**
 * @brief Calls removeFile() method on the DiskManager. Checks that none
 *    of the file's pages are pinned in the buffer pool. Removes any of the
 *    file's pages from the buffer pool before removing from disk.
 *
 * @pre A valid FileId is given as a parameter. None of the file's pages
 *    are pinned in the buffer pool.
 * @post If the file has pages in the buffer pool, the corresponding frames
 *    are reset and pages are removed from buf_map. The file is removed
 *    from disk via DiskManager->removeFile().
 *
 * @param file_id FileId of the file to be removed.
 *
 * @throw PagePinnedBufMgr If there are pinned pages of file_id.
 *
 * @see DiskManager::removeFile()
 */
void BufferManager::removeFile(FileId file_id){
  
  for( FrameId i = 0; i < BUF_SIZE; i++ ){
    Frame &frame = frame_table[i];

    if( frame.valid && frame.page_id.file_id == file_id ){
        if( frame.pin_count > 0 ){
            throw PagePinnedBufMgr(frame.page_id);
        }

        buf_map.remove( frame.page_id );
        frame.valid = false;
        frame.dirty = false;
        frame.pin_count = 0;
        replacement_pol->freeFrame(i);
    }
  }

  this->disk_mgr->removeFile(file_id);
}


/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Returns the current state of the buffer pool.
 * @see BufferState
 */
BufferState BufferManager::getBufferState(){
  Frame* cur_frame;
  BufferState cur_buf = 
     {BUF_SIZE, 0, 0, 0, 0, {INVALID_REP_TYPE, 0, 0, 0, 0, 0}};

  //iterate through the buffer
  for (FrameId i = 0; i < BUF_SIZE; i++){
    cur_frame = &(this->frame_table[i]);
    if (cur_frame->valid){
      cur_buf.valid++;
    }
    if(cur_frame->pin_count >0){
      cur_buf.pinned++;
    }
    if (cur_frame->dirty){
      cur_buf.dirty++;
    }
  }
  replacement_pol->getRepStats(&(cur_buf.replace_stats));
  cur_buf.unpinned = BUF_SIZE - cur_buf.pinned;
  return cur_buf;
}

/**
 * @brief Return the amount of unpinned pages in the buffer pool
 */
std::uint32_t BufferManager::getNumUnpinned(){

  BufferState cur_buf = getBufferState();
  return cur_buf.unpinned;
}
/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints frame state of every frame in the buffer pool, including
 *    pin count, valid bit, dirty bit, and ref_bit. If Page is valid,
 *    PageId is printed.
 */
void BufferManager::printAllFrames(){
  for (FrameId i = 0; i < BUF_SIZE; i++){
    std::cout <<  "Frame " << i << ": \n";
    this->_printFrameHelper(i);
  }
}

/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints frame state of every valid frame in the buffer pool,
 *    including PageId, pin count, valid bit, dirty bit, and ref_bit.
 */
void BufferManager::printValidFrames(){

  Frame  *cur_frame;

  for (FrameId i = 0; i < BUF_SIZE; i++){
    cur_frame = &(this->frame_table[i]);
    if (cur_frame->valid){ // only print out frame num if valid
      std::cout <<  "Frame " << i << ": \n";
      this->_printFrameHelper(i);
    }
  }
}

/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints frame state of given FrameId, including pin count,
 *    valid bit, dirty bit, and ref_bit. If Page is valid, PageId is
 *    printed.
 */
void BufferManager::printFrame(FrameId frame_id){
  this->_printFrameHelper(frame_id);
}


/**
 * @brief this is a helper METHOD to print one frame
 *    Prints frame state of given FrameId, including pin count,
 *    valid bit, dirty bit, and ref_bit. If Page is valid, PageId is
 *    printed.
 * @pre: caller has obtained the buf_map_mtx lock
 * @post: caller still holds the buf_map_mtx lock
 */
void BufferManager::_printFrameHelper(FrameId frame_id){
  Frame *cur_frame;

  cur_frame = &(this->frame_table[frame_id]);
  if (cur_frame->valid){
    std::cout <<  "PageId:  {" << cur_frame->page_id.file_id << "," <<
        cur_frame->page_id.page_num << "}, ";
  }
  std::cout << "pin count: " << cur_frame->pin_count << ", " << "valid: " <<
      cur_frame->valid << ", " <<"dirty: " << cur_frame->dirty;
  this->replacement_pol->printFrame(frame_id);
}

/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints frame state of given PageId, including FrameId, pin count,
 *    valid bit, dirty bit, and ref_bit. If Page is not in the buffer map,
 *    prints "Page Not Found".
 */
void BufferManager::printPage(PageId page_id){
  Frame* cur_frame;
  FrameId frame_id;
  if (!this->buf_map.contains(page_id)){
    std::cout<< "Page Not Found!"  << std::endl;
    return;
  }
  frame_id = this->buf_map.get(page_id);
  cur_frame = &(this->frame_table[frame_id]);
  std::cout <<  "FrameId: " << frame_id << ", "  << "pin count: " <<
      cur_frame->pin_count << ", " << "valid: " << cur_frame->valid << ", "  <<
      "dirty: " << cur_frame->dirty;
    this->replacement_pol->printFrame(frame_id);
}

/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints current buffer state, including total number of pages,
 *    number of valid pages, number of pinned pages, number of dirty pages,
 *    number of pages whose ref bit is set and the current clock hand
 *    position.
 */
void BufferManager::printBufferState(){
  BufferState cur_buf = 
     {BUF_SIZE, 0, 0, 0, 0, {INVALID_REP_TYPE, 0, 0, 0, 0, 0}};

  cur_buf =  this->getBufferState();
  //print current buffer state

  // for(FrameId i = 0; i < BUF_SIZE; i++){
  //   if(this->frame_table[i].pin_count > 0){
  //     std::cout << "Frame Id: " << i << "\nPin Count: " 
  //     << this->frame_table[i].pin_count << std::endl;
  //   }
  // }
  std::cout << "Total number of pages: " << cur_buf.total << std::endl;
  std::cout << "Number of valid pages: " << cur_buf.valid << std::endl;
  std::cout << "Number of pinned pages: " << cur_buf.pinned << std::endl;
  std::cout << "Number of unpinned pages: " << cur_buf.unpinned << std::endl;
  std::cout << "Number of dirty pages: " << cur_buf.dirty <<std::endl;
  std::cout << "Replacement Policy: " << 
    bm_rep_strs[cur_buf.replace_stats.rep_type] << std::endl;
  std::cout << "Number of calls to replacement policy: " << 
    cur_buf.replace_stats.rep_calls << std::endl;
  std::cout << "Average frames checked per call: " << 
    cur_buf.replace_stats.avg_frames_checked << std::endl;
  std::cout << "Number of pages with ref bit set: " << 
    cur_buf.replace_stats.ref_bit << std::endl;
  std::cout << "Current clock hand position: " << 
    cur_buf.replace_stats.clock_hand <<std::endl;
}


/**
 * @brief This method is for performance tests.
 *    Prints number of calls to replacment policy, average check on
 *    replacement calls, lru/mru queue/stack usage.
 */
void BufferManager::printReplacementStats(){
  this->replacement_pol->printStats();
  std::cout << std::endl;
}