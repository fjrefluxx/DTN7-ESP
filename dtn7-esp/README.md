# dtn7-esp

This folder contains the ESP32 implementation of RFC 9171 for use with ESP-IDF.

This implementation can be used as an IDF component, for further information on the usage of IDF components and the IDF Component Manager see: https://docs.espressif.com/projects/idf-component-manager/en/latest/

Usage examples can be found in the [examples folder](/examples/).

# Configuration Options
This uses the [Project Configuration](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html) mechanisms provided with the ESP-IDF, commonly referred to as "menuconfig".
All available configuration options are listed and described under [menuconfig](/dtn7-esp/menuconfig.md).

# Detailed Explanations
In the following further details regarding provided CLA's, Routers and Storage Options are explained.
The primary focus is to show important aspects for the development of custom CLA's, Routers and Storage Options.

## CLAs
### Custom CLAs
When developing custom CLAs it is important the choose whether the CLA should be polled ore whether it sends received bundles to the corresponding queue proactively, see [Execution Structure](/README.md#execution-structure).

CLAs which shall be polled must return received bundles via their ```getNewBundles()``` functions, were as ones sending the bundles directly to the receivedQueue must return an empty vector.

Another decision which is relevant when implementing a CLA is whether it is able to address nodes in transmissions (perform unicast).
No example of such a CLA is provided, but all provided routing strategies are able to handle such a CLA, assuming it provides a peer discovery mechanism and returns ```true``` in its ```checkCanAddress()``` function.

In any case the custom CLA must be added to the ```setupClass()``` function in [dtn7idf.cpp](/dtn7-esp/src/dtn7-esp.cpp) and it is recommended to make it configurable/selectable in menuconfig.


## Routing
### Custom Routing
The main functionality of the router is contained in its ```handleForwarding()``` function. This function is passed a pointer to a BundleInfo object and must select CLAs to use to forward this function and call their ```send()``` functions. If CLAs which can address specific nodes are to be used, these must be given the address of the intended destination node.
Before calling the CLA's send function the bundle must be prepared for transmission by using the ```prepareForSend()``` function of the ```Router``` base class.

In any case the custom routing strategy must be added to the ```setupClasses()``` function in [dtn7idf.cpp](/dtn7-esp/src/dtn7-esp.cpp) and it must be ensured that all other routing strategies are not setup in this function.
Furthermore, it is recommended to make the routing strategy configurable/selectable in menuconfig.