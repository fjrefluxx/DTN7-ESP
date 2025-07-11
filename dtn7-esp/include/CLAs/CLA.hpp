#pragma once
#include <list>
#include <string>
#include "Data.hpp"
#include "dtn7-bundle.hpp"
#include "sdkconfig.h"

/// @brief generic bas class for all CLA's
class CLA {
   private:
    /// @brief the name of the CLA, should be set to a unique and readable value for each type of CLA
    std::string name = "invalid";

    /// @brief this field must be set in each CLA implementation and is used by the router to check whether a CLA can address specific nodes or only broadcast messages
    bool canAddress;

   public:
    /// @brief returns the name of the CLA
    /// @return std::string with the name of the CLA
    virtual std::string getName() = 0;

    /// @brief returns a whether a CLA  is able to send message to a specific, singular node. Returns False if the CLA broadcasts messages
    /// @return whether a CLA can address specific nodes(true) or only broadcast messages(false)
    virtual bool checkCanAddress() = 0;

    CLA() {};

    virtual ~CLA() {};

    /// @brief if the CLA is not setup to use the received queue, this function can be used to get the bundles received since the last time it was called, must be safe to call from a different thread than send()
    /// @return new received bundles since this function was last called
    virtual std::vector<ReceivedBundle*> getNewBundles() = 0;

    /// @brief send a bundle to a target node via the CLA, must be safe to call from a different thread than getNewBundles() / if the queue system is used thread safety to the thread receiving bundles has to be kept in mind
    /// @param bundle the Bundle to send
    /// @param destination destination node, if the CLA can send to specific addresses, the nodes identifier is used to send only to this address
    /// @return true if the bundle was successfully sent
    virtual bool send(Bundle* bundle, Node* destination = nullptr) = 0;
};