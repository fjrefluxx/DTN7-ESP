# Console Example

This project includes a simple console application to interact with the BPA 
Implemented commands:
- setup [\<nodeID>]

  Setup dtn7idf, if a node id is given, in correct format, see RFC9171 it will
  be used, otherwise a unique node id will be generated from the ESP's MAC
  address, a callback which prints received messages will be added by default

    \<nodeID> Node ID to use for this node

- send  [\<EID>] [\<message>]
  send a message as a bundle to a specified EID

    \<EID>  destination EID

    \<message>  message to be send, if the message contains a space, it must be enclosed by " ".

- registerEndpoint  [\<EID>]
  register an Endpoint with the BPA, an EID, in correct format, see RFC9171,
  must be given. The endpoint will have as simple callback which prints
  received data to the console

    \<EID>  EID to use for the endpoint

- help  [\<string>]
  Print the summary of all registered commands if no arguments are given,
  otherwise print summary of given command.

    \<string>  Name of command

# Additional Menuconfig Options
To Configure the behavior of the Console Example, the following menuconfig settings are present:

- Setup Bundle Protocol Agent on Startup

    This Option Allows the BPA to be set up without having to issue the "setup" command

    - Use Custom Node URI
    
        If the BPA is set up on Startup, the Node URI can be defined using this setting

        - Node URI
    - Register Additional Endpoint
    
        If the BPA is set up on Startup, this option can be used to register an additional Endpoint without having to use the "register" command
        - Additional Endpoint

# Adjusted Default Settings
This Example makes some default settings:
- set a custom flash partition table (requires 4mb of Flash)
- enable Bluetooth using nimble as controller
- enable the BLE CLA
- sets the LOG level to "warn"