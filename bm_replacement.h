#ifndef _SWATDB_BM_REPLACEMENT_H_
#define  _SWATDB_BM_REPLACEMENT_H_

/**
 * \file bm_replacement.h: base class for Buffer Pool replacement policies
 */


#include <utility>
#include "swatdb_types.h"
#include "bufmgr.h"   // need for BufferState stats struct

/**
 * SwatDB ReplacementPolicy Class.
 * ReplacementPolicy is an abstract class that manages the buffer replacement
 * policy of the DBMS. Individual child classes implemement each policy.
 * Methods work within the buffer manager to choose frames to replace from
 * the frame table.
 */
class ReplacementPolicy {


  public:
    
    /**
     * @brief Empty Destructor. 
     */
    virtual ~ReplacementPolicy();

    /**
     * @brief Pure virtual method that implements core replacement policy
     *        functionality in child classes.
     *
     * @pre The replacement policy has been invoked. The buffer manager is
     *      attempting to add a frame to the buffer pool. The buffer map is
     *      locked.
     *
     * @post If there is a free (invalid) frame, that FrameID is chosen for
     *       replacement and is returned. Else, the replacement policy is
     *       invoked and used to determine and return the FrameID of an
     *       unpinned Frame which is eligible for replacement. If all pages are
     *       pinned, an exception is thrown.
     *
     * @return FrameID of the frame that is eligible for replacement in the
     *         buffer pool.
     *
     * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
     *        pinned.
     */
    virtual FrameId replace() = 0;

    /**
     * @brief Pure virtual method that implements any necessary update to
     *        replacement policy state following the pinning of a page in the
     *        buffer pool.
     *
     * @pre A page in the buffer pool has been pinned, and the pin count of
     *      that frame has increased from 0 to 1.
     *
     * @post The replacement policy state is updated to reflect the pinning
     *    of a specific page. The state updates vary for the different policies
     */
    virtual void pin(FrameId frame_id) = 0;

    /**
     * @brief Pure virtual method that implements any necessary update to
     *        replacement policy state following the unpinning of a page in the
     *        buffer pool.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post The replacement policy state is updated to reflect the unpinning
     *       of a specific page. The state updates vary for the different
     *       policies
     */
    virtual void unpin(FrameId frame_id) = 0;

    /**
     * @brief Virtual method that implements any necessary update to
     *        replacement policy state following the deallocation of a page in
     *        the buffer pool. The frame is invalidated in the buffer manager,
     *        and is added to the free list of frames in the replacement
     *        policy. Some policies must implement additional functionality.
     *
     * @pre A page in the buffer pool has been deallocated
     *
     * @post The replacement policy state is updated to reflect the freeing
     *       of a specific frame. The state updates vary for different
     *       policies.
     */
    virtual void freeFrame(FrameId frame_id);

    
    /**
    * @brief Updates the struct of replacement policy statistics within the
    *        buffer state struct. Adds the replacement type, number of calls to
    *        the replacement policy, number of calls to getPage or allocatePage
    *        average frames checked by each call to policy, and the ref_bit
    *        count and clock_hand, which is irrelevant unless the policy is
    *        Clock Replacement.
    */
    virtual void getRepStats(struct BufferState::ReplacementStats *rep_stats);

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints Frame state relevant to the replacement policy of a given
     *        FrameId, including pin count and ref_bit for Clock Replacement.
     */
    void printFrame(FrameId frame_id);


    /**
     * @brief Pure virtual method for printing relevant stats for the specific
     *        derived class.
     */
    virtual void printStats() = 0;

    /**
     * @brief Increments count of how many times allocatePage and 
     *        getPage have been called in BufferManager.  
     *        Used to compute statistics about replacement algorithms.
     */
    void incrementGetAllocCount(); 
 
  protected:

    /**
     * @brief Returns the type of a given replacement policy. Used to fill
     *        struct of replacement statistics.
     *
     * @return RepType of replacement policy.
     */
    virtual RepType _getType() = 0;

    /**
     * @brief Creates a free list for the buffer pool by checking each frame in
     *        the buffer pool and checking if it is valid. Used in constructor
     *        of each replacement policy.
     * @pre None.
     *
     * @post Free list member variable of policy is filled.
     */
    void _createFree();

    /**
     * Pointer to the frame table in the buffer manager
     */
    Frame *frame_table;

    /**
     * List of all free pages in the buffer pool.
     */
    std::queue<FrameId> free;

    /**
     * Running total of the amount of calls to the replacement policy since the
     * constructor.
     */
    std::uint64_t rep_calls;

    /**
     * Running average of the amount of frames checked per call to policy.
     * Frames in the free list do not contribute to this average.
     */
    double avg_frames_checked;


    /**
     * Running total of the number of calls to getPage or allocatePage in the 
     * buf_mgr. Allows calculation of percentage of getPages where replacement 
     * was required. 
     */
    std::uint64_t new_page_calls; 

};

#endif
