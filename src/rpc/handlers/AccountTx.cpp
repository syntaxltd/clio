#include <backend/BackendInterface.h>
#include <backend/Pg.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doAccountTx(Context const& context)
{
    auto request = context.params;

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
        return status;

    auto const maybeResponse = traverseTransactions(
        context,
        [&accountID](
            std::shared_ptr<Backend::BackendInterface const> const& backend,
            std::uint32_t const limit,
            bool const forward,
            std::optional<Backend::TransactionsCursor> const& cursorIn,
            boost::asio::yield_context& yield) {
            auto const start = std::chrono::system_clock::now();
            auto const txnsAndCursor = backend->fetchAccountTransactions(
                accountID, limit, forward, cursorIn, yield);
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " db fetch took "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now() - start)
                       .count()
                << " num blobs = " << txnsAndCursor.txns.size();
            return txnsAndCursor;
        });

    if (auto const status = std::get_if<Status>(&maybeResponse); status)
        return *status;
    auto response = std::get<boost::json::object>(maybeResponse);

    response[JS(account)] = ripple::to_string(accountID);
    return response;
}

}  // namespace RPC
