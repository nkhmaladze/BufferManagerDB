#include "bufmgr.h"
#include "swatdb_exceptions.h"
#include "diskmgr.h"
#include "file.h"
#include "page.h"
#include "catalog.h"
#include "math.h"
#include "bm_replacement.h"

/**
* @brief Empty Destructor. 
*/
ReplacementPolicy::~ReplacementPolicy(){}



/**
 * @brief Pure virtual method that implements any necessary update to
 *    replacement policy state following the deallocation of a page in the
 *    buffer pool. The frame is invalidated in the buffer manager, and is
 *    added to the free list of frames in the replacement policy. Some
 *    policies must implement additional functionality.
 *
 * @pre A page in the buffer pool has been deallocated
 *
 * @post The replacement policy state is updated to reflect the freeing
 *
 * @param FrameId frame_id of the frame being set invalid.
 *
 */
void ReplacementPolicy::freeFrame(FrameId frame_id) {
  this->free.push(frame_id);
}


/**
 * @brief Updates the struct of replacement policy statistics within the
 *    buffer state struct. Adds the replacement type, number of calls to
 *    the replacement policy, number of calls to getPage or allocatePage
 *    average frames checked by each call to policy, and the ref_bit
 *    count and clock_hand, which is irrelevant unless the policy is 
 *    Clock Replacement.
 */
void ReplacementPolicy::getRepStats(struct BufferState::ReplacementStats *rep_stats){
  rep_stats->rep_type = _getType();
  rep_stats->rep_calls = this->rep_calls;
  rep_stats->avg_frames_checked = this->avg_frames_checked;
  rep_stats->new_page_calls = this->new_page_calls;
  rep_stats->ref_bit = 0;
  rep_stats->clock_hand = 0;
}

/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints Frame state relevant to the replacement policy of a given
 *    FrameId, including pin count and ref_bit for Clock Replacement.
 */
void ReplacementPolicy::printFrame(FrameId frame_id){
  std::cout << std::endl;
}


/**
 * @brief Creates a free list for the buffer pool by checking each frame in
 *    the buffer pool and checking if it is valid. Used in constructor of
 *    each replacement policy.
 * @pre None.
 *
 * @post Free list member variable of policy is filled.
 */
void ReplacementPolicy::_createFree(){

  Frame *cur_frame;

  for(FrameId i = 0; i < BUF_SIZE; i++){
    cur_frame = &this->frame_table[i];
    // if current frame is invalid, add to free list
    if(!cur_frame->valid){
      this->free.push(i);
    }
  }
}


/**
  * @brief Increment new_page_calls by 1. Called by both allocatePage and 
  *    getPage in BufferManager (on successful getPage or allocatePage).  
  *    Used to compute statistics about replacment algorithms.
  */
void ReplacementPolicy::incrementGetAllocCount(){
  this->new_page_calls++;
}