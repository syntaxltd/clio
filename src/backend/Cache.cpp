#include <backend/Cache.h>

void
Cache::insert(ripple::uint256 const& key, Blob const& value, uint32_t seq)
{
    auto entry = map_[key];
    // stale insert, do nothing
    if (seq <= entry.recent.seq)
        return;
    entry.old = entry.recent;
    entry.recent = {seq, value};
    if (value.empty())
        pendingDeletes_.push_back(key);
    if (!entry.old.blob.empty())
        pendingSweeps_.push_back(key);
}

std::optional<Blob>
Cache::select(CacheEntry const& entry, uint32_t seq)
{
    if (seq < entry.old.seq)
        return {};
    if (seq < entry.recent.seq && !entry.old.blob.empty())
        return entry.old.blob;
    if (!entry.recent.blob.empty())
        return entry.recent.blob;
    return {};
}
void
Cache::update(
    std::vector<std::pair<ripple::uint256, Blob>> const& blobs,
    uint32_t seq)
{
    std::unique_lock lck{mtx_};
    for (auto const& k : pendingSweeps_)
    {
        auto e = map_[k];
        e.old = {};
    }
    for (auto const& k : pendingDeletes_)
    {
        map_.erase(k);
    }
    for (auto const& b : blobs)
    {
        insert(b.first, b.second, seq);
    }
}
std::optional<std::pair<ripple::uint256, Blob>>
Cache::getSuccessor(ripple::uint256 const& key, uint32_t seq)
{
    ripple::uint256 curKey = key;
    std::shared_lock lck{mtx_};
    while (true)
    {
        auto e = map_.upper_bound(curKey);
        if (e == map_.end())
            return {};
        auto const& entry = e->second;
        auto blob = select(entry, seq);
        if (!blob)
        {
            curKey = e->first;
            continue;
        }
        else
            return {{e->first, *blob}};
    }
}
std::optional<std::pair<ripple::uint256, Blob>>
Cache::getPredecessor(ripple::uint256 const& key, uint32_t seq)
{
    ripple::uint256 curKey = key;
    std::shared_lock lck{mtx_};
    while (true)
    {
        auto e = map_.lower_bound(curKey);
        --e;
        if (e == map_.begin())
            return {};
        auto const& entry = e->second;
        auto blob = select(entry, seq);
        if (!blob)
        {
            curKey = e->first;
            continue;
        }
        else
            return {{e->first, *blob}};
    }
}
std::optional<Blob>
Cache::get(ripple::uint256 const& key, uint32_t seq)
{
    std::shared_lock lck{mtx_};
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    auto const& entry = e->second;
    return select(entry, seq);
}

