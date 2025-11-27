# DTN7-esp Config {#menuconfig_options}
## Basic Settings

### Router Type
<br>**default** Simple Broadcast

Select the type of router to be used

Options:<br>
**Simple Broadcast**: Basic epidemic style router, works with all CLAs and, if ones that broadcasts bundles are present, periodically broadcasts each bundle. See [main readme "Routing"](/README.md#routing) for more information.

**Epidemic Router**:  more advanced epidemic router, requires information about network peers;  therefore, CLAs need to implement an advertisement mechanism. See [main readme "Routing"](/README.md#routing) for more information.



### Time Interval in seconds in which to retry stored bundles

<br>**default** 400

### Time Interval in seconds in which CLAs not using the queue system are polled

<br>**default** 10

###  Attach Previous Node Block
Select whether to attach a previous node block to bundles.
<br>**default** FALSE
    

### Attach Hop Count Block
 Select whether to attach a hop count block to bundles.
<br>**default** FALSE


### Default HopLimit
If *Attach Hop Count Block* is enabled (only then is this option visible), what hop limit should be set?

<br> **range** 1-255
<br>**default** 5

### Accurate Clock
Select whether the node has an accurate Clock, synchronized to DTN time, if set to false, a bundle age block will be added to locally generated bundles
<br>**default** FALSE

### Use GPS
Choose if GPS is to be used for time synchronization (the local clock will be synchronized to DTN time, as defined in RFC9171). Additionally, GPS is used to track the position of the local node.
<br>**default** FALSE

### GPS UART Number
If *Use GPS* is enabled (only then is this option visible), choose the UART peripheral to use.
<br>**default** 1
<br> **range** 0-2

### GPS UART RX pin
If *Use GPS* is enabled (only then is this option visible), select the pin used as UART RX (connect GPS module TX pin to the specified pin).
<br>**default** 16

### GPS Update Interval
If *Use GPS* is enabled (only then is this option visible), select the interval in seconds in which GPS position is read and updated.
<br>**default** 600

### LifeTime
The Bundle Lifetime to be set by default for locally generated bundles, in ms.
<br>**default** 86400000

### Send Status Report
Whether the node should send status reports if requested, NOT IMPLEMENTED! This setting as no effect.
<br>**default** FALSE

### Maximum Peer Age
 Maximum Time a node is stored in the list of known Peers after not being seen, in seconds.
<br>**default** 600
   
### Override Bundle Lifetime
If this setting is enabled, the lifetime stored in the Primary Block of bundles is ignored, and a custom value is used instead.
<br>**default** false

### Lifetime in Seconds
If *Override Bundle Lifetime* is enabled (only then is this option visible), this setting determines which lifetime, in seconds, is used.
<br>**default** 600

### Use Bundle ID's for reception Confirmation
If enabled, the local node stores a set of received bundle ID hashes. The hashes of the IDs of all received bundles are inserted into this set, which is emptied during each BPoL advertisement.
Additionally, each node in the forwarded-to list of stored bundles has a set of bundle ID hashes that is used to check if the last transmission of the bundle to the node was successful.
Enabling this affects: EpidemicRouter::checkForwardedTo(), encodeAdvertisePacket(), DTN7::bundleReceiver(), Node (Class), Node::serialize(), BundleProtocolAgent::bundleReception().
Only compatible with **EpidemicRouter** and **BPoL compatible LoRa CLA**.
For more information see [main readme "experimental features"](/README.md#extras--experimental-features).

<br>**default** False

### CRC Selection
#### CRC Type for Primary Blocks generated at this Node
See RFC9172 Section 4.2.1 for details on the different CRC Types, 0 = No CRC, 1 = CRC16, 2 = CRC32C.
<br>**default** 1
<br>**range** 0 2


#### CRC Type for Canonical Blocks generated at this Node
See RFC9172 Section 4.2.1 for details on the different CRC Types, 0 = No CRC, 1 = CRC16, 2 = CRC32C.
<br>**default** 0
<br>**range** 0 2


## Storage Config
### Storage Type
Select the storage implementation to be used. For more information regarding each storage type see [main readme "storage"](/README.md#storage).

<br>**default** InMemory Serialized

#### Flash
Choose this to use flash storage. To use flash storage a custom flash partition table should be used, otherwise it is not reasonably usable.

#### InMemory
Choose this to use In memory storage with class objects.
#### InMemory Serialized
Choose this to use In memory storage with serialized objects.
#### InMemory Serialized Improved Access
Choose this to use In memory storage with serialized objects and replacement speed improvement.

#### No Storage,
Use a dummy storage which does not actually stores bundles, useful for testing without influence from storage performance.



### Retry Batch Size
The amount of bundles that should be read from storage at a time when retrying delayed bundles.
<br>**default** 5
    
### Flash Storage
#### Keep Bundles Between Restarts
Whether stored bundles are persistent between restarts of the ESP.
If this feature is enabled, the flash storage stores additional information, such as the start and end indices of its stored bundles in flash.
 These are then read on boot and the data partition is not erased.
<br>**default** FALSE

### InMemory Storage Config
#### Max Stored Bundles
The amount of Bundles to be stored locally when using InMemory Storage with class objects.
<br>**default** 100

### InMemory Storage Serialized (Improved Access) Config
#### Minimal Free Heap
The Minimal Heap in bytes that should be be kept free, when this is reached the InMemoryStorageSerialized will delete the oldest bundles.
The number of deleted bundles is set with *MaxRemovedBundles*
<br>**default** 40000


#### Max Removed Bundles
Defines how many bundles are to be removed if there is not enough space to delay a bundle
<br>**default** 5


## CLA selection

### Setup LoRa CLA
Whether to setup the LoRa CLA when setting up the BundleProtocolAgent
<br>**default** false

### Setup Serial CLA
Whether to setup the Serial CLA when setting up the BundleProtocolAgent
<br>**default** false

### Setup BLE CLA
Whether to setup the BLE CLA when setting up the BundleProtocolAgent
**Important:** to use the BLE CLA Bluetooth must be enabled in menuconfig and the "Host" option must be set to "NimBLE"
<br>**default** false

## Task/Queue Settings
### Receive Queue Size
Size (amount of elements which can be in the queue at each time) of the ReceiveQueue
<br>**default** 10

### Forward Queue Size
Size (amount of elements which can be in the queue at each time) of the ForwardQueue
<br>**default** 10

###  BundleReceiverStackSize
Stack size in bytes of the BundleReceiver task. Should be chosen large enough to handle stack needed by callbacks.
<br>**default** 8000



###  BundleRetryStackSize
Stack size in bytes of the BundleRetry task.
<br>**default** 8000

###  BundleForwarderStackSize
Stack size in bytes of the BundleForwarder task.
<br>**default** 12000


###  ClaPollStacksize
Stack size in Bytes of the CLA Poll Task.
<br>**default** 4000

###  BundleReceiverPriority
Task priority of the BundleReceiver task
<br>**default** 2


###  BundleRetryPriority
Task priority of the BundlePoll Task
<br>**default** 2

###  BundleForwarderPriority
Task priority of the BundleForwarder Task
<br>**default** 2

###  ClaPollPriority
Task priority of the CLA Poll Task
<br>**default** 2


#  CLA Config
## Lora CLA Config
### BPoL
#### enableBPoL
Set whether to send BPoL-compatible packets. If set to false, sent packets will be compatible with dtn7zero.         

<br>**default** TRUE

#### AdvertiseInterval
The time between BPoL advertisements in s.
<br>**default** 300


#### Include Position in BPol Advertisements
If *enableBPoL* is true (only then is this option visible), select whether the Node Position is to be included in the BPol advertise message, only works if GPS configured.
<br>**default** false


### LoRa Hardware
Select either one of the predefined ESP+LoRa development boards or custom if no appropriate option is present.
<br>**default** Custom<br>
Options:<br>
Heltec WiFi LoRa 32(V2)<br>
Heltec WiFi LoRa 32(V3)<br>
Lilygo LoRa32


#### LoRa Chip
Only visible if *LoRa Hardware* is set to custom.
Select the type of LoRa chip used
<br>**default** SX1276<br>
Options:<br>
SX1276<br>
SX1262<br>
LLCC68

#### SCK pin
Only visible if *LoRa Hardware* is set to custom.
<br>**default** 5

#### MISO pin
Only visible if *LoRa Hardware* is set to custom.
<br>**default** 19

#### MOSI pin
Only visible if *LoRa Hardware* is set to custom.
<br>**default** 27

####  NSS/CS pin
Only visible if *LoRa Hardware* is set to custom.
<br>**default** 18

#### DIO0 pin
Only visible if *LoRa Hardware* is set to custom.
DIO0 pin when using SX1276, otherwise DIO1.
<br>**default** 26


#### NRST pin
Only visible if *LoRa Hardware* is set to custom.
<br>**default** 14

#### DIO1/BUSY pin
Only visible if *LoRa Hardware* is set to custom.
<br>**default** 33

### LoRa Coding Settings
#### LoRa Frequency in kHz
<br>**default** 868100

#### Bandwidth
Sets the LoRa bandwidth in kHz
<br>**default** 125

#### CodingRate
 Sets the LoRa coding rate
<br>**default** 5

####  SpreadingFactor
Sets the LoRa spreading factor
<br>**default** 7
help 
    
####  PreambleLength
Sets the LoRa preamble length
<br>**default** 8

### Duty Cycle and power
#### Duty Cycle in Percent
Sets the LoRa duty cycle
<br>**default** 1

#### DutyCycleTime
Sets the time span over which the LoRa duty cycle is measured, in minutes.
<br>**default** 60
help
    
#### Tx Power In dBm
Sets the LoRa power in dBm, defaults to 13, as this is the allowed max for 868.1Mhz
<br>**range** 2 17
<br>**default** 13

    

## Serial Cla Config
### UART port number
UART hardware port number for serial CLA.
The available choices depend on the model of ESP32 used.
 

### UART baud Rate
UART baud rate for the serial CLA
<br>**range** 1200 115200
<br>**default** 9600
help
    
### UART RXD pin number
GPIO number for UART RX pin.
<br>**default** 5


### UART TXD pin number
GPIO number for UART TX pin.
<br>**default** 4

### Size Of the Uart RX Buffer
The size of the UART RX buffer
<br>**default** 1024

## BLE CLA config
### Minimum time in MS between BLE bundle send operations
send() operations which occur with less time in between each other than defined here are
delayed until the gap between messages reaches the time defined here
<br>**default** 1000

### Advertise time
Time in ms the BLE CLA spends in advertise mode. To this time a random offset is added at the beginning of each advertise interval.
<br>**default** 3000

### Scan time
Time in ms the BLE CLA spends in scan mode. To this time a random offset is added at the beginning of each scan interval.
<br>**default** 3000

### Max scan/advertise offset
This defines the upper bound of the random length variation of BLE scanning/advertising
<br>**default** 1000

### BLE max peer age
The minimum amount of time, in milliseconds, that a BLE peer must be not seen before it is removed from the list of current BLE peers. This is independent of DTN neighbor management.
<br>**default** 60000

### max BLE transmission attempts
Sets the maximal number of BLE transmission attempts to be made for each BLE CLA::send() call.
<br>**default** 3

### BLE service UUID
This defines the UUID of the GATT service used by the BLE CLA
<br>**range** 0x0000 0xFFFF
<br>**default** 0xFEE1

### BLE write UUID
This defines the UUID of the GATT write characteristic used by the BLE CLA
<br>**range** 0x0000 0xFFFF
<br>**default** 0xFEE2

# Router Config
## Simple Broadcast Router Config
### NumOfBroadcasts
Number of Broadcast attempts to be made before a transmissions is considered a success.
<br>**default** 5

### BroadcastGap
Sets the minimal time between to broadcast attempts of the same bundle in ms, default is equivalent to 3 minutes.
<br>**default** 180000
help
    
### MinNodesToForward
Sets the minimal Number of Nodes a bundle should be forwarded to, only non broadcast transmissions count toward this number.
<br>**default** 5


## Epidemic Router Config
### NumOfForwards
Number of nodes each bundle shall be forwarded to before a transmissions is considered a success.
<br>**default** 5

# Experimental Settings
## Notify retry Task
Set this to true to enable an experimental feature that allows the bundle retry task to be triggered independently of its periodic wakeup.
When enabled, the BLE CLA wakes up the retry task when it discovers a new peer.
<br>**default** false

    
