#include "argtable3/argtable3.h"
#include "dtn7-esp.hpp"
#include "esp_console.h"
#include "esp_system.h"

#define PROMPT_STR "dtn7-esp"

Endpoint* nodeCentralEndpoint = nullptr;
bool isSetup = false;

/// @brief simple callback to print received messages
/// @param data
/// @param destination
/// @param source
/// @param primaryB
void endpointCallback(std::vector<uint8_t> data, std::string destination,
                      std::string source, PrimaryBlock primaryB) {
    printf("Endpoint %s received: %.*s\n", destination.c_str(), data.size(),
           data.data());
    return;
};

// setup command

static struct {
    struct arg_str* nodeID;
    struct arg_end* end;
} setup_args;

int setupDtn(int argc, char** argv) {
    if (isSetup) {
        ESP_LOGE("setupDtn", "Already setup");
        return 1;
    }
    printf("setupDtn: setting up dtn7-esp... \n");
    arg_parse(argc, argv, (void**)&setup_args);
    std::string nodeID;
    if (setup_args.nodeID->count != 0) {
        nodeID = std::string(setup_args.nodeID->sval[0]);
        printf("setupDtn: using given node id: %s \n", nodeID.c_str());
        EID testEID = EID::fromUri(nodeID);
        if (testEID.valid != true) {
            ESP_LOGE("setupDtn",
                     "Invalid node ID given. Must be in format \"dtn://xxx\" "
                     "or \"ipn://xxx:xxx\" ");
            return 1;
        }
    }
    else {
        nodeID = DTN7::uriFromMac();
    }
    nodeCentralEndpoint = DTN7::setup(nodeID, endpointCallback);
    isSetup = true;
    printf("setupDtn: setup done, nodeID:%s \n", nodeID.c_str());
    return 0;
}

esp_console_cmd_t setupCmd{
    .command = "setup",
    .help =
        "setup dtn7-esp. If a node ID is given in correct format, this will be "
        "used; otherwise, a unique node ID is be generated from the ESP's mac "
        "address. A callback which prints received messages is added by "
        "default.",
    .func = setupDtn,
    .argtable = &setup_args};

// send Command

static struct {
    struct arg_str* dest;
    struct arg_str* message;
    struct arg_end* end;
} sendMessage_args;

int sendMessage(int argc, char** argv) {
    arg_parse(argc, argv, (void**)&sendMessage_args);
    if (!isSetup) {
        ESP_LOGE("sendMessage",
                 "bundle protocol agent not set up, run setup command first");
        return 1;
    }
    if (sendMessage_args.dest->count == 0) {
        ESP_LOGE("sendMessage", "no destination EID given");
        return 1;
    }
    if (sendMessage_args.message->count == 0) {
        ESP_LOGE("sendMessage", "no message given");
        return 1;
    }
    std::string dest = std::string(sendMessage_args.dest->sval[0]);
    printf("sendMessage: Destination EID: %s \n", dest.c_str());
    EID testEID = EID::fromUri(dest);
    if (testEID.valid != true) {
        ESP_LOGE("sendMessage",
                 "Invalid destination EID given, must be in format "
                 "\"dtn://xxx\" or \"ipn://xxx:xxx\" ");
        return 1;
    }

    std::string message = std::string(sendMessage_args.message->sval[0]);
    nodeCentralEndpoint->sendText(message, dest);
    return 0;
}

esp_console_cmd_t sendMessageCmd{
    .command = "send",
    .help = "send a message as a bundle to a specified EID",
    .func = sendMessage,
    .argtable = &sendMessage_args};

// register endpoint command

int registerEndpoint(int argc, char** argv) {
    arg_parse(argc, argv, (void**)&setup_args);
    if (!isSetup) {
        ESP_LOGE("registerEndpoint", "BPA not set up");
        return 1;
    }
    if (setup_args.nodeID->count == 0) {
        ESP_LOGE("registerEndpoint", "endpoint needs to have an EID");
        return 1;
    }
    printf("registerEndpoint: registering new endpoint \n");
    arg_parse(argc, argv, (void**)&setup_args);
    if (setup_args.nodeID->count != 0) {
        std::string eid = std::string(setup_args.nodeID->sval[0]);
        printf("registerEndpoint: using given node id: %s \n", eid.c_str());
        EID testEID = EID::fromUri(eid);
        if (testEID.valid != true) {
            ESP_LOGE("registerEndpoint",
                     "Invalid EID given, must be in format \"dtn://xxx\" or "
                     "\"ipn://xxx:xxx\" ");
            return 1;
        }
        DTN7::registerEndpoint(eid, endpointCallback);
        return 0;
    }

    return 2;
}

esp_console_cmd_t registerEndpointCmd{
    .command = "registerEndpoint",
    .help =
        "Register an endpoint with the BPA. An EID in correct format must be "
        "given. The endpoint will have as simple callback which prints "
        "received data to the console.",
    .func = registerEndpoint,
    .argtable = &setup_args};

void setupConsole() {
    esp_console_repl_t* repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    /* Register commands */
    esp_console_register_help_command();

    // register setup command
    setup_args.nodeID = arg_str0(NULL, NULL, "<nodeID>", "node ID to use");
    setup_args.end = arg_end(1);
    esp_console_cmd_register(&setupCmd);

    // register send command
    sendMessage_args.dest = arg_str0(NULL, NULL, "<EID>", "destination EID");
    sendMessage_args.message = arg_str0(NULL, NULL, "<message>",
                                        "message to send. Enclose the message "
                                        "with \" \" if it contains spaces. ");
    sendMessage_args.end = arg_end(2);
    esp_console_cmd_register(&sendMessageCmd);

    // register registerEndpoint command
    setup_args.nodeID =
        arg_str0(NULL, NULL, "<EID>", "EID to use for the endpoint");
    setup_args.end = arg_end(1);
    esp_console_cmd_register(&registerEndpointCmd);

    // setup console target hardware
    esp_console_dev_uart_config_t hw_config =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return;
}