# Console Example

This project includes a simple console application to interact with the BPA
Implemented commands:
- setup BPA
- send text as bundle
- register endpoint to print received text
- help command to show supported commands and their syntax

By default this example:
- uses a custom flash partition table (requires 4mb of Flash)
- enables BLuetooth using nimble as controller
- enables the BLE CLA
- sets the LOG level to "warn"