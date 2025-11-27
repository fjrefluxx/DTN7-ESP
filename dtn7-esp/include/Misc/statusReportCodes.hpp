#pragma once

/**
 * @file CLA.hpp
 * @brief This file contains the a namespace for the various BundleStatusReportReasonCodes.
 */


/// @brief this namespace contains a mapping between status report reason codes and numerical values
namespace BundleStatusReportReasonCodes {

static const uint NO_ADDITIONAL_INFORMATION = 0;
static const uint LIFETIME_EXPIRED = 1;
static const uint FORWARDED_OVER_UNIDIRECTIONAL_LINK = 2;
static const uint TRANSMISSION_CANCELED = 3;
static const uint DEPLETED_STORAGE = 4;
static const uint DESTINATION_ENDPOINT_ID_UNAVAILABLE = 5;
static const uint NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE = 6;
static const uint NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE = 7;
static const uint BLOCK_UNINTELLIGIBLE = 8;
static const uint HOP_LIMIT_EXCEEDED = 9;
static const uint TRAFFIC_PARED = 10;
static const uint BLOCK_UNSUPPORTED = 11;

/// @brief function to check whether a status report code is to be considered not a failure
/// @param reason the reason code to be checked
/// @return whether to consider the status code a failure
[[maybe_unused]] static bool checkNoFailure(uint reason) {
    return (reason == NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE) ||
           (reason == NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE) ||
           (reason == TRAFFIC_PARED) || (FORWARDED_OVER_UNIDIRECTIONAL_LINK);
}

}  // namespace BundleStatusReportReasonCodes