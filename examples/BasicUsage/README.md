# Basic Example Project using DTN7-ESP.
This project shows how to set up DTN7-ESP, and demonstrate how to interact with dtn endpoints for sending and receiving data.
It is demonstrated how to register an endpoint with the BPA and how to receive data with or without a callback.

This project also uses a custom flash partition table for the Flash storage which is configured to be enabled in menuconfig.
The ESP used for this example should have at least 4MB of flash, otherwise the custom flash table needs to be disabled.

Exploring the options available in menuconfig can help to understand all available configuration options.