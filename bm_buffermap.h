#ifndef _SWATDB_BM_BUFFERMAP_H_
#define  _SWATDB_BM_BUFFERMAP_H_

/**
 * \file bm_buffermap.h
 */

#include <utility>
#include <unordered_map>

#include "swatdb_types.h"

/**
 * Hash function for BufferMap.
 */
struct BufHash{
  std::size_t operator()(const PageId& page_id) const{
    return (std::hash<unsigned int>() (page_id.file_id)) ^
        (std::hash<unsigned int>() (page_id.page_num));
  }
};

/**
 * BufferMap is a wrapper class for std::unordered_map<PageId, FrameId> that
 * maps PageIds to a Frame index in the buffer pool. Has different method names
 * from std::unordered_map. Have get(), contains(), insert(), and remove()
 * methods.
 */
class BufferMap {

  public:

    /**
     * @brief Constructor for BufferMap.
     */
    BufferMap(){ }

    /**
     * @brief Destructor for BufferMap.
     */
    ~BufferMap(){ }

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
    FrameId get(PageId page_id);

    /**
     * @brief Returns true if the map contains the given PageId, else false.
     *
     * @pre A lock is held on the map. A PageId is provided.
     * @post Returns true if the map contains the PageId, else false. Lock is
     *    still held on the map.
     *
     * @param page_id A PageId to be searched in the map.
     * @return bool indicating whether the given PageId exists in the
     *    BufferMap.
     */
    bool contains(PageId page_id);

    /**
     * @brief Inserts the pair <page_id, frame_id> into the map.
     *
     * @pre A lock is held on the map. A PageId and FrameId are provided as
     *    input.
     * @post Adds a new <PageId, FrameId> pair to the map. If the map
     *    contains page_id, then it throws an exception. Lock is still held on
     * the map.
     *
     * @param page_id A PageId key.
     * @param frame_id A FrameId value.
     *
     * @throw throw PageAlreadyLoadedBufMgr if a page with this PageId
     * is already in the Buffer Pool
     */
    void insert(PageId page_id, FrameId frame_id);

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
    void remove(PageId page_id);

  private:

    /**
     * The underlying std::unordered_map for storing the map information
     */
    std::unordered_map<PageId,FrameId,BufHash> buf_map;
};

#endif
