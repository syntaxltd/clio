#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doNFTTx(Context const& context)
{
    auto const maybeTokenID = getNFTID(context.params);
    if (auto const status = std::get_if<Status>(&maybeTokenID); status)
        return *status;
    auto const tokenID = std::get<ripple::uint256>(maybeTokenID);

    auto const maybeResponse = traverseTransactions(
        context,
        [&tokenID](
            std::shared_ptr<Backend::BackendInterface const> const& backend,
            std::uint32_t const limit,
            bool const forward,
            std::optional<Backend::TransactionsCursor> const& cursorIn,
            boost::asio::yield_context& yield)
            -> Backend::TransactionsAndCursor {
            auto const start = std::chrono::system_clock::now();
            auto const txnsAndCursor = backend->fetchNFTTransactions(
                tokenID, limit, forward, cursorIn, yield);
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

    response[JS(nft_id)] = ripple::to_string(tokenID);
    return response;
}

}  // namespace RPC
