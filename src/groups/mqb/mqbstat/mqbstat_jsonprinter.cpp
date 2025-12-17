// Copyright 2024 Bloomberg Finance L.P.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// mqbstat_jsonprinter.cpp                                            -*-C++-*-
#include <mqbstat_jsonprinter.h>

#include <mqbscm_version.h>

// MQB
#include <mqbstat_queuestats.h>

#include <bmqio_statchannel.h>
#include <bmqio_statchannelfactory.h>
#include <bmqu_memoutstream.h>

// BDE
#include <ball_log.h>
#include <bdljsn_json.h>
#include <bdljsn_jsonutil.h>
#include <bdlt_iso8601util.h>
#include <bsls_assert.h>

namespace BloombergLP {
namespace mqbstat {

namespace {

struct DomainsStatsPrintUtils {
    // PUBLIC CLASS METHODS

    /// "domainQueues" stat context:
    /// Populate the specified `bdljsn::JsonObject*` with the values
    /// from the specified `ctx`.

    inline static void
    populateMetric(bdljsn::JsonObject*                   metricsObject,
                   const bmqst::StatContext&             ctx,
                   mqbstat::QueueStatsDomain::Stat::Enum metric)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(metricsObject);

        const bsls::Types::Int64 value =
            mqbstat::QueueStatsDomain::getValue(ctx, -1, metric);

        (*metricsObject)[mqbstat::QueueStatsDomain::Stat::toString(metric)]
            .makeNumber() = value;
    }

    inline static void
    populateQueueStatsDomainMetrics(bsl::ostream&             os,
                                    const bdljsn::Json&       parent,
                                    const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS

        if (ctx.numValues() == 0) {
            // Prefer to omit an empty "values" object
            return;  // RETURN
        }

        bdljsn::Json        json(parent, parent.allocator());
        bdljsn::JsonObject& values = json.theObject();

        typedef mqbstat::QueueStatsDomain::Stat Stat;

        populateMetric(&values, ctx, Stat::e_NB_PRODUCER);
        populateMetric(&values, ctx, Stat::e_NB_CONSUMER);

        populateMetric(&values, ctx, Stat::e_MESSAGES_CURRENT);
        populateMetric(&values, ctx, Stat::e_MESSAGES_MAX);
        populateMetric(&values, ctx, Stat::e_MESSAGES_UTILIZATION_MAX);
        populateMetric(&values, ctx, Stat::e_BYTES_CURRENT);
        populateMetric(&values, ctx, Stat::e_BYTES_MAX);
        populateMetric(&values, ctx, Stat::e_BYTES_UTILIZATION_MAX);

        populateMetric(&values, ctx, Stat::e_PUT_MESSAGES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUT_BYTES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUT_MESSAGES_ABS);
        populateMetric(&values, ctx, Stat::e_PUT_BYTES_ABS);

        populateMetric(&values, ctx, Stat::e_PUSH_MESSAGES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUSH_BYTES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUSH_MESSAGES_ABS);
        populateMetric(&values, ctx, Stat::e_PUSH_BYTES_ABS);

        populateMetric(&values, ctx, Stat::e_ACK_DELTA);
        populateMetric(&values, ctx, Stat::e_ACK_ABS);
        populateMetric(&values, ctx, Stat::e_ACK_TIME_AVG);
        populateMetric(&values, ctx, Stat::e_ACK_TIME_MAX);

        populateMetric(&values, ctx, Stat::e_NACK_DELTA);
        populateMetric(&values, ctx, Stat::e_NACK_ABS);

        populateMetric(&values, ctx, Stat::e_CONFIRM_DELTA);
        populateMetric(&values, ctx, Stat::e_CONFIRM_ABS);
        populateMetric(&values, ctx, Stat::e_CONFIRM_TIME_AVG);
        populateMetric(&values, ctx, Stat::e_CONFIRM_TIME_MAX);

        populateMetric(&values, ctx, Stat::e_REJECT_ABS);
        populateMetric(&values, ctx, Stat::e_REJECT_DELTA);

        populateMetric(&values, ctx, Stat::e_QUEUE_TIME_AVG);
        populateMetric(&values, ctx, Stat::e_QUEUE_TIME_MAX);

        populateMetric(&values, ctx, Stat::e_GC_MSGS_DELTA);
        populateMetric(&values, ctx, Stat::e_GC_MSGS_ABS);

        populateMetric(&values, ctx, Stat::e_ROLE);

        populateMetric(&values, ctx, Stat::e_CFG_MSGS);
        populateMetric(&values, ctx, Stat::e_CFG_BYTES);

        populateMetric(&values, ctx, Stat::e_NO_SC_MSGS_DELTA);
        populateMetric(&values, ctx, Stat::e_NO_SC_MSGS_ABS);

        populateMetric(&values, ctx, Stat::e_HISTORY_ABS);

        bdljsn::WriteOptions opts = bdljsn::WriteOptions()
                                        .setSpacesPerLevel(2)
                                        .setStyle(
                                            bdljsn::WriteStyle::e_COMPACT)
                                        .setSortMembers(false);
        const int rc = bdljsn::JsonUtil::write(os, json, opts);
        BSLS_ASSERT_SAFE(0 == rc);
        os << bsl::endl;
    }

    inline static void populateOne(bsl::ostream&             os,
                                   const bdljsn::Json&       parent,
                                   const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS

        for (bmqst::StatContextIterator queueIt = ctx.subcontextIterator();
             queueIt;
             ++queueIt) {
            bdljsn::Json json(parent, parent.allocator());
            json.theObject().insert("queue_name", queueIt->name());

            if (queueIt->numSubcontexts() > 0) {
                // Add metrics per appId, if any
                for (bmqst::StatContextIterator appIdIt =
                         queueIt->subcontextIterator();
                     appIdIt;
                     ++appIdIt) {
                    // Do not expect another nested StatContext within appId
                    BSLS_ASSERT_SAFE(0 == appIdIt->numSubcontexts());

                    bdljsn::Json jsonApp(parent, parent.allocator());
                    jsonApp.theObject().insert("app_id", appIdIt->name());

                    populateQueueStatsDomainMetrics(os, jsonApp, *appIdIt);
                }
            }
            else {
                populateQueueStatsDomainMetrics(os, json, *queueIt);
            }
        }
    }

    inline static void populateAll(bsl::ostream&             os,
                                   const bdljsn::Json&       parent,
                                   const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        bdljsn::Json json(parent, parent.allocator());

        for (bmqst::StatContextIterator domainIt = ctx.subcontextIterator();
             domainIt;
             ++domainIt) {
            json.theObject().insert("domain_name",
                                    bdljsn::Json(domainIt->name(),
                                                 parent.allocator()));
            populateOne(os, json, *domainIt);
        }
    }
};

struct ClientStatsConversionUtils {
    // PUBLIC CLASS METHODS

    /// "clients" stat context:
    /// Populate the specified `bdljsn::JsonObject*` with the values
    /// from the specified `ctx`.

    inline static void
    populateMetric(bdljsn::JsonObject*                   metricsObject,
                   const bmqst::StatContext&             ctx,
                   mqbstat::QueueStatsClient::Stat::Enum metric)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(metricsObject);

        const bsls::Types::Int64 value =
            mqbstat::QueueStatsClient::getValue(ctx, -1, metric);

        (*metricsObject)[mqbstat::QueueStatsClient::Stat::toString(metric)]
            .makeNumber() = value;
    }

    inline static void
    populateQueueStatsMetrics(bdljsn::JsonObject*       queueObject,
                              const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(queueObject);

        if (ctx.numValues() == 0) {
            // Prefer to omit an empty "values" object
            return;  // RETURN
        }

        bdljsn::JsonObject& values = (*queueObject)["values"].makeObject();

        typedef mqbstat::QueueStatsClient::Stat Stat;

        populateMetric(&values, ctx, Stat::e_PUT_MESSAGES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUT_BYTES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUT_MESSAGES_ABS);
        populateMetric(&values, ctx, Stat::e_PUT_BYTES_ABS);

        populateMetric(&values, ctx, Stat::e_PUSH_MESSAGES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUSH_BYTES_DELTA);
        populateMetric(&values, ctx, Stat::e_PUSH_MESSAGES_ABS);
        populateMetric(&values, ctx, Stat::e_PUSH_BYTES_ABS);

        populateMetric(&values, ctx, Stat::e_ACK_DELTA);
        populateMetric(&values, ctx, Stat::e_ACK_ABS);
        populateMetric(&values, ctx, Stat::e_CONFIRM_DELTA);
        populateMetric(&values, ctx, Stat::e_CONFIRM_ABS);
    }

    inline static void populateOne(bdljsn::JsonObject*       clientObject,
                                   const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(clientObject);
        for (bmqst::StatContextIterator queueIt = ctx.subcontextIterator();
             queueIt;
             ++queueIt) {
            bdljsn::JsonObject& queueObj =
                (*clientObject)[queueIt->name()].makeObject();
            populateQueueStatsMetrics(&queueObj, *queueIt);

            if (queueIt->numSubcontexts() > 0) {
                bdljsn::JsonObject& appIdsObject =
                    queueObj["appIds"].makeObject();

                // Add metrics per appId, if any
                for (bmqst::StatContextIterator appIdIt =
                         queueIt->subcontextIterator();
                     appIdIt;
                     ++appIdIt) {
                    // Do not expect another nested StatContext within appId
                    BSLS_ASSERT_SAFE(0 == appIdIt->numSubcontexts());

                    populateQueueStatsMetrics(
                        &appIdsObject[appIdIt->name()].makeObject(),
                        *appIdIt);
                }
            }
        }
    }

    inline static void populateAll(bdljsn::JsonObject*       parent,
                                   const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(parent);

        for (bmqst::StatContextIterator clientIt = ctx.subcontextIterator();
             clientIt;
             ++clientIt) {
            // Populate client metrics
            populateOne(&(*parent)[clientIt->name()].makeObject(), *clientIt);
        }
    }
};

struct ChannelStatsConversionUtils {
    // PUBLIC CLASS METHODS

    /// "channels" stat context:
    /// Populate the specified `bdljsn::JsonObject*` with the values
    /// from the specified `ctx`.

    inline static void
    populateMetric(bdljsn::JsonObject*                       obj,
                   const bmqst::StatContext&                 ctx,
                   bmqio::StatChannelFactoryUtil::Stat::Enum metric)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(obj);

        const bsls::Types::Int64 value =
            bmqio::StatChannelFactoryUtil::getValue(ctx, -1, metric);

        (*obj)[bmqio::StatChannelFactoryUtil::Stat::toString(metric)]
            .makeNumber() = value;
    }

    inline static void populatePortMetrics(bdljsn::JsonObject*       xObject,
                                           const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(xObject);
        if (ctx.numValues() == 0) {
            // Prefer to omit an empty "values" object
            return;  // RETURN
        }

        bdljsn::JsonObject& values = (*xObject)["values"].makeObject();

        typedef bmqio::StatChannelFactoryUtil::Stat Stat;

        populateMetric(&values, ctx, Stat::e_BYTES_IN_DELTA);
        populateMetric(&values, ctx, Stat::e_BYTES_IN_ABS);
        populateMetric(&values, ctx, Stat::e_BYTES_OUT_DELTA);
        populateMetric(&values, ctx, Stat::e_BYTES_OUT_ABS);
        populateMetric(&values, ctx, Stat::e_CONNECTIONS_DELTA);
        populateMetric(&values, ctx, Stat::e_CONNECTIONS_ABS);
    }

    inline static void populatePort(bdljsn::JsonObject*       portObject,
                                    const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(portObject);

        for (bmqst::StatContextIterator xIt = ctx.subcontextIterator(); xIt;
             ++xIt) {
            // Populate channel metrics (e.g. 127.0.0.1~localhost:36160)
            bdljsn::JsonObject& xObj = (*portObject)[xIt->name()].makeObject();

            populatePortMetrics(&xObj, *xIt);
        }
    }

    inline static void populateOne(bdljsn::JsonObject*       channelObject,
                                   const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(channelObject);
        for (bmqst::StatContextIterator portIt = ctx.subcontextIterator();
             portIt;
             ++portIt) {
            // Populate port metrics (e.g. 36160).
            // Port identifier is integer, thus need to convert to string.
            char portItName[64];
            sprintf(portItName, "%lld", portIt->id());

            bdljsn::JsonObject& portObj =
                (*channelObject)[portItName].makeObject();
            populatePort(&portObj, *portIt);
        }
    }

    inline static void populateAll(bdljsn::JsonObject*       parent,
                                   const bmqst::StatContext& ctx)
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(parent);

        for (bmqst::StatContextIterator channelIt = ctx.subcontextIterator();
             channelIt;
             ++channelIt) {
            // Populate channel metrics (e.g. remote/local)
            populateOne(&(*parent)[channelIt->name()].makeObject(),
                        *channelIt);
        }
    }
};

}  // close unnamed namespace

// ----------------------------------
// class JsonPrinter::JsonPrinterImpl
// ----------------------------------

/// The implementation class for JsonPrinter, containing all the cached options
/// for printing statistics as JSON.  This implementation exists and is hidden
/// from the package include for the following reasons:
/// - Don't want to expose `bdljsn` names and symbols to the outer scope.
/// - Member fields and functions defined for this implementation are used only
///   locally, so there is no reason to make it visible.
class JsonPrinter::JsonPrinterImpl {
  private:
    // CLASS-SCOPE CATEGORY
    BALL_LOG_SET_CLASS_CATEGORY("MQBSTAT.JSONPRINTERIMPL");

  private:
    // PRIVATE TYPES
    typedef JsonPrinter::StatContextsMap StatContextsMap;

  private:
    // DATA
    /// Options for printing a compact JSON
    const bdljsn::WriteOptions d_opsCompact;

    /// Options for printing a pretty JSON
    const bdljsn::WriteOptions d_opsPretty;

    /// StatContext-s map
    const StatContextsMap d_contexts;

  private:
    // NOT IMPLEMENTED
    JsonPrinterImpl(const JsonPrinterImpl& other) BSLS_CPP11_DELETED;
    JsonPrinterImpl&
    operator=(const JsonPrinterImpl& other) BSLS_CPP11_DELETED;

  public:
    // CREATORS

    /// Create a new `JsonPrinterImpl` object, using the specified
    /// `statContextsMap` and the specified `allocator`.
    explicit JsonPrinterImpl(const StatContextsMap& statContextsMap,
                             bslma::Allocator*      allocator);

    // ACCESSORS

    /// Print the JSON-encoded stats to the specified `out`.
    /// If the specified `compact` flag is `true`, the JSON is printed in a
    /// compact form, otherwise the JSON is printed in a pretty form.
    /// Return `0` on success, and non-zero return code on failure.
    ///
    /// THREAD: This method is called in the *StatController scheduler* thread.
    int printStats(bsl::ostream&         os,
                   bool                  compact,
                   int                   statId,
                   const bdlt::Datetime& now) const;
};

inline JsonPrinter::JsonPrinterImpl::JsonPrinterImpl(
    const StatContextsMap& statContextsMap,
    bslma::Allocator*      allocator)
: d_opsCompact(bdljsn::WriteOptions()
                   .setSpacesPerLevel(0)
                   .setStyle(bdljsn::WriteStyle::e_COMPACT)
                   .setSortMembers(true))
, d_opsPretty(bdljsn::WriteOptions()
                  .setSpacesPerLevel(4)
                  .setStyle(bdljsn::WriteStyle::e_PRETTY)
                  .setSortMembers(true))
, d_contexts(statContextsMap, allocator)
{
    // NOTHING
}

inline int
JsonPrinter::JsonPrinterImpl::printStats(bsl::ostream&         os,
                                         bool                  compact,
                                         int                   statsId,
                                         const bdlt::Datetime& datetime) const
{
    // executed by *StatController scheduler* thread

    // PRECONDITIONS

    bdljsn::Json json;
    json.makeObject();

    {
        // Output stats_id
        json.theObject().insert("stats_id", bdljsn::JsonNumber(statsId));
    }
    {
        // Output datetime in de-facto standard ISO-8601 format:
        char buffer[64];
        bdlt::Iso8601Util::generate(buffer, sizeof(buffer), datetime);
        json.theObject().insert("timestamp", buffer);
    }
    // Populate DOMAIN QUEUES stats
    {
        const bmqst::StatContext& ctx =
            *d_contexts.find("domainQueues")->second;

        DomainsStatsPrintUtils::populateAll(os, json, ctx);
    }
    // {
    //     const bmqst::StatContext& ctx =
    //         *d_contexts.find("domainQueues")->second;

    //     bdljsn::JsonObject& domainQueuesObj =
    //     obj["domainQueues"].makeObject();

    //     DomainsStatsConversionUtils::populateAll(&domainQueuesObj, ctx);
    // }
    // // Populate CLIENTS stats
    // {
    //     const bmqst::StatContext& ctx = *d_contexts.find("clients")->second;

    //     bdljsn::JsonObject& clientsObj = obj["clients"].makeObject();

    //     ClientStatsConversionUtils::populateAll(&clientsObj, ctx);
    // }
    // // Populate CLUSTERS stats
    // {
    //     const bmqst::StatContext& ctx =
    //         *d_contexts.find("clusterNodes")->second;

    //     bdljsn::JsonObject& clustersObj = obj["clusters"].makeObject();

    //     ClientStatsConversionUtils::populateAll(&clustersObj, ctx);
    // }

    // // Populate TCP CHANNELS stats
    // {
    //     const bmqst::StatContext& ctx =
    //     *d_contexts.find("channels")->second;

    //     bdljsn::JsonObject& tcpChannelsObj = obj["channels"].makeObject();

    //     ChannelStatsConversionUtils::populateAll(&tcpChannelsObj, ctx);
    // }

    // const bdljsn::WriteOptions& ops = compact ? d_opsCompact : d_opsPretty;
    // const int rc = bdljsn::JsonUtil::write(os, json, ops);
    // if (0 != rc) {
    //     BALL_LOG_ERROR << "Failed to encode stats JSON, rc = " << rc;
    //     return rc;  // RETURN
    // }

    return 0;
}

// -----------------
// class JsonPrinter
// -----------------

JsonPrinter::JsonPrinter(const StatContextsMap& statContextsMap,
                         bslma::Allocator*      allocator)
{
    bslma::Allocator* alloc = bslma::Default::allocator(allocator);

    d_impl_mp.load(new (*alloc) JsonPrinterImpl(statContextsMap, alloc),
                   alloc);
}

int JsonPrinter::printStats(bsl::ostream&         os,
                            bool                  compact,
                            int                   statsId,
                            const bdlt::Datetime& datetime) const
{
    // executed by *StatController scheduler* thread

    // PRECONDITIONS
    BSLS_ASSERT_SAFE(d_impl_mp);

    return d_impl_mp->printStats(os, compact, statsId, datetime);
}

}  // close package namespace
}  // close enterprise namespace
