#ifndef _SWATDB_BM_FRAME_H_
#define  _SWATDB_BM_FRAME_H_

/**
 * \file bm_frame.h: stores metadata associated with each Frame of the 
 *                   Buffer Pool
 */


#include <utility>
#include <mutex>
#include "swatdb_types.h"


/**
 * Frame Class which holds metadata about corresponding Page in the buffer pool.
 */
class Frame {

  /**
   * BufferManager and Policies have access to private data members of each Frame.
   * They need direct access to Frame data fields to support 
   * synchronizing access to frames by some higher-level operations
   *  (getters/setters alone won't do it)
   */
  friend class BufferManager;
  friend class ReplacementPolicy;
  friend class Clock;
  friend class Random;



  public:

    /**
     * @brief Constructor. Calls resetFrame to reset the Frame.
     */
    Frame();

    /**
     * @brief Destructor.
     */
    ~Frame();

    /**
     * @brief Resets the metadata of the Frame.
     *
     * @pre None.
     * @post page_id is set to INVALID_PAGE_ID. pin_count is set to 0. valid,
     *    dirty, and ref_bit are all set to false.
     */
    void resetFrame();

    /**
     * @brief Updates the Frame data according to the loaded Page.
     *
     * @pre None.
     * @post Frame data is updated. pin_count is set to 1, page_id is set to
     *    page_id parameter, and valid is set to true.
     *
     * @param page_id PageId of the Page that is loaded to the Frame
     */
    void loadFrame(PageId page_id);

  private:

    /**
     * PageId of the Page in the Frame.
     * Recall PageId is made up of {FileId, PageNum}.
     */
    PageId page_id;

    /**
     * The number of pins on the Page.
     */
    int pin_count;

    /**
     * true if the Frame data is valid. Else false.
     */
    bool valid;

    /**
     * true if the Frame is dirty. Else false.
     */
    bool dirty;

};

#endif
