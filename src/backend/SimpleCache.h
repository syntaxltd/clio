#ifndef CLIO_SIMPLECACHE_H_INCLUDED
#define CLIO_SIMPLECACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <backend/Types.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>
namespace Backend {
class SimpleCache
{
    struct CacheEntry
    {
        uint32_t seq = 0;
        Blob blob;
    };
    std::map<ripple::uint256, CacheEntry> map_;
    mutable std::shared_mutex mtx_;
    uint32_t latestSeq_ = 0;
    std::atomic_bool enabled_ = true;
    std::atomic_bool full_ = false;

public:
    void
    update(std::vector<LedgerObject> const& blobs, uint32_t seq);

    std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const;

    std::optional<LedgerObject>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const;

    std::optional<LedgerObject>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const;

    void
    setFull();

    void
    disable();

    void
    enable();

    bool
    isEnabled();

    bool
    isFull();

    size_t
    size();
};

}  // namespace Backend
#endif
