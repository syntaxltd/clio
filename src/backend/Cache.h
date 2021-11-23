#ifndef CLIO_CACHE_H_INCLUDED
#define CLIO_CACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <backend/Types.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>
namespace Backend {
class Cache
{
    struct SeqBlobPair
    {
        uint32_t seq;
        Blob blob;
    };
    struct CacheEntry
    {
        SeqBlobPair recent;
        SeqBlobPair old;
    };

    std::map<ripple::uint256, CacheEntry> map_;
    std::vector<ripple::uint256> pendingDeletes_;
    std::vector<ripple::uint256> pendingSweeps_;
    mutable std::shared_mutex mtx_;

    void
    insert(ripple::uint256 const& key, Blob const& value, uint32_t seq);

    /*
    void
    insert(ripple::uint256 const& key, Blob const& value, uint32_t seq)
    {
        map_.emplace(key,{{seq,value,{}});
    }
    void
    update(ripple::uint256 const& key, Blob const& value, uint32_t seq)
    {
            auto& entry = map_.find(key);
            entry.old = entry.recent;
            entry.recent = {seq, value};
            pendingSweeps_.push_back(key);
    }
    void
    erase(ripple::uint256 const& key, uint32_t seq)
    {
            update(key, {}, seq);
            pendingDeletes_.push_back(key);
    }
    */
    std::optional<Blob>
    select(CacheEntry const& entry, uint32_t seq) const;

public:
    void
    update(std::vector<LedgerObject> const& blobs, uint32_t seq);

    std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const;

    std::optional<std::pair<ripple::uint256, Blob>>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const;

    std::optional<std::pair<ripple::uint256, Blob>>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const;
};

}  // namespace Backend
#endif
