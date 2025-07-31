/**
 * @file dtn7-esp.cpp
 * @brief Implements functionality of the DTN7 namespace. Can be of interest if, e.g., a custom CLA/Router/Storage is to be added
 */
#include "dtn7-esp.hpp"
#include <stdio.h>
#include "esp_log.h"

#include "BLE_CLA.hpp"
#include "BroadcastRouter.hpp"
#include "BundleProtocolAgent.hpp"
#include "Data.hpp"
#include "Endpoint.hpp"
#include "EpidemicRouter.hpp"
#include "FlashStorage.hpp"
#include "InMemoryStorage.hpp"
#include "LoRaCLA.hpp"
#include "Router.hpp"
#include "Storage.hpp"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps.h"
#include "helpers.h"

BundleProtocolAgent* DTN7::BPA = nullptr;
TaskHandle_t DTN7::bundleReceiverHandle = NULL;
TaskHandle_t DTN7::storageRetryHandle = NULL;
TaskHandle_t DTN7::bundleForwardHandel = NULL;
TaskHandle_t DTN7::claPollHandle = NULL;

#if CONFIG_USE_LORA_CLA
LoraCLA* DTN7::loraCLA = NULL;
#endif

#if CONFIG_USE_BLE_CLA
BleCLA* DTN7::bleCla = NULL;
#endif

#if CONFIG_HasAccurateClock

bool DTN7::clockSynced = false;

#endif

HashWrapper* DTN7::hasher = NULL;
int32_t DTN7::maxPeerAge = CONFIG_MaxPeerAge;

/// @brief local node object of this node, including node identifier and EID list
/// The EIDs list of this node object is not updated when registering/unregistering endpoints with the BPA
Node* DTN7::localNode = NULL;

/// @brief set up the tasks needed by the BPA
static inline void createTasks() {
    // set up the task which handles new bundles
    xTaskCreate(&DTN7::bundleReceiver, "BundleReceiver",
                CONFIG_BundleReceiverStackSize, NULL,
                CONFIG_BundleReceiverPriority, &DTN7::bundleReceiverHandle);

    // setup the task which retrys previously stored bundles
    xTaskCreate(&DTN7::retryBundles, "BundleRetry", CONFIG_BundleRetryStackSize,
                NULL, CONFIG_BundleRetryPriority, &DTN7::storageRetryHandle);

    // setup the task which forwards/retried previously stored bundles
    xTaskCreate(&DTN7::bundleForwarder, "BundleForwarder",
                CONFIG_BundleForwarderStackSize, NULL,
                CONFIG_BundleForwarderPriority, &DTN7::bundleForwardHandel);

    // setup the task which polls CLAs not using the queue system
    xTaskCreate(&DTN7::pollClas, "ClaPoll", CONFIG_ClaPollStacksize, NULL,
                CONFIG_ClaPollPriority, &DTN7::claPollHandle);
    return;
}

/// @brief setup the Local Node of the BPA, sets the identifier to be the URI
/// @param URI
static inline void setupLocalNode(std::string URI) {
    DTN7::localNode = new Node(URI);
    DTN7::localNode->identifier = URI;

    // DTN7::BPA->storage->addNode(*DTN7::localNode); do NOT add local node to known peers
    return;
}

/// @brief Set up all configured CLAs. If a custom CLA is to be added, do that here!
static inline void setupClas() {
// Setup all CLAs enabled in menuconfig
#if CONFIG_USE_LORA_CLA
    DTN7::loraCLA = new LoraCLA();
    DTN7::BPA->router->clas.push_back(DTN7::loraCLA);
#endif
#if CONFIG_USE_Serial_CLA
    CLA* serialCla = new SerialCLA();
    DTN7::BPA->router->clas.push_back(serialCla);
#endif
#if CONFIG_USE_BLE_CLA
    DTN7::bleCla = new BleCLA(DTN7::localNode->URI);
    DTN7::BPA->router->clas.push_back(DTN7::bleCla);
#endif
    return;
}

/// @brief set up all polymorphic classes required by the BundleProtocolAgent (i.e., Router, Storage, HashWrapper) and the BPA itself
/// @param URI the URI used for the BundleProtocolAgent
static inline void setupClasses(std::string URI) {
    std::vector<CLA*> clas;
    // setup the Storage class as configured in menuconfig
#if CONFIG_StorageType_Flash
    Storage* storage = new FlashStorage();
#elif CONFIG_StorageType_InMemory
    ESP_LOGI("BundleProtocolAgent Setup", "Setting up InMemoryStorage...");
    Storage* storage = new InMemoryStorage();
#elif CONFIG_StorageType_InMemorySerialized
    ESP_LOGI("BundleProtocolAgent Setup",
             "Setting up InMemoryStorageSerialized...");
    Storage* storage = new InMemoryStorageSerialized();
#elif CONFIG_StorageType_InMemorySerializedIA
    ESP_LOGI("BundleProtocolAgent Setup",
             "Setting up InMemoryStorageSerializedIA...");
    Storage* storage = new InMemoryStorageSerializedIA();
#elif CONFIG_StorageType_Dummy
    ESP_LOGI("BundleProtocolAgent Setup", "Setting up DummyStorage...");
    Storage* storage = new DummyStorage();
#else
    ESP_LOGE("BundleProtocolAgent Setup",
             "Failed, unknown Storage Type Configured");
    abort();
#endif

    // setup the router class as configured in menuconfig
#if CONFIG_RouterType_SimpleBroadcast
    Router* router = new SimpleBroadcastRouter(clas, storage);
#elif CONFIG_RouterType_SimpleEpidemic
    Router* router = new EpidemicRouter(clas, storage);
#else
    ESP_LOGE("BundleProtocolAgent Setup",
             "Failed, unknown Router Type Configured");
    abort();
#endif
    // create the hasher class instance, only used if usage of BundleID hashes for reception confirmation is used
    DTN7::hasher = new StdHasher();

    // create BPA instance
    DTN7::BPA = new BundleProtocolAgent(URI, storage, router);
    return;
}

Endpoint* DTN7::setup(std::string URI,
                      void (*onReceive)(std::vector<uint8_t>, std::string,
                                        std::string, PrimaryBlock)) {
    // check whether BPA is already configured
    if (DTN7::BPA != nullptr)
        return DTN7::BPA->localEndpoint;

    // setup relevant classes
    setupClasses(URI);

    // add callback to local endpoint
    DTN7::BPA->localEndpoint->setCallback(onReceive);

    // create the local node object
    setupLocalNode(URI);
#if CONFIG_UseGPS
    initializeGPS();
#endif

    // create tasks required by the BPA
    createTasks();

    // setup all configured CLAs
    setupClas();
    return DTN7::BPA->localEndpoint;
}

Endpoint* DTN7::setup(std::string URI) {
    // check whether BPA is already configured
    if (DTN7::BPA != nullptr)
        return DTN7::BPA->localEndpoint;

    // setup relevant classes
    setupClasses(URI);

    // create the local node object
    setupLocalNode(URI);
#if CONFIG_UseGPS
    initializeGPS();
#endif

    // create tasks required by the BPA
    createTasks();

    // setup all configured CLAs
    setupClas();
    return DTN7::BPA->localEndpoint;
}

Endpoint* DTN7::registerEndpoint(std::string URI,
                                 void (*onReceive)(std::vector<uint8_t>,
                                                   std::string, std::string,
                                                   PrimaryBlock)) {
    Endpoint* result;
    // check whether a callback was given and create endpoint accordingly
    if (onReceive != NULL)
        result = new Endpoint(URI, onReceive);
    else {
        result = new Endpoint(URI);
    }

    // register Endpoint with BPA
    DTN7::BPA->registerEndpoint(result);

    return result;
}

Endpoint* DTN7::unregisterEndpoint(std::string URI) {
    // find Pointer to endpoint to be unregistered
    std::vector<Endpoint*> endpoints = DTN7::BPA->registeredEndpoints;
    for (Endpoint* end : endpoints) {
        if (end->localEID.getURI() == URI) {

            // unregister endpoint
            DTN7::BPA->unregisterEndpoint(end);
            return end;
        }
    }

    return nullptr;
}

Endpoint* DTN7::unregisterEndpoint(Endpoint* endpoint) {
    // check if endpoint is registered
    if (endpoint->BPA == nullptr)
        return nullptr;
    DTN7::BPA->unregisterEndpoint(endpoint);
    return endpoint;
}

void DTN7::clearOldPeers() {
    ESP_LOGI("clearOldPeers", "checking peer age");

    // get all known peers
    std::vector<Node> nodes = BPA->storage->getNodes();

    // create timeval struct
    struct timeval tv_now;

    // compare current time with maximum age for all peers
    for (Node n : nodes) {
        // get current time
        gettimeofday(&tv_now, NULL);

        // convert current time to milliseconds
        uint64_t now =
            ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);

        // check whether node is added staticly and not remove it
        if (n.lastSeen == UINT64_MAX)
            continue;

        // calculate node age
        uint64_t age =
            (now - n.lastSeen) /
            1000;  // last seen is always less or equal than the current time (except for staticly added nodes, but these do not get checked here), therfore a unsigned int can be used for the result
        ESP_LOGI("clearOldPeers", "Node: %s, age:%llu, limit:%i", n.URI.c_str(),
                 age, CONFIG_MaxPeerAge);
        if (age > maxPeerAge)
            BPA->storage->removeNode(n.URI);
    }
    return;
}

void DTN7::bundleReceiver(void* param) {
    ESP_LOGI("bundleReceiver", "Task started");
    ReceivedBundle* recBundle;
    while (true) {
        // read an element from the receive queue, this is a blocking action, other tasks are scheduled if no element is available
        if (xQueueReceive(DTN7::BPA->receiveQueue, &recBundle,
                          (TickType_t)100) == pdTRUE) {

            // log heap and stack usage for debuging
            ESP_LOGD(
                "bundleReceiver",
                "Free Heap: = %i, minimal Free Stack since Task creation:%u",
                heap_caps_get_free_size(MALLOC_CAP_8BIT),
                uxTaskGetStackHighWaterMark(NULL));

            // copy bundle pointer and from node from received bundle and delete received bundle
            Bundle* bundle = recBundle->bundle;
            std::string fromNode = recBundle->fromAddr;
            delete recBundle;
            ESP_LOGI("bundleReceiver", "receiving Bundle..., fromNode: %s",
                     fromNode.c_str());

            // get bundleID
            std::string bundleId = bundle->getID();
#if CONFIG_useReceivedSet
            // if enabled add hash of bundleID to set of revived bundleIDs of this node
            DTN7::localNode->receivedHashes.insert(
                DTN7::hasher->hash(bundleId));
#endif
            // check if the sender of node is known, i.e. the from field in the received bundle is not none and ensure to not add local node as peer
            if (fromNode != "none" && fromNode != DTN7::localNode->URI) {
                Node stored = DTN7::BPA->storage->getNode(
                    fromNode);  // this partially handles discovery, if a unknown node is the sender of a bundle it will be added to the nodes List

                // check if node was not known
                if (stored.URI == "none") {
                    stored.URI =
                        fromNode;  // now the node is known by its identifier, but not what EIDs it has
                    stored.identifier = fromNode;
                    ESP_LOGI("bundleReceiver",
                             "Node was previously unknown, now it is stored");
                }

                // update last seen of node to now
                stored.setLastSeen();

                // give updated node back to storage
                DTN7::BPA->storage->addNode(stored);
            }

            // check if bundle with the same ID was already received and if yes, discard bundle as duplicate
            if (!DTN7::BPA->storage->checkSeen(bundleId)) {
                DTN7::BPA->storage->storeSeen(bundleId);
                BPA->bundleReception(bundle, fromNode);
                ESP_LOGI("bundleReceiver", "finished reception");
            }
            else {
                ESP_LOGI("bundleReceiver", "duplicate bundle: %s, is discarded",
                         bundleId.c_str());
                delete bundle;
            }

            vTaskDelay(1);  // needed to avoid watchdog
        }
    }

    ESP_LOGE("bundleReceiver", "Task finished");  // should never be reached
    vTaskDelete(NULL);  // delete this task safely if we get here
}

void DTN7::bundleForwarder(void* param) {
    ESP_LOGI("bundleForwarder", "Task started");
    BundleInfo* bundle;
    while (true) {
        // read an element from the forward queue, this is a blocking action, other tasks are scheduled if no element is available
        if (xQueueReceive(DTN7::BPA->forwardQueue, &bundle, (TickType_t)100) ==
            pdTRUE) {
            // log heap and stack usage for debuging
            ESP_LOGD(
                "bundleForwarder",
                "Free Heap: = %i, minimal Free Stack since Task creation:%u",
                heap_caps_get_free_size(MALLOC_CAP_8BIT),
                uxTaskGetStackHighWaterMark(NULL));

            ESP_LOGI("bundleForwarder",
                     "forwarding Bundle..., BPA's router has:%u CLA'S",
                     BPA->router->clas.size());

            // call BPA function for forwarding
            BPA->bundleForwarding(bundle);

            vTaskDelay(1);  // needed to avoid watchdog
        }
    }
    ESP_LOGE("bundleForwarder", "Task finished");  // should never be reached
    vTaskDelete(NULL);  // delete this task safely if we get here
}

void DTN7::retryBundles(void* param) {
    ESP_LOGI("bundleRetrier", "Task started");

    while (true) {
#if CONFIG_NOTIFY_RETRY_TASK  // if the experimental feature to allow the retry task to be triggered externally is enabled, we need to wait for notification instead of just sleeping
        // wait for notification to wake up retry task. If this notification does not appear, the original behavior is kept by the timeout
        ulTaskNotifyTake(pdTRUE, (CONFIG_TimeBetweenStorageRetry * 1000) /
                                     portTICK_PERIOD_MS);
#else
        // begin by waiting the time between retries as configured in menuconfig. This is done in the beginning of the Loop in order to not start by retrying directly after setup of the task
        vTaskDelay((CONFIG_TimeBetweenStorageRetry * 1000) /
                   portTICK_PERIOD_MS);
#endif

        // log heap and stack usage for debuging
        ESP_LOGD("bundleRetrier",
                 "Free Heap: = %i, minimal Free Stack since Task creation:%u",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 uxTaskGetStackHighWaterMark(NULL));

        // check the age of known peers and remove all that have not been seen recently
        clearOldPeers();

        ESP_LOGI("bundleRetrier", "Retrying Stored Bundles");

        // the retry mechanism is structured into cycles. In one cycle all bundles which have been stored at the beginning of the cycle are retried. This cycle is stateted in the following.
        DTN7::BPA->storage->beginRetryCycle();

        // now we retry bundles as long as ones which are to be retryted in this cycle are available. This iteration allows us to only return small batches of the stored bundles and means that at no point all bundles must be kept in memory at the same time, only the number of bundles in one batch.
        while (DTN7::BPA->storage->hasBundlesToRetry()) {
            // we get a batch of bundles, the size of which  is determined by the storage implementation.
            std::vector<BundleInfo> toRetry =
                DTN7::BPA->storage->getBundlesRetry();

            ESP_LOGI("bundleRetrier",
                     "Retrying Batch of Bundles, batch size:%u",
                     toRetry.size());

            // now retry all bundles in batch
            for (BundleInfo bundle : toRetry) {
                // if the bundle is expired we do not retry it and it is hereby discarded, as it is not stored anymore -> important: if a bundle is beeing retried, it is not stored by the storage system anymore. It may be reinserted into storage if deemed nexecray by the router.
                if (checkExpiration(&bundle)) {
                    // send the bundle to the forward queue again
                    BundleInfo* bundleHeap = new BundleInfo(bundle);
                    xQueueSend(DTN7::BPA->forwardQueue, (void*)&bundleHeap,
                               portMAX_DELAY);
                }
                vTaskDelay(
                    100);  // needed to avoid watchdog and to space out potentially occurring transmissions to decrease the risk of missing them due to the receiver not beeing done decoding the previous transmission
            }
            vTaskDelay(1);  // needed to avoid watchdog
        }
    }

    ESP_LOGE("bundleRetrier", "Task finished");  // should never be reached
    vTaskDelete(NULL);  // delete this task safely if we get here
}

void DTN7::pollClas(void* param) {
    ESP_LOGI("PollCLAs", "Task started");
    while (true) {
        // Pause between polling attempts as configured in menuconfig.
        vTaskDelay((CONFIG_TimeBetweenClaPoll * 1000) / portTICK_PERIOD_MS);

        // log heap and stack usage for debuging
        ESP_LOGD("PollCLAs",
                 "Free Heap: = %i, minimal free stack since task creation:%u",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 uxTaskGetStackHighWaterMark(NULL));
        ESP_LOGI("PollCLAs", "Polling CLAs");

        // The router class handles the access to the CLAs, therefore we use its getNewBundles to poll al applicable CLAs
        std::vector<ReceivedBundle*> polled =
            DTN7::BPA->router->getNewBundles();

        // now handle reception of all polled bundles
        for (ReceivedBundle* bundle : polled) {
            // send each bundle to receiveQueue
            xQueueSend(DTN7::BPA->receiveQueue, (void*)&bundle, portMAX_DELAY);
            vTaskDelay(1);  // avoid watchdog
        }
    }
}

bool DTN7::checkExpiration(BundleInfo* bundle) {
    // to check whether a bundle has surpassed its age limit, we first retrieve said limit from the primary block of the bundle
    uint64_t ageLimit = bundle->bundle.primaryBlock.lifetime;

    // RFC 9171 Section 4.2.1. under "Lifetime": the BPA is allowed to override a bundle's lifetime.
    // Thus, it is allowed to deleted a bundle at a different age on this node. The original lifetime must be kept in the primary block.
    // If enabled in menuconfig, uses the lifetime override instead of the TTL from the primary block.
#if CONFIG_IgnoreBundleTTL
    ageLimit = OverrideBundleTTL;
#endif
    // if the bundle has a bundle age block, use this to determine the age of the bundle
    if (bundle->bundle.hasBundleAge) {
        // the current time in order to calculate the time the bundle spent at this node
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        // convert current time to miliseconds
        uint64_t currentTime =
            ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);

        // calculate the current age of the bundle with the following formula: current age = (time spent at this node) + age from age block
        // the time spent at this node is calculated by subtracting the time the bundle was received from the current time
        uint64_t currentAge =
            (currentTime - bundle->bundle.receivedAt) + bundle->bundle.getAge();

        // print calculate bundle age and limit for information purposes
        ESP_LOGI("checkExpiration", "BundleAge:%llu, Limit:%llu", currentAge,
                 ageLimit);

        // if the limit has been surpassed, delete the bundle with the appropriate reason code
        if (currentAge >= ageLimit) {
            BPA->bundleDeletion(
                bundle, BundleStatusReportReasonCodes::LIFETIME_EXPIRED);
            ESP_LOGW("checkExpiration", "AgeLimit exceeded, age:%llu",
                     currentAge);
            return false;
        }
    }

    // if the node has an accurate, synchronized clock, the bundles creation time is compared with the current time to determine its age
#if CONFIG_HasAccurateClock
    //first, check whether node clock is actually synchronized
    if (clockSynced) {

        // check whether the creating node had an accurate clock by checking whether the creation time is not 0
        if (bundle->bundle.primaryBlock.timestamp.creationTime != 0) {
            // get the time at which the bundle expires
            uint64_t expirationTime =
                bundle->bundle.primaryBlock.timestamp.creationTime + ageLimit;

            // the current time in order to calculate the time the bundle spent at this node
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);

            // convert current time to miliseconds
            uint64_t currentTime = ((int64_t)tv_now.tv_sec * 1000L +
                                    (int64_t)tv_now.tv_usec / 1000);

            // check whether the expiration time has be surpassed, if yes delete the bundle with the appropriate reason code
            if (expirationTime < currentTime) {
                BPA->bundleDeletion(
                    bundle, BundleStatusReportReasonCodes::LIFETIME_EXPIRED);
                return false;
            }
        }
    }
    else {
        ESP_LOGW("checkExpiration",
                 "Accurate Clock enabled, but not synchronized! Falling back "
                 "to non accurate clock operation");
    }
#endif
    return true;
}

void DTN7::deinitializeBPA() {
    // if the gps module was enabled we have to deinitialize it
#if CONFIG_UseGPS
    deinitializeGPS();
#endif
    // delete all tasks of the BPA
    vTaskDelete(DTN7::bundleReceiverHandle);
    vTaskDelete(DTN7::storageRetryHandle);
    vTaskDelete(DTN7::bundleForwardHandel);

    // call the destructors of all CLAs
    for (CLA* cla : DTN7::BPA->router->clas) {
        delete cla;
    }

    // delete the Localnode object and the BPA object
    delete DTN7::localNode;
    delete DTN7::BPA;
}

std::string DTN7::uriFromMac() {
    // get the esp's base mac address, this is unique from the factory
    uint8_t macAddr[6];
    esp_base_mac_addr_get(macAddr);

    // convert the bytes of the mac address into a string
    std::string macString = Node::idFromBytes(macAddr, 6);

    // addent "dtn:// " before the mac address and return
    return std::string("dtn:// ").append(macString);
}

void DTN7::addStaticPeer(Node node) {
    // the node gets the largest possible value for last seen, this value would normally only be reached after ~600 Million Years
    node.lastSeen = UINT64_MAX;
    // now add node to storage
    DTN7::BPA->storage->addNode(node);
    return;
}
