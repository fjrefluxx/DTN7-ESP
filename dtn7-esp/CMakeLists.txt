idf_component_register(SRCS 
                        "src/Routing/BroadcastRouter.cpp"
                        "src/Routing/Router.cpp" 
                        "src/Routing/EpidemicRouter.cpp" 
                        "src/BundleProtocolAgent.cpp" 
                        "src/Data.cpp" 
                        "src/dtn7-esp.cpp"
                        "src/Endpoint.cpp" 
                        
                        "src/Storage/FlashStorage.cpp"
                        "src/Storage/InMemoryStorage.cpp" 
                        "src/CLAs/LoRaCLA.cpp" 
                        "src/CLAs/SerialCLA.cpp" 
                        "src/CLAs/BLE/BLE_CLA.cpp"
                        "src/CLAs/BLE/BLEhandling.cpp" 
                        "src/Storage/StorageSerialized.cpp" 
                        "proto-c/protocol.pb-c.c"
                        "protobuf-c/protobuf-c.c" 

                    REQUIRES freertos nvs_flash esp_hw_support lwip esp_rom esp_timer driver bt
                    INCLUDE_DIRS 
                        "include"
                        "include/CLAs"
                        "include/CLAs/LoRa"
                        "include/CLAs/BLE"
                        "include/Misc"
                        "include/Routing"
                        "include/Storage"
                    )
