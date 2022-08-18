#include <rpc/RPCHelpers.h>

// {
//   nft_id: <ident>
//   ledger_hash: <ledger>
//   ledger_index: <ledger_index>
//   ledger_index_min: <OPTIONAL -1 or ledger_index>
//   ledger_index_max: <OPTIONAL -1 or ledger_index>
//   binary: <OPTIONAL bool>
//   forward: <OPTIONAL bool>
//   limit: <OPTIONAL integer>
//   marker: <OPTIONAL marker>
// }

namespace RPC {

Result
doNFTTx(Context const& context)
{
    auto const maybeTokenID = getNFTID(context.params);
    if (auto const status = std::get_if<Status>(&maybeTokenID))
        return *status;
    auto const tokenID = std::get<ripple::uint256>(maybeTokenID);

    auto const maybeResponse = traverseTransactions(
        context,
        [&tokenID, &context](
            std::uint32_t const limit,
            bool const forward,
            std::optional<Backend::TransactionsCursor> const& cursorIn)
            -> Backend::TransactionsAndCursor {
            auto const start = std::chrono::system_clock::now();
            auto const txnsAndCursor = context.backend->fetchNFTTransactions(
                tokenID, limit, forward, cursorIn, context.yield);
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " db fetch took "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now() - start)
                       .count()
                << " num blobs = " << txnsAndCursor.txns.size();
            return txnsAndCursor;
        });

    if (auto const status = std::get_if<Status>(&maybeResponse))
        return *status;
    auto response = std::get<boost::json::object>(maybeResponse);

    response[JS(nft_id)] = ripple::to_string(tokenID);
    return response;
}

}  // namespace RPC
