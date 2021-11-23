#ifndef RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDINTERFACE_H_INCLUDED
#include <ripple/ledger/ReadView.h>
#include <boost/asio.hpp>
#include <backend/Cache.h>
#include <backend/DBHelpers.h>
#include <backend/Types.h>
class ReportingETL;
class AsyncCallData;
/*
class PlainETLSource;
class SslETLSource;

template <class T>
class ETLSourceImpl;

template <>
class ETLSourceImpl<PlainETLSource>;

template <>
class ETLSourceImpl<SslETLSource>;
*/
class BackendTest_Basic_Test;
namespace Backend {

class DatabaseTimeout : public std::exception
{
    const char*
    what() const throw() override
    {
        return "Database read timed out. Please retry the request";
    }
};

class BackendInterface
{
protected:
    mutable bool isFirst_ = true;
    mutable std::optional<LedgerRange> range;
    mutable Cache cache_;

public:
    BackendInterface(boost::json::object const& config)
    {
    }
    virtual ~BackendInterface()
    {
    }

    // *** public read methods ***
    // All of these reads methods can throw DatabaseTimeout. When writing code
    // in an RPC handler, this exception does not need to be caught: when an RPC
    // results in a timeout, an error is returned to the client
public:
    // *** ledger methods
    //

    Cache const&
    cache()
    {
        return cache_;
    }

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerBySequence(uint32_t sequence) const = 0;

    virtual std::optional<ripple::LedgerInfo>
    fetchLedgerByHash(ripple::uint256 const& hash) const = 0;

    virtual std::optional<uint32_t>
    fetchLatestLedgerSequence() const = 0;

    std::optional<LedgerRange>
    fetchLedgerRange() const
    {
        return range;
    }

    std::optional<ripple::Fees>
    fetchFees(std::uint32_t seq) const;

    // *** transaction methods
    virtual std::optional<TransactionAndMetadata>
    fetchTransaction(ripple::uint256 const& hash) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchTransactions(std::vector<ripple::uint256> const& hashes) const = 0;

    virtual AccountTransactions
    fetchAccountTransactions(
        ripple::AccountID const& account,
        std::uint32_t limit,
        bool forward = false,
        std::optional<AccountTransactionsCursor> const& cursor = {}) const = 0;

    virtual std::vector<TransactionAndMetadata>
    fetchAllTransactionsInLedger(uint32_t ledgerSequence) const = 0;

    virtual std::vector<ripple::uint256>
    fetchAllTransactionHashesInLedger(uint32_t ledgerSequence) const = 0;

    // *** state data methods

    virtual std::optional<Blob>
    fetchLedgerObject(ripple::uint256 const& key, uint32_t sequence) const = 0;

    virtual std::vector<Blob>
    fetchLedgerObjects(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const = 0;

    virtual std::vector<LedgerObject>
    fetchLedgerDiff(uint32_t ledgerSequence) const = 0;

    // Fetches a page of ledger objects, ordered by key/index.
    // Used by ledger_data
    LedgerPage
    fetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit,
        std::uint32_t limitHint = 0) const;

    // Fetches the successor to key/index
    virtual std::optional<LedgerObject>
    fetchSuccessor(ripple::uint256 key, uint32_t ledgerSequence) const = 0;

    BookOffersPage
    fetchBookOffers(
        ripple::uint256 const& book,
        uint32_t ledgerSequence,
        std::uint32_t limit,
        std::optional<ripple::uint256> const& cursor = {}) const;

    virtual void
    writeSuccessor(std::string&& key, uint32_t seq, std::string&& successor)
        const = 0;

    // *** protected write methods
protected:
    friend class ::ReportingETL;
    friend class BackendIndexer;
    friend class ::AsyncCallData;

    friend std::shared_ptr<BackendInterface>
    make_Backend(boost::json::object const& config);
    friend class ::BackendTest_Basic_Test;
    virtual std::optional<LedgerRange>
    hardFetchLedgerRange() const = 0;
    // Doesn't throw DatabaseTimeout. Should be used with care.
    std::optional<LedgerRange>
    hardFetchLedgerRangeNoThrow() const;

    void
    updateRange(uint32_t newMax)
    {
        if (!range)
            range = {newMax, newMax};
        else
            range->maxSequence = newMax;
    }

    void
    updateCache(std::vector<LedgerObject> const& updates, uint32_t seq) const;

    virtual void
    writeLedger(
        ripple::LedgerInfo const& ledgerInfo,
        std::string&& ledgerHeader,
        bool isFirst = false) const = 0;

    void
    writeLedgerObject(std::string&& key, uint32_t seq, std::string&& blob)
        const;

    virtual void
    writeTransaction(
        std::string&& hash,
        uint32_t seq,
        uint32_t date,
        std::string&& transaction,
        std::string&& metadata) const = 0;

    virtual void
    writeAccountTransactions(
        std::vector<AccountTransactionsData>&& data) const = 0;

    // Tell the database we are about to begin writing data for a particular
    // ledger.
    virtual void
    startWrites() const = 0;

    // Tell the database we have finished writing all data for a particular
    // ledger
    bool
    finishWrites(uint32_t ledgerSequence);

    virtual bool
    doOnlineDelete(uint32_t numLedgersToKeep) const = 0;

    // Open the database. Set up all of the necessary objects and
    // datastructures. After this call completes, the database is ready for
    // use.
    virtual void
    open(bool readOnly) = 0;

    // Close the database, releasing any resources
    virtual void
    close() = 0;

    // *** private helper methods
private:
    virtual LedgerPage
    doFetchLedgerPage(
        std::optional<ripple::uint256> const& cursor,
        std::uint32_t ledgerSequence,
        std::uint32_t limit) const = 0;

    virtual void
    doWriteLedgerObject(std::string&& key, uint32_t seq, std::string&& blob)
        const = 0;

    virtual bool
    doFinishWrites() const = 0;
};

}  // namespace Backend
using BackendInterface = Backend::BackendInterface;
#endif
