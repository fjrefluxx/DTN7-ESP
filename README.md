# DTN7-ESP

Implementation of the Bundle Protocol Version 7 according to [RFC 9171](https://www.rfc-editor.org/rfc/rfc9171.pdf) for ESP32 microcontrollers using [ESP-IDF](https://idf.espressif.com/) and FreeRTOS.

Other FreeRTOS-platforms may be usable with adjustments.
This project was delevoped with the classical ESP32 and has been tested on: ESP32, ESP32-S3, ESP32-C3.


The implementation of the bundle data structure can be found under [dtn7-bundle](dtn7-bundle/) and the implementation of the Bundle Protocol logic, along with all CLAs, routing, and storage components can be found under [dtn7-esp](dtn7-esp/).

## Overview
[[_TOC_]]

## Installation
### Requirements
1. Install and configure ESP-IDF (v5.2.1 or newer), see [ESP-IDF's get started tutorial](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

2. Install the DTN7-ESP IDF-component using the [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-component-manager.html).

3. The ESP-IDF extension for Visual Studio Code is recommended.


### Getting Started
The easiest starting point is to modify an [example](/examples/). In Visual Studio Code, open the folder of the desired example and run the ```Build Project``` command of the ESP-IDF extension.

To develop your own application, start from the [Template Project](/examples/TemplateProject/).

> [!NOTE] Logging
> The project makes heavy use of the [ESP logging library](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html). 
> Log levels are defined in menuconfig. 
> Turning to `debug` might help finding problems. Turn off logging in menuconfig before deploying to reduce overhead.


### Full Setup
1. Create a new ESP-IDF project

2. Create a file ```idf_component.yml``` in folder ```main/```. 

3. Add the dependency for DTN7-ESP in ```main/idf_component.yml```:
    
    ```
    dependencies:
        dtn7-esp:
            git: "[insert your path to the folder]/dtn7-esp/dtn7-esp.git"
            path: "dtn7-esp"
    ```

    > [!NOTE]
    > Format must be correct. In case of errors, check the corresponding file in one of the example projects.

4. Run the ESP-IDF ```menuconfig```:
 
   In VSC with the ESP-IDF plugin, press the small gear in the bottom row or use ```View -> Command Palette -> ESP-IDF: SDK Configuration editor (menuconfig)```. This should automatically download the dependencies and show the available configuration options.
   
   > [!TIP]
   > When running ```menuconfig``` again to change settings, it might be necessary to execute this command twice or clear the previous settings by using the ```ESP-IDf: Full Clean Project``` option.

5. Configure ESP-IDF to use C++:
    - Change to `main/``` directory
    - In `CMakeLists.txt`, change `main.c` to `main.cpp`
    - Rename `main.c` file to `main.cpp`
    - In `main.cpp`, change the head of the `app_main` function to `extern "C" void app_main(void)`
    - In `main.cpp`, include dtn7-esp.hpp (add line `#include "dtn7idf.hpp"`)

6. Define a local endpoint:
    
    In the ```app_main``` function in ```main.cpp``` add
    ```
    Endpoint* localEndpoint = DTN7::setup("dtn://node0");
    ```
    to setup a local endpoint with the URI *dtn://node0*. This endpoint can now be used with its `send()` function.

    > [!TIP] Reception Callback 
    > Add a receive callback function as second argument to `setup()`, so that the endpoint is notified of received messages addressed to it.
    > See [Callback Example](/examples/CallbackExample/) for information and examples on callback usage.
    >
    > If no callback is specified, received messages must be actively polled with the `poll()` function, 
    > see the [Basic Usage example's app_main](/examples/BasicUsage/main/main.cpp).


    > [!TIP] URI Generation
    > To get a unique central EID, use `DTN7::uriFromMac()` to generate a unique DTN URI based on the ESP32's MAC address. 
    > This URI can be used instead of a fixed URI when calling the `setup()` function. 
    > See [Basic Usage example's app_main](/examples/BasicUsage/main/main.cpp) for a usage example.
    
  
7. If Flash storage is used, a custom partition table must be created. See [Storage Options](###storage).

8. Use `Build, Flash and Monitor` (flame icon in the bottom row) to build the project, flash the ESP32 device and monitor its output.

    These step can also be run individually, see [Running an Example](/examples/README.md#running-an-example).


### Basic Usage Example
[The basic usage example](/examples/BasicUsage/) demonstrates how to facilitate communication between ESPs via DTN.
It includes how to set up the _Bundle Protocol Agent_ (BPA), how to register endpoints with the BPA, and how to send and receive data.
It provides additional examples for callback functions for receiving data from endpoints and using custom flash partition tables.

Although this example does not aim to show the interaction between multiple nodes, 
nodes running it will receive each other's bundles when using compatible CLAs and are in range of each other.

## Supported LoRa Hardware
[RadioLib](https://github.com/jgromes/RadioLib) is used for the interaction with the LoRa modem. 
Compatibility with **SX1276 LoRa modules** has been tested.
Other LoRa modules supported by RadioLib should be compatible but were not tested. 

> [!WARNING]
> The LoRa CLA is only supported on classical ESP32, as the SPI hardware interaction differs between ESP32 types.
> Other variants, like the ESP32-S3, may require adjustments of the [RadioLib HAL](/dtn7-esp/include/CLAs/LoRa/EspHal2.h).

The pin connections between the LoRa modem and the ESP32 are defined in `menuconfig`. 
The default values correspond to the [Heltec Wifi LoRa32(V2)](https://heltec.org/project/wifi-lora-32v2/).



## Features

### Routing
Two simple, broadcast-oriented routing strategies are included.

1. **Simple Broadcast Router**

    Bundles are broadcasted a given number of times with a given interval between consecutive broadcast attempts. 
    Does not include or require any neighborhood detection mechanism.
    
    In combination with non-broadcast CLAs, this router attempts to forward a bundle to each known node by consecutive unicasts.

    > [!NOTE]
    > This router is similar to the [SimpleEpidemicRouter](https://github.com/dtn7/dtn7zero/blob/main/dtn7zero/routers/simple_epidemic_router.py) from dtn7zero with some adaptations. 
    > A delay between broadcast attempts of each bundle and the ability to declare a transmission success after a set number of broadcasts has been added.

   
2. **Epidemic Router**

    The Epidemic Router checks whether any nodes are in range, to which a bundle has not been forwarded, before broadcasting. 
    This requires a _neighborhood discovery mechanism_ provided by the CLA, as provided by the **LoRa CLA in BPoL-compatible mode**.
   
    In combination with non-broadcast CLAs, this router attempts to forward a bundle to each known node by consecutive unicasts.
    
    This router declares the transmission of a bundle a success when a number of neighbors (defined in `menuconfig`) are expected have received it.


### Storage
There are different storage options for bundles waiting for transmission.
A comparison for performance and space efficiency of the different options can be found [here](/evaluation/storage/).

1. Flash memory bundle storage. 

   Our preferred storage method, because large numbers of bundles can be stored without using the internal ESP memory and the flash can be used as persistent storage between reboots. 
   Access time is increased compared to internal storage options.

   > [!IMPORTANT]
   > Requires the ESP32 device to have flash memory! 
   > 
   > A custom flash partition layout is required to make use of the available flash space.
   

2. ESP memory bundle storage.
    
    Simply stores a pre-defined maximum number of bundles in the internal memory. 
    Bundles are stored without encoding, so accessing bundles is fast but the required memory per bundle is highest.
    
    > [!IMPORTANT]
    > When the limit of storable bundles is exceeded, the access performance degrades drastically due to the automatic batch-wise search and deletion of the oldest bundles in memory to make space for new bundles. 

3. ESP memory, serialized bundle storage. 

    Bundles are serialized before storing them. This leads to an increase in the time required for storing and accessing bundles, but reduces the used memory.

    Memory usage can be limited dynamically by setting a minimum amount of heap space that needs to remain free, configurable in `menuconfig`. 

    > [!IMPORTANT]
    > When the memory limit is reached, the access performance degrades drastically due to the automatic search and deletion of the oldest bundles in memory to make space for new bundles. 
    
4. ESP memory, partially serialized bundle storage.
    
    Bundles are serialized before storing them together with non-serialized information like the receiving time. 
    in comparison to the fully serialized approach, this slightly reduces bundle access times while slightly increasing memory usage.

    Memory usage can be limited dynamically by setting a minimum amount of heap space that needs to remain free, configurable in `menuconfig`. 

    > [!IMPORTANT]
    > When the memory limit is reached, the access performance degrades drastically due to the automatic search and deletion of the oldest bundles in memory to make space for new bundles. 


### Convergence Layer Adapters (CLAs)
- [LoRa CLA](#lora-cla)
- [Serial CLA](#serial-cla)
- [BLE CLA](#ble-cla)


#### LoRa CLA

The LoRa CLA interacts with the LoRa modem to send and receive bundles over it. 
Incoming bundles are directly put to the relevant processing queues after successful decoding.
The LoRa CLA provides duty cycle limitations compliant to EU regulation.

> [!TIP] We recommend using the LoRa CLA as reference for implementing your own CLA.

The included LoRa CLA can operate in either two modes:

1. CBOR-encoded bundle transmission (_non-BPoL-compatible mode_):

    Bundles are encoded as CBOR, as described in [RFC 9171](https://www.rfc-editor.org/rfc/rfc9171.pdf). 
    
    > [!NOTE]
    > Compatible to the LoRa CLA in [dtn7zero](https://github.com/dtn7/dtn7zero). 
    

2. Protobuf-encoded bundle transmission (_BPoL-compatible mode_)
    
    Bundles are encoded as protobuf messages instead of CBOR to be compatible to [BPoL](https://github.com/BigJk/dtn7-rs-lora-ecla/).

    In **BPoL-compatible mode**, the LoRa CLA also features a BPoL-compatible advertisement process for neighborhood discovery. Within a given interval, nodes announce their presence to other nodes nearby through the sending of advertisment messages (ADV).

    > [!IMPORTANT] Experimental Feature: Advertising node position.
    > With a working GPS module (cf. [extras](#gps)), a node's position can be included in the ADV.
    > In the current implementation, node position is not read from received advertisments.

    > [!IMPORTANT] Experimental Feature: Advertise known BundleID hashes
    > The IDs of all known bundles are hashed and included in the ADV. 
    > This allows other nodes to determine known and unknown messages in their neighborhood. 
    >
    > This feature is very experimental. Can be enabled in `menuconfig` with the option "Use bundle IDs for reception confirmation". 
    > Only the **Epidemic Router** in combination with the **LoRa CLA** makes use of this feature.
    > See [extras](#reception-confirmation) for further details. 


#### Serial CLA

A simple poll-based serial CLA using the ESP's UART peripheral. 
Received bundles are not sent to queues but must be actively polled.
Can be used as reference for the design of poll-based CLA mechanisms.

The serial CLA can be used for testing purposes, e.g., when no LoRa modem is available.

#### BLE CLA

> [!WARNING] Work in progress
> The BLE CLA is a work in progress and subject to changes.

A CLA for _Bluetooth Low Energy_, using the ESP's NimBLE host controller.
This CLA discovers possible neighbors and requires transmissions to be addressed to a specific node.

> [!IMPORTANT]
> To use the BLE CLA, the following settings MUST be made in menuconfig:
>
> CONFIG_BT_ENABLED=y
> CONFIG_BT_NIMBLE_ENABLED=y

> [!TIP]
> To help simplify configuring the BLE controller in menuconfig, a ["sdkconfig.defaults" file](/dtn7idf/include/CLAs/ble/sdkconfig.defaults) is provided along with the BLE CLA.
> If the settings included in this are used as defaults for a project, it is ensured that the BLE CLA will function properly, and minor memory usage optimizations are implemented. 
> General information on "sdkconfig.defaults" can be found [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html#using-sdkconfig-defaults).



Each node is regularly switching between advertising their presence and scanning for peers in configurable intervals with a possible random offset.
Each node acts both as GATT server and client.
In the server role, the node advertises a GATT service and characteristic, to which data can be uploaded.
In the client role, the node connects to an upload characteristic of another node and writes a CBOR encoded bundle to it.

UUIDs for service and characteristic are defined in `menuconfig`.
Currently, it is expected that dtn peers use their node ID as BLE device name.
Therefore, peers are determined by checking their name, and it must begin with "dtn:" or "ipn:".

> [!TIP]
> As NimBLE produces many log outputs, it may be beneficial to reduce its log level by setting "NimBLE Host log verbosity"(BT_NIMBLE_LOG_LEVEL) to "warning" or "error".

> [!NOTE]
> This CLA uses a lot of memory (~150kB), mostly for the Nimble controller, this can potentially be reduce through optimized menuconfig settings, but should be kept in mind.


## Extras / Experimental Features

List of additional project features, outside of the scope for basic DTN functionality.

### GPS Module Support
Equip the ESP device with a GPS module and activate its use in `menuconfig`.
With a GPS lock — and in BPoL-compatible mode — the node includes its position in BPoL advertisments.
Tested with an NMEA GPS connected via serial.

> [!IMPORTANT]
> Reading position information from BPoL advertisements or any usage thereof is not implemented.

Furthermore, the GPS signal can be used to synchronize all nodes' local clocks to [dtn time](https://www.rfc-editor.org/rfc/rfc9171.html#name-dtn-time).
This allows DTN nodes to send bundles with a non-zero creation timestamp and without a bundle age block.


### Indirect Reception Confirmation
The **Epidemic Router** and the **LoRa CLA** provide the option to indirectly confirm whether bundle transmissions were successfully received by a node. 
For that, the advertisment messages (ADV) are extended with a list of hashes of received bundle IDs, specifically those that were received since the last ADV was sent out. 

The **Epidemic Router** evaluates incoming hash lists to contain the hashes of recently sent bundles. 
If those hashes are not contained, the bundle is marked as not transmitted (to that node), which leads to a repeated forwarding attempt. 
It those hashes are contained, the bundle is marked as having been transmitted to that node. 

> [!IMPORTANT]
> There are three parameters of interest: (1) the interval between ADV transmissions, (2) the interval in which bundles are evaluated for a successfull transmission, and (3) the time after which nodes are removed from the list of known neighboors when no ADV is received.
> These parameters need adjustment to maximize the benefit of this feature; however, an evaluation has not been done yet.


### Triggering Bundle Retry from CLA (BLE CLA only)
Normally, the scheduling of bundle retries in the routing mechanism is separated of the CLA. 
This experimental features adds the option to externally trigger the bundle retry process from the CLA.
For example, this would allow the CLA to deliberatly forward bundles to a newly discovered neighbor. 

Enable in menuconfig via `Notify retry Task`.


## :warning: Missing Features / Known or Possible Compatability Issues

- Status Reports:  
    The generation of Bundle Protocol status reports is not implemented.

- Separation of application agent in administrative and application elements:  
    Regarding [Figure 2 in RFC 9171](https://www.rfc-editor.org/rfc/rfc9171.pdf#fig-2), the application agent could be separated into an administrative element and an application-specific element. 
    The lack of separation results in administrative records being also delivered to the application.

- :altert: CRC checks:  
    [RFC 9171 Section 4.3.1](https://www.rfc-editor.org/rfc/rfc9171.html#name-primary-bundle-block) requires the primary block to use CRC or a 
    _BPSec Block Integrity_ block targeting the primary block. 
    At the current state, no CRC checksum is included during bundle encoding, no CRC checking is implemented during bundle decoding, and no BPSec extension blocks are included.

- Fragmentation:  
    Fragmented bundles can be processed, however, they cannot be delivered to local endpoints, as no reassembly is implemented.
    Similarly, bundles exceeding the maximum bundle size (TODO: define the size?) are not fragmented.

### BPoL-specific Issues
+ Reading position information from BPoL advertisements or any usage thereof is not implemented.
+ PingPong and config packets defined in BPoL are not implemented.
+ The reference BPoL implementation lacks a documentation of the format of the BundleID hashes, so compatibility of hashed IDs with BPoL is questionable.

### Serial Data Transfer Issues
Some computers have issues with the serial data transfer to the ESP32.
Formatting errors occuring in log outputs are a strong indicator for that issue.
This also affects flashing; if "unexplainable" errors occur, this might be the reason.
In such cases, only using a different computer for flashing and log output did help.



## Additional Information

### Custom Flash Partition Table
The use of flash storage is recommended to save space on the ESP's internal memory. 
This requires a custom partition table, e.g., [_partitions.csv_](/examples/BasicUsage/partitions.csv) from [the basic usage example](/examples/BasicUsage/).

> [!TIP]
> Espressif provides more information regarding [partition tables](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/partition-tables.html) and the [Partition Table Editor](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/additionalfeatures/partition-table-editor.html).


### Reception Callback Definitions
If defined for any endpoint, a reception callback is triggered upon bundle reception addressing that endpoint.

Reception callbacks are implemented as function pointers.
The called function must have the signature:
```c
void callback(std::vector<uint8_t> data, std::string destination, std::string source, PrimaryBlock primaryB)
```

1. `data` — bundle data payload, as a vector of bytes.
2. `destination` —  bundle destination URI, as a string. If the same callback is used for different endpoints, this can be used to identify the invoking endpoint.
3. `source` — bundle source URI, as a string.
4. `primaryB` —  bundle primary block. Contains all (meta) information about the bundle.

See the [Basic Usage Example](/examples/BasicUsage) for a basic definition of a reception callback.
See the specific [Callback Example](/examples/CallbackExample/) for more complex use cases, like multiple endpoints with individual callbacks and multiple different endpoints sharing one callback.

> [!IMPORTANT]
> Callbacks are running in a different task (thread) than the main function. 
> Observe thread safety when using shared resources!

<!--  DO WE REALLY NEED IT?
## Execution Structure
The execution of the bundle processing is split between multiple tasks. Queues are used for data transfer between tasks.
The diagram displays the tasks' behavior and their execution structure. 

![Task Structure](images/diagrams/primaryTasks.png)


An important part of the execution structure not shown in the diagram above is the behavior of CLAs which are not being polled by the "CLAPoll Task".
In this case the CLAs need to send their received Bundles to the "receiveQueue" themselves.
As an example of how this can be achieved see the following diagram of the LoRa CLA.

![LoRa reception](images/diagrams/loraClaReception.svg)
-->

## Further resources
- Original BPoL GitHub: https://github.com/BigJk/dtn7-rs-lora-ecla
- GitHub dtn7 project site: https://github.com/dtn7/

## To-Do

- Storage implementation documentation (in .cpp files, actual storage operations)
- Update RadioLib dependency to newer version
- CRC 

--- 
