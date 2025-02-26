#include "bufmgr.h"
#include "swatdb_exceptions.h"
#include "diskmgr.h"
#include "file.h"
#include "page.h"
#include "catalog.h"
#include "math.h"

/**
 * Frame Class which holds metadata about corresponding Page in the buffer pool.
 */

/**
 * @brief Constructor. Calls resetFrame to reset the frame.
 */
Frame::Frame(){
  this->resetFrame();
}
/**
 * @brief Destructor.
 */
Frame::~Frame(){}

/**
 * @brief Resets the metadata of the frame.
 *
 * @pre None.
 * @post page_id is set to INVALID_PAGE_ID. pin_count is set to 0. valid,
 *    dirty, and ref_bit are all set to false.
 */
void Frame::resetFrame(){
  this->page_id = INVALID_PAGE_ID;
  this->pin_count = 0;
  this->valid = false;
  this->dirty = false;
}

/**
 * @brief Updates the Frame data according to the loaded Page.
 *
 * @pre None.
 * @post Frame data is updated. pin_count is set to 1, page_id is set to
 *    page_id parameter, and valid is set to true.
 *
 * @param page_id PageId of the Page that is loaded to the frame
 */
void Frame::loadFrame(PageId new_pid){
  this->resetFrame();
  this->page_id = new_pid;
  this->pin_count = 1;
  this->valid = true;
}
