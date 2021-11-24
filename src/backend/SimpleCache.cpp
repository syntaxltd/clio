#include <backend/SimpleCache.h>
namespace Backend {

void
SimpleCache::update(std::vector<LedgerObject> const& objs, uint32_t seq)
{
    std::unique_lock lck{mtx_};
    if (seq > sequence_)
        sequence_ = seq;
    for (auto const& obj : objs)
    {
        if (obj.blob.size())
        {
            auto& e = map_[obj.key];
            if (seq > e.seq)
                e = {seq, obj.blob};
        }
        else
            map_.erase(obj.key);
    }
}
std::optional<LedgerObject>
SimpleCache::getSuccessor(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock{mtx_};
    if (seq < sequence_)
        return {};
    auto e = map_.upper_bound(key);
    if (e == map_.end())
        return {};
    return {{e->first, e->second.blob}};
}
std::optional<LedgerObject>
SimpleCache::getPredecessor(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock lck{mtx_};
    if (seq < sequence_)
        return {};
    auto e = map_.lower_bound(key);
    --e;
    if (e == map_.begin())
        return {};
    return {{e->first, e->second.blob}};
}
std::optional<Blob>
SimpleCache::get(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock lck{mtx_};
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    if (seq < e->second.seq)
        return {};
    return {e->second.blob};
}
}  // namespace Backend
