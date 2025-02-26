#ifndef _SWATDB_BM_POLICIES_H_
#define  _SWATDB_BM_POLICIES_H_

/**
 * \file bm_policies.h: specific Buffer Pool replacement policies
 */


#include <utility>
#include <list>
#include <queue>

#include "swatdb_types.h"
#include "bm_replacement.h"     // base class def


/**
 * SwatDB Clock Class.
 * Clock is a derived class of ReplacementPolicy that manages the buffer
 * replacement policy of the DBMS. The Clock algorithm is an approximation of
 * least recently used with less overhead. Chooses a page for replacement using
 * a clock hand, which loops the buffer frames in a circular order. Each frame
 * has an associated reference bit, which is turned on when the pin count goes
 * to zero.
 */
class Clock: public ReplacementPolicy {


  public:


    /**
     * @brief Clock constructor. Initializes pointer to buffer manager's
     *        frame_table, creates the free list, sets clock_hand, rep_calls,
     *        and avg_frames_checked to 0, and sets all ref_bits to false.
     *
     * @pre BufferManager constructor is being called, frame_table parameter
     *      points to an initialized frame_table object.
     *
     * @post A Clock object will be initialized with member variables
     *       initialized.
     *
     * @param Frame *frame_table, pointer to frame_table array of
     *        BufferManager.
     */
    Clock(Frame *frame_table);

    /**
     * Empty Destructor.
     */
    ~Clock();

    /**
     * @brief Method that implements core replacement policy
     *        functionality of the clock replacement policy.
     *
     * @pre The replacement policy has been invoked. The buffer manager is
     *      attempting to add a frame to the buffer pool. The buffer map is
     *      locked.
     *
     * @post If there is a free (invalid) frame, that FrameID is chosen for
     *       replacement and is returned. If there is a Page that is valid, and
     *       is not referenced, it is selected for eviction and FrameId is
     *       returned.  In the process of performing the replacement algorithm,
     *       ref_bit of some valid pages that are not pinned may be set to
     *       false.
     *
     * @return FrameID of the frame that is eligible for replacement in the
     *         buffer pool.
     *
     * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
     *        pinned.
     */
    FrameId replace();


    /**
     * @brief Frame is being pinned with pin count going from 0 to 1. Clock
     *        policy has nothing to do here, but an empty method must exist for
     *        BufferManager to call.
     *
     * @pre A page in the buffer pool has been pinned, and the pin count of
     *      that frame has increased from 0 to 1.
     *
     * @post Nothing.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void pin(FrameId frame_id);

    /**
     * @brief Frame is being unpinned with pin count going from 1 to 0. Set the
     *        ref_bit associated with this frame_id to true.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post Ref_bit of frame_id is set to true.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void unpin(FrameId frame_id);


    /**
     * @brief Updates the struct of replacement policy statistics within the
     *        buffer state struct. Adds the replacement type, number of calls
     *        to the replacement policy, average frames checked by each call to
     *        policy, the ref_bit count and clock_hand, and new page calls. 
     *
     * @param Pointer to the ReplacementStats struct member of BufferState
     *        struct.
     */
    void getRepStats(struct BufferState::ReplacementStats *rep_stats);


    /**
     * @brief The frame is invalidated in the buffer manager, and is
     *        added to the free list of frames in the replacement policy.
     *        Ref_bit associated with this frame is set to false.
     *
     * @pre A page in the buffer pool has been deallocated.
     *
     * @post The replacement policy state is updated to reflect the freeing
     *       of a specific frame.
     *
     * @param FrameId frame_id of the frame being set invalid.
     */
    void freeFrame(FrameId frame_id);


    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *        Prints Frame state, including pin count and ref_bit.
     */
    void printFrame(FrameId frame_id);


    /**
     * @brief Prints clock_hand, average frames checked, replacement calls,
     *        how many frames have ref_bit set.
     */
    void printStats();


  private:


    /**
     * Clock hand which is a std::uint32_t, corresponding to the index of
     * a Frame in frame_table.
     */
    std::uint32_t clock_hand;


    /**
     * Parallel array to the frame_table which stores the ref_bits of each
     * frame.
     */
    bool ref_table[BUF_SIZE];


    /**
     * @brief Increments the clock hand according to the clock replacement
     *    policy.
     *
     * @pre None.
     * @post clock_hand is advanced.
     */
    void _advanceClock();


    /**
     * @brief Derived class from pure virtual function that returns replacement
     *        policy type specific to this derived class.
     *
     * @return RepType ClockT.
     *
     */
    RepType _getType();


};


/**
 * SwatDB Random Class.
 * Random is a derived class of ReplacementPolicy that manages the buffer
 * replacement policy of the DBMS. The random algorithm randomly generates
 * frame ids to check if they are eligible for replacement. Performs well on
 * large buffer pools and has minimal overhead.
 */
class Random: public ReplacementPolicy {


  public:


    /**
     * @brief Random constructor. Initializes pointer to buffer manager's
     *        frame_table, creates the free list, sets rep_calls and
     *        avg_frames_checked to 0.
     *
     * @pre BufferManager constructor is being called, frame_table parameter
     *      points to an initialized frame_table object.
     *
     * @post A Random policy object will be initialized with member variables
     *      initialized.
     *
     * @param Frame *frame_table, pointer to frame_table array of
     *        BufferManager.
     */
    Random(Frame *frame_table);

    /**
     * Empty Destructor.
     */
    ~Random();


    /**
     * @brief Method that implements the Random replacement policy
     *        functionality
     *
     * @pre The replacement policy has been invoked. The buffer manager is
     *      attempting to add a frame to the buffer pool. The buffer map is
     *      locked.
     *
     * @post If there is a free (invalid) frame, that FrameID is chosen for
     *       replacement and is returned. Else, a random frame is selected and
     *       evaluated for eligibility. If it is pinned, another random frame
     *       is selected. If no unpinned frame is found by BUF_SIZE attempts,
     *       sequential scan is done to find an eligible frame. If all pages
     *       are pinned, an exception is thrown.
     *
     * @return FrameID of the frame that is eligible for replacement in the
     *         buffer pool.
     *
     * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
     *        pinned.
     */
    FrameId replace();


    /**
     * @brief Frame is being pinned with pin count going from 0 to 1. Random
     *        policy has nothing to do here, but an empty method must exist for
     *        BufferManager to call.
     *
     * @pre A page in the buffer pool has been pinned, and the pin count of
     *      that frame has increased from 0 to 1.
     *
     * @post Nothing.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void pin(FrameId frame_id);

    /**
     * @brief Frame is being unpinned with pin count going from 1 to 0. Random
     *        policy has nothing to do here, but an empty method must exist for
     *        BufferManager to call.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post Nothing.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void unpin(FrameId frame_id);


    /**
     * @brief Prints average frames checked, replacement calls, summary stats
     *        on randomness.
     */
    void printStats();


  private:


    /**
     * @brief Derived class from pure virtual function that returns replacement
     *        policy type specific to this derived class.
     *
     * @return RepType RandomT.
     */
    RepType _getType();


    /**
     * Parallel array to frame_table that stores number of times each frame_id
     *    is chosen for replacement. Is useful for determining randomness.
     */
    int times_chosen[BUF_SIZE];


};


/**
 * SwatDB MRU Class.
 * MRU is a derived class of ReplacementPolicy that manages the buffer
 * replacement policy of the DBMS. The Most Recently Used (MRU) algorithm keeps
 * a stack of unpinned pages, choosing the most recently unpinned page for
 * replacement. It is well suited for sequential scans, but it has more
 * overhead than other policies.
 */
class MRU: public ReplacementPolicy {


  public:


    /**
     * @brief MRU constructor. Initializes pointer to buffer manager's
     *    frame_table, creates the free list, sets stack, rep_calls,
     *    and avg_frames_checked to 0.
     *
     * @pre BufferManager constructor is being called, frame_table parameter
     *    points to an initialized frame_table object.
     *
     * @post A MRU object will be initialized with member variables
     *    initialized.
     *
     * @param Frame *frame_table, pointer to frame_table array of BufferManager.
     */
    MRU(Frame *frame_table);
   
    /**
     * Empty Destructor.
     */
    ~MRU();

    /**
     * @brief Method that implements the Most Recently Used (MRU) replacement
     *        policy functionality.
     *
     * @pre The replacement policy has been invoked. The buffer manager is
     *      attempting to add a frame to the buffer pool. The buffer map is
     *      locked.
     *
     * @post If there is a free (invalid) frame, that FrameID is chosen for
     *       replacement and is returned. Else, the most recently unpinned
     *       frame is chosen and taken off the stack. If all pages are pinned,
     *       an exception is thrown.
     *
     * @return FrameID of the frame that is eligible for replacement in the
     *         buffer pool.
     *
     * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
     *        pinned.
     */
    FrameId replace();


    /**
     * @brief Frame is being pinned with pin count going from 0 to 1. Frame id
     *        is removed from stack.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post Frame id is removed from stack.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void pin(FrameId frame_id);


    /**
     * @brief Frame is being unpinned with pin count going from 1 to 0. Frame
     *        id is added to the top of the stack.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post Frame id is added to the top of the stack.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void unpin(FrameId frame_id);


    /**
     * @brief The frame is invalidated in the buffer manager, and is
     *        added to the free list of frames in the replacement policy.
     *        frame_id is also removed from the stack.
     *
     * @pre A page in the buffer pool has been deallocated.
     *
     * @post frame_id is added to the free list and removed from the stack.
     *
     * @param FrameId frame_id of the frame being set invalid.
     */
    void freeFrame(FrameId frame_id);

    /**
     * @brief Prints replacement calls, average frames checked when pinning,
     *        and max and average size of stack.
     */
    void printStats();


  private:


    /**
     * List that is used like a stack that holds all unpinned frames.
     */
    std::list<FrameId> stack;


    /**
     * @brief Derived class from pure virtual function that returns replacement
     *        policy type specific to this derived class.
     *
     * @return RepType MruT.
     *
     */
    RepType _getType();


    /**
     * Number of times frame has to be removed from stack. Used for calculating
     *    avg_frames_checked.
     */
    int remove_calls;

};



/**
 * SwatDB LRU Class.
 * LRU is a derived class of ReplacementPolicy that manages the buffer
 * replacement policy of the DBMS. The Least Recently Used (LRU) algorithm
 * keeps a queue of unpinned pages, choosing the least recently unpinned page
 * for replacement. It is a generally good policy, but it has more overhead
 * than other policies.
 */
class LRU: public ReplacementPolicy {


  public:


    /**
     * @brief LRU constructor. Initializes pointer to buffer manager's
     *        frame_table, creates the free list, sets queue, rep_calls,
     *        and avg_frames_checked to 0.
     *
     * @pre BufferManager constructor is being called, frame_table parameter
     *      points to an initialized frame_table object.
     *
     * @post A LRU object will be initialized with member variables
     *       initialized.
     *
     * @param Frame *frame_table, pointer to frame_table array of
     *        BufferManager.
     */
    LRU(Frame *frame_table);

    /**
     * Empty Destructor.
     */
    ~LRU();

    /**
     * @brief Method that implements the Least Recently Used (LRU) replacement
     *        policy functionality.
     *
     * @pre The replacement policy has been invoked. The buffer manager is
     *      attempting to add a frame to the buffer pool. The buffer map is
     *      locked.
     *
     * @post If there is a free (invalid) frame, that FrameID is chosen for
     *       replacement and is returned. Else, the least recently unpinned
     *       frame is chosen and taken off the queue. If all pages are pinned,
     *       an exception is thrown.
     *
     * @return FrameID of the frame that is eligible for replacement in the
     *         buffer pool.
     *
     * @throw InsufficientSpaceBufMgr if all pages in the buffer pool are
     *        pinned.
     */
    FrameId replace();


    /**
     * @brief Frame is being pinned with pin count going from 0 to 1. Frame id
     *        is removed from queue.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post Frame id is removed from queue.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void pin(FrameId frame_id);


    /**
     * @brief Frame is being unpinned with pin count going from 1 to 0. Frame
     *        id is added to the bottom of the queue.
     *
     * @pre A page in the buffer pool has been unpinned from pin count 1 to 0.
     *
     * @post Frame id is added to the bottom of the queue.
     *
     * @param FrameId frame_id of the frame being unpinned.
     */
    void unpin(FrameId frame_id);


    /**
     * @brief The frame is invalidated in the buffer manager, and is
     *        added to the free list of frames in the replacement policy.
     *        frame_id is also removed from the queue.
     *
     * @pre A page in the buffer pool has been deallocated.
     *
     * @post frame_id is added to the free list and removed from the queue.
     *
     * @param FrameId frame_id of the frame being set invalid.
     */
    void freeFrame(FrameId frame_id);


    /**
     * @brief Prints replacement calls, average frames checked when pinning,
     *        and max and average size of queue.
     */
    void printStats();


  private:


    /**
     * List that is used like a queue that holds all unpinned frames.
     */
    std::list<FrameId> queue;


    /**
     * @brief Derived class from pure virtual function that returns replacement
     *        policy type specific to this derived class.
     *
     * @return RepType LruT.
     */
    RepType _getType();


    /**
     * Number of times frame has to be removed from stack. Used for calculating
     *    avg_frames_checked.
     */
    int remove_calls;

};




// endif gaurd for .h file
#endif
