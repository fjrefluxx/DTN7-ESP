#pragma once
#include <set>
#include <string>
#include <vector>
#include "EID.hpp"
#include "dtn7-bundle.hpp"

#define RETENTION_CONSTRAINT_DISPATCH_PENDING 2
#define RETENTION_CONSTRAINT_FORWARD_PENDING 1
#define RETENTION_CONSTRAINT_NONE 0

/// @brief This class represents a DTN node
class Node {
   public:
    /// @brief unique node identifier, to be used by CLA
    std::string identifier = "empty";

    /// @brief a vector containing all EIDs known to be registered at this node
    std::vector<EID> Eids;

    /// @brief stores the nodeID of the node, as given in the constructor
    std::string URI = "none";

    /// @brief time the Node was Last seen in ms
    uint64_t lastSeen = 0;
#if CONFIG_useReceivedSet
    /// @brief if enabled in menuconfig, the hashes of received bundles are stored here
    std::set<size_t> receivedHashes;

    /// @brief if confirmation of bundle reception via bundle id hashes is enabled in menuconfig,
    /// this is used to store that this node has confirmed the reception of a specific bundle.
    /// This is only applicable if the node object is contained in a BundleInfo object and the indicates that the Bundle contained in the same BundleInfo object was received by this node.
    bool confirmedReception = false;
#endif

    /// @brief creates new node object from a URI
    /// @param URI URI of the node
    Node(std::string URI);

    /// @brief whether the node object stores the Position of the Node
    bool hasPos = false;

    /// @brief stores the Position of the Node, first Latitude then Longitude
    std::pair<float, float> position{0, 0};

    /// @brief generates an empty node object
    Node();

    /// @brief creates a node object from a previously serialized node object created using the serialize function.
    /// @param serialized the serialized node object
    Node(std::vector<uint8_t> serialized);

    /// @brief converts bytes to a std::string containing those bytes as hex, useful for using i.e. mac address as node identifier
    /// @param data pointer to byte array
    /// @param dataSize size of byte array
    /// @return std::string containing bytes as hex
    static std::string idFromBytes(uint8_t* data, size_t dataSize);

    /// @brief serializes a node object useful for storing node objects
    /// @return std::vector containing the bytes of the serialized node object
    std::vector<uint8_t> serialize();

    /// @brief prints the node object
    void print();

    /// @brief updates the Last seen field with the current Time
    void setLastSeen();

    /// @brief adds a Position to the node
    /// @param lat Latitude
    /// @param lng  Longitude
    void setPosition(float lat, float lng);

    /// @brief removes the position information from the Node
    /// @return true if the node had Position information
    bool removePosition();
};

/// @brief a class representing a newly received Bundle, for internal use only, only transfers pointer to bundle together with received from string, destructor does not delete bundle!
class ReceivedBundle {
   public:
    /// @brief pointer to the contained bundle
    Bundle* bundle = nullptr;

    /// @brief identifier of the node from which the bundle was received
    std::string fromAddr;

    /// @brief constructor for the received bundle
    /// @param bundle Bundle to be contained
    /// @param fromIdentifier identifier of node it was received from
    ReceivedBundle(Bundle* bundle, std::string fromIdentifier);

    /// @brief Destructs ReceivedBundle. Does not delete bundle!
    ~ReceivedBundle();
};

/// @brief this class stores additional info to bundles and the corresponding Bundle
class BundleInfo {
   public:
    /// @brief whether the Bundle was locally delivered
    bool locallyDelivered = false;

    /// @brief list of nodes which have already been forwarded the bundle
    std::vector<Node> forwardedTo;

    /// @brief number of times the Bundle was broadcast
    uint numOfBroadcasts = 0;

    /// @brief last system time the Bundle was broadcast
    uint64_t lastBroadcastTime = 0;

    /// @brief the actual Bundle data
    Bundle bundle;

    /// @brief generates a BundleInfo object from a serialized BundleInfo Object, as created using BundleInfo::serialize.
    /// @param serialized
    BundleInfo(std::vector<uint8_t> serialized);

    /// @brief generates a BundleInfo Object from a Bundle Object
    /// @param bundle
    BundleInfo(Bundle* bundle);

    BundleInfo() {};

    ~BundleInfo() {};

    /// @brief serialization function for the bundle info class, used for storage
    /// @return vector containing serialized BundleInfo object
    std::vector<uint8_t> serialize();

    /// @brief function to set retention constraints
    /// @param constraint retention constraint to set, see Bundle class for definition of Retention constraints
    void setRetentionConstraint(uint constraint);
};
