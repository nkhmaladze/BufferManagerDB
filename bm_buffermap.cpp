/**
 * @file bm_buffermap.cpp
 * @author Nikoloz Khmaladze & Omar Ebied
 * @date 2025-02-09
 * 
 * Implementation of the BufferMap class.
 * This file implements the BufferMap class, a wrapper around
 * std::unordered_map<PageId, FrameId>` that provides a specialized
 * interface for mapping PageIds to Frame indices within the buffer pool.
 * It offers methods like `get`, `contains`, `insert`, and `remove` for
 * managing the mapping, and includes error handling for cases where PageIds
 * are not found or already present.
 */

#include "bufmgr.h"
#include "swatdb_exceptions.h"
#include "diskmgr.h"
#include "file.h"
#include "page.h"
#include "catalog.h"
#include "math.h"

#include <iostream>
#include <string>
#include <unordered_map>

/**
 * BufferMap is a wrapper class for std::unordered_map<PageId, FrameId> that
 * maps PageIds to a Frame index in the buffer pool. Has different method names
 * from std::unordered_map. Have get(), contains(), insert(), and remove()
 * methods.
 */

/**
 * @brief Returns FrameId corresponding to the given PageId.
 *
 * @pre A lock is held on the map. This map contains the given PageId.
 * @post Returns the corresponding FrameId. Lock is still held on the map.
 *
 * @param page_id A PageId for which the corresponding FrameId will be
 *    returned.
 * @return FrameId which correspond to the given PageId.
 *
 * @throw PageNotFoundBufMgr if the given PageId is not in the map.
 */
FrameId BufferMap::get(PageId page_id){

  std::unordered_map<PageId, unsigned int, BufHash>::iterator it = buf_map.find(page_id);

  if (it == buf_map.end()){
    throw PageNotFoundBufMgr(page_id);
  }
  return it->second;

}

/**
 * @brief Returns true if the map contains the given PageId, else false.
 *
 * @pre A lock is held on the map. A PageId is provided.
 * @post Returns true if the map contains the PageId, else false. Lock is
 *    still held on the map.
 *
 * @param page_id A PageId to be searched in the map.
 * @return bool indicheck_eqcating whetther the given PageId exists in the
 *    BufferMap.
 */
bool BufferMap::contains(PageId page_id){

  if (buf_map.find(page_id) == buf_map.end()){
    return false;
  }
  return true;
  
}

/**
 * @brief Inserts the pair <page_id, frame_id>> into the map.
 *
 * @pre A lock is held on the map. A PageId and FrameId are provided as
 *    input.
 * @post Adds a new <PageId, FrameId> pair to the map. If the map
 *    contains page_id, then it throws an exception. Lock is still held
 *    on the map.
 *
 * @param page_id A PageId key.
 * @param frame_id A FrameId value.
 * @throw PageAlreadyLoadedBufMgr exeception
 */
void BufferMap::insert(PageId page_id, FrameId frame_id){

  if (contains(page_id)){
    throw PageAlreadyLoadedBufMgr(page_id); 
  }
  buf_map[page_id] = frame_id;

}

/**
 * @brief Removes the key-value pair corresponding to the given PageId
 *    from the map.
 *
 * @pre A lock is held on the map. The given PageId is in the map.
 * @post The key-value pair searched by page_id is removed from the map.
 *
 * @param page_id The PageId key for the key-value pair to be removed.
 *
 * @throw PageIdNotFoundBufMgr If page_id is not in the map.
 */
void BufferMap::remove(PageId page_id){

  if (!contains(page_id)){
    throw PageNotFoundBufMgr(page_id); 
  }
  buf_map.erase(page_id);

}


