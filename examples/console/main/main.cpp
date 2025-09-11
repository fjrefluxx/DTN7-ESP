#include <stdio.h>
#include "commands.h"

extern "C" void app_main(void) {

#if CONFIG_PreSetup
#if CONFIG_UseCustomNodeURI
    EID testEID_Node = EID::fromUri(CONFIG_NodeURI);
    if (testEID_Node.valid != true) {
        ESP_LOGE("PreSetup CustomNodeURI",
                 "Invalid EID given, must be in format \"dtn://xxx\" or "
                 "\"ipn://xxx:xxx\" ");
    }
    else {
        nodeCentralEndpoint = DTN7::setup(CONFIG_NodeURI, endpointCallback);
        isSetup = true;
    }
    
#else
    nodeCentralEndpoint = DTN7::setup(DTN7::uriFromMac(), endpointCallback);
#endif

#if CONFIG_RegisterAdditionalEndpoint
    EID testEID_Additional = EID::fromUri(CONFIG_AdditionalEndpoint);
    if (testEID_Additional.valid != true) {
        ESP_LOGE("PreSetup RegisterAdditionalEndpoint",
                 "Invalid EID given, must be in format \"dtn://xxx\" or "
                 "\"ipn://xxx:xxx\" ");
    }
    else {
        DTN7::registerEndpoint(CONFIG_AdditionalEndpoint, endpointCallback);
    }

#endif
#endif
    setupConsole();
}