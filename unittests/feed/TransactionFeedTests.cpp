//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/Types.h"
#include "feed/FeedBaseTest.h"
#include "feed/impl/TransactionFeed.h"
#include "util/Fixtures.h"
#include "util/MockPrometheus.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/TestObject.h"
#include "util/config/Config.h"
#include "util/prometheus/Gauge.h"
#include "web/interface/ConnectionBase.h"

#include <boost/asio/io_context.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Book.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/TER.h>

#include <memory>

constexpr static auto ACCOUNT1 = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto CURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr static auto ISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr static auto TXNID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

constexpr static auto TRAN_V1 =
    R"({
        "transaction":
        {
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"1",
            "DeliverMax":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date":0
        },
        "meta":
        {
            "AffectedNodes":
            [
                {
                    "ModifiedNode":
                    {
                        "FinalFields":
                        {
                            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                            "Balance":"110"
                        },
                        "LedgerEntryType":"AccountRoot"
                    }
                },
                {
                    "ModifiedNode":
                    {
                        "FinalFields":
                        {
                            "Account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "Balance":"30"
                        },
                        "LedgerEntryType":"AccountRoot"
                    }
                }
            ],
            "TransactionIndex":22,
            "TransactionResult":"tesSUCCESS",
            "delivered_amount":"unavailable"
        },
        "type":"transaction",
        "validated":true,
        "status":"closed",
        "ledger_index":33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";

constexpr static auto TRAN_V2 =
    R"({
        "tx_json":
        {
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "DeliverMax":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "date":0
        },
        "meta":
        {
            "AffectedNodes":
            [
                {
                    "ModifiedNode":{
                    "FinalFields":{
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Balance":"110"
                    },
                    "LedgerEntryType":"AccountRoot"
                    }
                },
                {
                    "ModifiedNode":{
                    "FinalFields":{
                        "Account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Balance":"30"
                    },
                    "LedgerEntryType":"AccountRoot"
                    }
                }
            ],
            "TransactionIndex":22,
            "TransactionResult":"tesSUCCESS",
            "delivered_amount":"unavailable"
        },
        "type":"transaction",
        "validated":true,
        "status":"closed",
        "ledger_index":33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";

using namespace feed::impl;
using namespace util::prometheus;
namespace json = boost::json;

using FeedTransactionTest = FeedBaseTest<TransactionFeed>;

TEST_F(FeedTransactionTest, SubTransactionV1)
{
    testFeedPtr->sub(sessionPtr, 1);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V1));

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubTransactionV2)
{
    testFeedPtr->sub(sessionPtr, 2);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V2));

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubAccountV1)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 1);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);

    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();

    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V1));

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubAccountV2)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);

    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();

    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V2));

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubBothTransactionAndAccount)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 2);
    testFeedPtr->sub(sessionPtr, 2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);

    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();

    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.run();

    EXPECT_EQ(receivedFeedMessage().size(), json::serialize(json::parse(TRAN_V2)).size() * 2);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(receivedFeedMessage().size(), json::serialize(json::parse(TRAN_V2)).size() * 2);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubBookV1)
{
    auto const issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    testFeedPtr->sub(book, sessionPtr, 1);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto metaObj = CreateMetaDataForBookChange(CURRENCY, ISSUER, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    constexpr static auto OrderbookPublish =
        R"({
            "transaction":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount":"1",
                "DeliverMax":"1",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TransactionType":"Payment",
                "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date":0
            },
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "ModifiedNode":
                        {
                            "FinalFields":
                            {
                                "TakerGets":"3",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"1"
                                }
                            },
                            "LedgerEntryType":"Offer",
                            "PreviousFields":{
                                "TakerGets":"1",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"3"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS",
                "delivered_amount":"unavailable"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "engine_result_code":0,
            "engine_result":"tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";

    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(OrderbookPublish));

    cleanReceivedFeed();

    // trigger by offer cancel meta data
    metaObj = CreateMetaDataForCancelOffer(CURRENCY, ISSUER, 22, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    constexpr static auto OrderbookCancelPublish =
        R"({
            "transaction":{
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount":"1",
                "DeliverMax":"1",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TransactionType":"Payment",
                "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date":0
            },
            "meta":{
                "AffectedNodes":
                [
                    {
                        "DeletedNode":
                        {
                            "FinalFields":
                            {
                                "TakerGets":"3",
                                "TakerPays":{
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"1"
                                }
                            },
                            "LedgerEntryType":"Offer"
                        }
                    }
                ],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS",
                "delivered_amount":"unavailable"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "engine_result_code":0,
            "engine_result":"tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";
    ctx.restart();
    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(OrderbookCancelPublish));

    // trigger by offer create meta data
    constexpr static auto OrderbookCreatePublish =
        R"({
            "transaction":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount":"1",
                "DeliverMax":"1",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TransactionType":"Payment",
                "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date":0
            },
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "CreatedNode":
                        {
                            "NewFields":{
                                "TakerGets":"3",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"1"
                                }
                            },
                            "LedgerEntryType":"Offer"
                        }
                    }
                ],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS",
                "delivered_amount":"unavailable"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "engine_result_code":0,
            "engine_result":"tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";
    metaObj = CreateMetaDataForCreateOffer(CURRENCY, ISSUER, 22, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    cleanReceivedFeed();
    ctx.restart();
    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(OrderbookCreatePublish));

    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubBookV2)
{
    auto const issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    testFeedPtr->sub(book, sessionPtr, 2);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto const metaObj = CreateMetaDataForBookChange(CURRENCY, ISSUER, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    constexpr static auto OrderbookPublish =
        R"({
            "tx_json":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "DeliverMax":"1",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TransactionType":"Payment",
                "date":0
            },
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "ModifiedNode":
                        {
                            "FinalFields":
                            {
                                "TakerGets":"3",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"1"
                                }
                            },
                            "LedgerEntryType":"Offer",
                            "PreviousFields":
                            {
                                "TakerGets":"1",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"3"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS",
                "delivered_amount":"unavailable"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "engine_result_code":0,
            "engine_result":"tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";

    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(OrderbookPublish));

    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, TransactionContainsBothAccountsSubed)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 2);

    auto const account2 = GetAccountIDWithString(ACCOUNT2);
    testFeedPtr->sub(account2, sessionPtr, 2);

    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V2));

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V2));

    cleanReceivedFeed();
    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubAccountRepeatWithDifferentVersion)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 1);

    auto const account2 = GetAccountIDWithString(ACCOUNT2);
    testFeedPtr->sub(account2, sessionPtr, 2);

    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V2));

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V2));

    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    cleanReceivedFeed();
    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubTransactionRepeatWithDifferentVersion)
{
    // sub version 1 first
    testFeedPtr->sub(sessionPtr, 1);
    // sub version 2 later
    testFeedPtr->sub(sessionPtr, 2);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT1, ACCOUNT2, 110, 30, 22).getSerializer().peekData();

    testFeedPtr->pub(trans1, ledgerinfo, backend);

    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_V1));

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);
    cleanReceivedFeed();

    testFeedPtr->pub(trans1, ledgerinfo, backend);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedTransactionTest, SubRepeat)
{
    auto const session2 = std::make_shared<MockSession>(tagDecoratorFactory);

    testFeedPtr->sub(sessionPtr, 1);
    testFeedPtr->sub(session2, 1);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 2);

    testFeedPtr->sub(sessionPtr, 1);
    testFeedPtr->sub(session2, 1);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 2);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);
    testFeedPtr->unsub(session2);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    auto const account = GetAccountIDWithString(ACCOUNT1);
    auto const account2 = GetAccountIDWithString(ACCOUNT2);
    testFeedPtr->sub(account, sessionPtr, 1);
    testFeedPtr->sub(account2, session2, 1);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    testFeedPtr->sub(account, sessionPtr, 1);
    testFeedPtr->sub(account2, session2, 1);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    testFeedPtr->unsub(account2, session2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    auto const issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    testFeedPtr->sub(book, sessionPtr, 1);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);
    testFeedPtr->sub(book, session2, 1);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 2);

    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);
    testFeedPtr->unsub(book, session2);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);
    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);
}

TEST_F(FeedTransactionTest, PubTransactionWithOwnerFund)
{
    testFeedPtr->sub(sessionPtr, 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreateCreateOfferTransactionObject(ACCOUNT1, 1, 32, CURRENCY, ISSUER, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray const metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    ripple::STObject line(ripple::sfIndexes);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(10, false));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(100, false));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{TXNID});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(ripple::sfFlags, 0);
    auto const issue2 = GetIssue(CURRENCY, ISSUER);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(issue2, 100));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);
    auto const issueAccount = GetAccountIDWithString(ISSUER);
    auto const kk = ripple::keylet::account(issueAccount).key;
    ON_CALL(*backend, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Return(line.getSerializer().peekData()));
    ripple::STObject const accountRoot = CreateAccountRootObject(ISSUER, 0, 1, 10, 2, TXNID, 3);
    ON_CALL(*backend, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(testing::Return(accountRoot.getSerializer().peekData()));

    testFeedPtr->pub(trans1, ledgerinfo, backend);
    constexpr static auto TransactionForOwnerFund =
        R"({
            "transaction":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TakerGets":
                {
                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                    "value":"1"
                },
                "TakerPays":"3",
                "TransactionType":"OfferCreate",
                "hash":"EE8775B43A67F4803DECEC5E918E0EA9C56D8ED93E512EBE9F2891846509AAAB",
                "date":0,
                "owner_funds":"100"
            },
            "meta":
            {
                "AffectedNodes":[],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "engine_result_code":0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result":"tesSUCCESS",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";

    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TransactionForOwnerFund));
}

constexpr static auto TRAN_FROZEN =
    R"({
        "transaction":
        {
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TakerGets":
            {
                "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                "value":"1"
            },
            "TakerPays":"3",
            "TransactionType":"OfferCreate",
            "hash":"EE8775B43A67F4803DECEC5E918E0EA9C56D8ED93E512EBE9F2891846509AAAB",
            "date":0,
            "owner_funds":"0"
        },
        "meta":{
            "AffectedNodes":[],
            "TransactionIndex":22,
            "TransactionResult":"tesSUCCESS"
        },
        "type":"transaction",
        "validated":true,
        "status":"closed",
        "ledger_index":33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";

TEST_F(FeedTransactionTest, PubTransactionOfferCreationFrozenLine)
{
    testFeedPtr->sub(sessionPtr, 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreateCreateOfferTransactionObject(ACCOUNT1, 1, 32, CURRENCY, ISSUER, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray const metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    ripple::STObject line(ripple::sfIndexes);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(10, false));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(100, false));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{TXNID});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(ripple::sfFlags, ripple::lsfHighFreeze);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(GetIssue(CURRENCY, ISSUER), 100));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(3);
    auto const issueAccount = GetAccountIDWithString(ISSUER);
    auto const kk = ripple::keylet::account(issueAccount).key;
    ON_CALL(*backend, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Return(line.getSerializer().peekData()));
    ripple::STObject const accountRoot = CreateAccountRootObject(ISSUER, 0, 1, 10, 2, TXNID, 3);
    ON_CALL(*backend, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(testing::Return(accountRoot.getSerializer().peekData()));
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_FROZEN));
}

TEST_F(FeedTransactionTest, SubTransactionOfferCreationGlobalFrozen)
{
    testFeedPtr->sub(sessionPtr, 1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = CreateCreateOfferTransactionObject(ACCOUNT1, 1, 32, CURRENCY, ISSUER, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray const metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    ripple::STObject line(ripple::sfIndexes);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(10, false));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(100, false));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{TXNID});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(ripple::sfFlags, ripple::lsfHighFreeze);
    auto const issueAccount = GetAccountIDWithString(ISSUER);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(GetIssue(CURRENCY, ISSUER), 100));

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(2);
    auto const kk = ripple::keylet::account(issueAccount).key;
    ON_CALL(*backend, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Return(line.getSerializer().peekData()));
    ripple::STObject const accountRoot = CreateAccountRootObject(ISSUER, ripple::lsfGlobalFreeze, 1, 10, 2, TXNID, 3);
    ON_CALL(*backend, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(testing::Return(accountRoot.getSerializer().peekData()));
    testFeedPtr->pub(trans1, ledgerinfo, backend);

    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(TRAN_FROZEN));
}

struct TransactionFeedMockPrometheusTest : WithMockPrometheus, SyncAsioContextTest {
protected:
    util::TagDecoratorFactory tagDecoratorFactory{util::Config{}};
    std::shared_ptr<web::ConnectionBase> sessionPtr;
    std::shared_ptr<TransactionFeed> testFeedPtr;

    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
        testFeedPtr = std::make_shared<TransactionFeed>(ctx);
        sessionPtr = std::make_shared<MockSession>(tagDecoratorFactory);
    }
    void
    TearDown() override
    {
        sessionPtr.reset();
        testFeedPtr.reset();
        SyncAsioContextTest::TearDown();
    }
};

TEST_F(TransactionFeedMockPrometheusTest, subUnsub)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx\"}");
    auto& counterAccount = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account\"}");
    auto& counterBook = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"book\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));
    EXPECT_CALL(counterBook, add(1));
    EXPECT_CALL(counterBook, add(-1));

    testFeedPtr->sub(sessionPtr, 1);
    testFeedPtr->unsub(sessionPtr);

    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 1);
    testFeedPtr->unsub(account, sessionPtr);

    auto const issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    testFeedPtr->sub(book, sessionPtr, 1);
    testFeedPtr->unsub(book, sessionPtr);
}

TEST_F(TransactionFeedMockPrometheusTest, AutoDisconnect)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx\"}");
    auto& counterAccount = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account\"}");
    auto& counterBook = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"book\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));
    EXPECT_CALL(counterBook, add(1));
    EXPECT_CALL(counterBook, add(-1));

    testFeedPtr->sub(sessionPtr, 1);

    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr, 1);

    auto const issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    testFeedPtr->sub(book, sessionPtr, 1);

    sessionPtr.reset();
}
