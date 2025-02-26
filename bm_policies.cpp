/**
 * @file bm_policies.cpp
 * @author Nikoloz Khmaladze & Omar Ebied
 * @date 2025-02-09
 * 
 * Implementation of the Clock and Random replacement
 * policies.
 * This file contains the implementations of the Clock and Random page
 * replacement policies, which are used by the BufferManager to decide which
 * pages to evict from the buffer pool when it's full.  It defines the
 * methods for these policies, including how they track page usage and
 * select victims for replacement.
 */

#include <utility>
#include <iostream>
#include <deque>
#include "bufmgr.h"
#include "swatdb_exceptions.h"
#include "file.h"
#include "page.h"
#include "catalog.h"
#include "math.h"
#include "bm_replacement.h"
#include "bm_policies.h"
#include "bm_frame.h"


    
/**
 * @brief Clock constructor. Initializes pointer to buffer manager's
 *    frame_table, creates the free list, sets clock_hand, rep_calls,
 *    and avg_frames_checked to 0, and sets all ref_bits to false.
 *
 * @pre BufferManager constructor is being called, frame_table parameter
 *    points to an initialized frame_table object.
 *
 * @post A Clock Replacement object will be initialized with member variables
 *    initialized.
 *
 * @param Frame *frame_table, pointer to frame_table array of BufferManager.
 */
Clock::Clock(Frame *frame_table){
  
  this->frame_table = frame_table;
  this->_createFree();
  this->clock_hand = 0;
  this->rep_calls = 0;
  this->avg_frames_checked = 0.0;

  for( uint32_t i = 0; i < BUF_SIZE; i++ ){
    this->ref_table[i] = false;
  }
}


/**
* @brief Empty Destructor. 
*/
Clock::~Clock(){}


/**
 * @brief Method that implements core replacement policy
 *    functionality of the clock replacement policy.
 *
 * @pre The replacement policy has been invoked. The buffer manager is
 *    attempting to add a frame to the buffer pool. The buffer map is
 *    locked.
 *
 * @post If there is a free (invalid) frame, that FrameID is chosen for
 *    replacement and is returned. If there is a Page that is valid, and is not
 *    referenced, it is selected for eviction and FrameId is returned. In the
 *    process of performing the replacement algorithm, ref_bit of some
 *    valid pages that are not pinned may be set to false.
 *
 * @return FrameID of the frame that is eligible for replacement in the
 *    buffer pool.
 *
 * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
 *    pinned.
 */
FrameId Clock::replace(){
  if( !this->free.empty() ){
     FrameId frontID = this->free.front();
     this->free.pop();
     return frontID;
  }

  uint32_t frames_checked = 0;
  
  while(true){
    Frame &frame = this->frame_table[this->clock_hand];

    // had to check for pinned pages FIRST
    if(frame.pin_count > 0){
      this->_advanceClock();
      frames_checked++;
      continue; //don't consider for replacement
    }
    
    // now we can consider for replacement (only valid frames for replacement)
    if (frame.valid){ 
      if(this->ref_table[this->clock_hand]){
        this->ref_table[this->clock_hand] = false;
        this->_advanceClock();
      }
      else{
        this->rep_calls++;
        this->avg_frames_checked = (((this->avg_frames_checked) * (this->rep_calls-1)) + frames_checked) / ((this->rep_calls)+1);
        FrameId tempclock = this->clock_hand;
        this->_advanceClock();
        return tempclock;
      }
    }

    if (frames_checked >= BUF_SIZE) {
        throw InsufficientSpaceBufMgr();
    }
  }
}


/**
 * @brief Frame is being pinned with pin count going from 0 to 1. Clock
 * policy has nothing to do here, but an empty method must exist for 
 * BufferManager to call.
 *
 * @pre A page in the buffer pool has been pinned, and the pin count of that
 *    frame has increased from 0 to 1.
 *
 * @post Nothing.
 *
 * @param FrameId frame_id of the frame being unpinned.
 */
void Clock::pin(FrameId frame_id){}


/**
 * @brief Frame is being unpinned with pin count going from 1 to 0. Set the
 * ref_bit associated with this frame_id to true.
 *
 * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
 *
 * @post Ref_bit of frame_id is set to true.
 *
 * @param FrameId frame_id of the frame being unpinned.
 */
void Clock::unpin(FrameId frame_id){
  this->ref_table[frame_id] = true;
}

/**
 * @brief Updates the struct of replacement policy statistics within the
 *   buffer state struct. Specifies the clock replacement type, number of calls
 *   to the replacement policy, average frames checked by each call to
 *   policy, the ref_bit count and clock_hand, and new page calls.
 *
 * @param Pointer to the ReplacementStats struct member of BufferState struct.
 */
void Clock::getRepStats(struct BufferState::ReplacementStats *rep_stats){
  rep_stats->rep_type = _getType();
  rep_stats->rep_calls = this->rep_calls;
  rep_stats->avg_frames_checked = this->avg_frames_checked;
  rep_stats->new_page_calls = this->new_page_calls;
  rep_stats->ref_bit = 0;
  // loop through ref_table to get total ref_bits set to true
  for(FrameId i = 0; i < BUF_SIZE; i++){
    if (this->ref_table[i]){
      rep_stats->ref_bit ++;
    }
  }
  rep_stats->clock_hand = this->clock_hand;
}


/**
 * @brief The frame is invalidated in the buffer manager, and is
 *    added to the free list of frames in the replacement policy. Ref_bit
 *    associated with this frame is set to false.
 *
 * @pre A page in the buffer pool has been deallocated.
 *
 * @post The replacement policy state is updated to reflect the freeing
 *    of a specific frame.
 *
 * @param FrameId frame_id of the frame being set invalid.
 */
void Clock::freeFrame(FrameId frame_id){
  this->free.push(frame_id);
  this->ref_table[frame_id] = false;
}


/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Prints Frame state, including pin count and ref_bit.
 */
void Clock::printFrame(FrameId frame_id){  
  std::cout << ", " <<
  "ref_bit: " << this->ref_table[frame_id] << std::endl;
}


/**
 * @brief Prints clock_hand, average frames checked, replacement calls,
 *    how many frames have ref_bit set.
 */
void Clock::printStats(){
  int ref_bit_count = 0;
  double pct_replace = 0;

  for(FrameId i = 0; i < BUF_SIZE; i++){
    if (this->ref_table[i]){
      ref_bit_count ++;
    }
  }

  if(this->new_page_calls != 0) {  // don't divide by 0
    pct_replace = 100* (double)this->rep_calls / this->new_page_calls;
  } 
  std::cout << "Replacement Policy: " << "CLOCK" << std::endl;
  std::cout << "Number of calls to replacement policy: " 
    << this->rep_calls << std::endl;
  std::cout << "Percentage of new page calls that use replacement policy: " 
    << pct_replace << "%" << std::endl;
  std::cout << "Number of new page calls: " 
    << this->new_page_calls << std::endl;
  std::cout << "Average frames checked per call to replacement policy: " 
    << this->avg_frames_checked << std::endl;
  std::cout << "Clock hand position: " << this->clock_hand << std::endl;
  std::cout << "Frames with ref bit set: " << ref_bit_count << std::endl;
}

/**
 * @brief Increments the clock hand according to the clock replacement
 *    policy.
 *
 * @pre None.
 * @post clock_hand is advanced.
 */
void Clock::_advanceClock(){
  this->clock_hand = (this->clock_hand + 1) % BUF_SIZE;
}


/**
 * @brief Derived class from pure virtual function that returns replacement
 * policy type specific to this derived class.
 *
 * @return RepType ClockT.
 *
 */
RepType Clock::_getType(){
  return ClockT;
}



/**
 * @brief Random constructor. Initializes pointer to buffer manager's
 *    frame_table, creates the free list, sets rep_calls and
 *    avg_frames_checked to 0.
 *
 * @pre BufferManager constructor is being called, frame_table parameter
 *    points to an initialized frame_table object.
 *
 * @post A Random policy object will be initialized with member variables
 *    initialized.
 *
 * @param Frame *frame_table, pointer to frame_table array of BufferManager.
 */
Random::Random(Frame *frame_table) {
  this->frame_table = frame_table;
  this->rep_calls = 0;
  this->new_page_calls = 0;
  this->avg_frames_checked = 0;
  for(std::uint32_t i = 0; i < BUF_SIZE; i++){
    this->times_chosen[i] = 0;
  }
  this->_createFree();
}


/**
* @brief Empty Destructor. 
*/
Random::~Random(){}


/**
 * @brief Method that implements the Random replacement policy functionality
 *
 * @pre The replacement policy has been invoked. The buffer manager is
 *    attempting to add a frame to the buffer pool. The buffer map is
 *    locked.
 *
 * @post If there is a free (invalid) frame, that FrameID is chosen for
 *    replacement and is returned. Else, a random frame is selected and
 *    evaluated for eligibility. If it is pinned, another random frame is
 *    selected. If no unpinned frame is found by BUF_SIZE attempts,
 *    sequential scan is done to find an eligible frame. If all pages are
 *    pinned, an exception is thrown.
 *
 * @return FrameID of the frame that is eligible for replacement in the
 *    buffer pool.
 *
 * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
 *    pinned.
 */
FrameId Random::replace() {
  // first check if there are free frames and return one if true
  if(!this->free.empty()) {
    FrameId frame_id = this->free.front();
    this->free.pop();
    return frame_id;
  }
  std::uint32_t rand_num = std::rand() % BUF_SIZE;
  std::uint32_t c = 0;
  Frame *cur_frame = &this->frame_table[rand_num];
  // Tries random frame BUF_SIZE times
  while(cur_frame->pin_count != 0 && c < BUF_SIZE/2) {
    c++;
    rand_num = std::rand() % BUF_SIZE;
    cur_frame = &this->frame_table[rand_num];
  }
  // if random frame tried BUF_SIZE times, loop throuugh and check every frame
  if(c == BUF_SIZE) {
    for(FrameId i = 0; i < BUF_SIZE; i++) {
      cur_frame = &this->frame_table[i];
      if(cur_frame->pin_count == 0){
        // update replacement stats
        if((this->rep_calls + 1) == 0) {  // start over if wrap-around
          this->rep_calls = 0;
        }
        this->avg_frames_checked = (this->avg_frames_checked *
                                    this->rep_calls) + c + i;
        this->rep_calls ++;
        this->avg_frames_checked /= this->rep_calls;
        this->times_chosen[i] ++;
        return i;
      }
    }
    throw InsufficientSpaceBufMgr();
  } else { // random frame found that can be replaced before BUF_SIZE tries
    // update replacement stats
    //  std::cout << "frame found: " << rand_num << std::endl;
    if((this->rep_calls + 1) == 0) {  // start over if wrap-around
      this->rep_calls = 0;
    }
    this->avg_frames_checked = 
      (this->avg_frames_checked * this->rep_calls) + c + 1;
    this->rep_calls ++;
    this->avg_frames_checked /= this->rep_calls;
    this->times_chosen[rand_num] ++;
    return rand_num;
  }
}


/**
 * @brief Frame is being pinned with pin count going from 0 to 1. Ramdom
 *    policy has nothing to do here, but an empty method must exist for
 *    BufferManager to call.
 *
 * @pre A page in the buffer pool has been pinned, and the pin count of that
 *    frame has increased from 0 to 1.
 *
 * @post Nothing.
 *
 * @param FrameId frame_id of the frame being unpinned.
 */
void Random::pin(FrameId frame_id){}


/**
 * @brief Frame is being unpinned with pin count going from 1 to 0. Ramdom
 *    policy has nothing to do here, but an empty method must exist for
 *    BufferManager to call.
 *
 * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
 *
 * @post Nothing.
 *
 * @param FrameId frame_id of the frame being unpinned.
 */
void Random::unpin(FrameId frame_id){}


/**
 * @brief Prints average frames checked, replacement calls, summary stats
 *    on randomness.
 */
void Random::printStats(){
  // loop through times_chosen to get avg
  // calculate std dev
  double sum = 0;
  for(FrameId i = 0; i < BUF_SIZE; i++){
    sum += this->times_chosen[i];
  }
  double avg = sum / BUF_SIZE;
  sum = 0;
  for(FrameId i = 0; i < BUF_SIZE; i++){
    sum += ((this->times_chosen[i] - avg) * (this->times_chosen[i] - avg));
  }
  sum = sum / (BUF_SIZE - 1);
  double sd = sqrt(sum);

  double pct_replace = 100* (double)this->rep_calls / this->new_page_calls;
  std::cout << "Replacement Policy: " << "RANDOM" << std::endl;
  std::cout << "Number of calls to replacement policy: " 
    << this->rep_calls << std::endl;
  std::cout << "Percentage of new page calls that use replacement policy: " 
    << pct_replace << "%" << std::endl;
  std::cout << "Number of new page calls: " << this->new_page_calls 
    << std::endl;
  std::cout << "Average frames checked per call to replacement policy: " 
    << this->avg_frames_checked << std::endl;
  std::cout << "Average times each frame is chosen: " << avg << std::endl;
  std::cout << "Standard deviation of times each frame is chosen: " << sd 
    << std::endl;
}


/**
 * @brief Derived class from pure virtual function that returns replacement
 * policy type specific to this derived class.
 *
 * @return RepType RandomT.
 *
 */
RepType Random::_getType(){
  return RandomT;
}

