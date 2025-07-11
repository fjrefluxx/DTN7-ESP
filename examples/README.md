# Examples


## [Basic Usage](/examples/BasicUsage/)

Explore the options available in menuconfig and learn the core functions.

- Interaction with the Bundle Protocol Agent
- Sending and receiving messages via BP
- Configuring and using a custom flash memory partition table


## [Callback Example](/examples/CallbackExample/)

- Extended example how to define and use callbacks
- Using callbacks on multiple endpoints
- Generation of reply messages from within a callback


## [Console Example](/examples/console/)

- Simple console setup to interact with the BPA
- Sending and receiving messages 



## [Template Project](/examples/TemplateProject/)
A minimal, ready-to-use project setup. A good starting point for application development.


## Running an example

The example projects contain a pre-configured ESP-IDF project, which must be built and deployed. 
A summary guideline for Visual Studio Code is provided below
See espressif's [build your first project guideline](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#build-your-first-project) for other options.


### :bulb: Deploying via the ESP-IDF extension in Visual Studio Code

Have the [ESP-IDF Extension for VS Code](https://github.com/espressif/vscode-esp-idf-extension) installed and configured.

1. Clone the repository and open an example root folder
2. Build project – click the "Build Project" button.  
    An initial build downloads all dependencies, this may take a while.
3. (optional) View and change the configuration options.  
    Click the "SDK Configuration Editor (menuconfig)" button (gear icon at the bottom) to show all available configuration options.
    Press the save button on the top right to save any changed configurations.
    Changes in the configuration requires the project to be built again (repeat step 2).
4. Flash the ESP device.  
    Press the "Flash device" button. 
    The first flash operation asks for the flash method and which serial port to use.
5. Monitor the device's serial output via the "Monitor Device" function.

> [!TIP]
> Building, flashing, and monitoring can be run as a single operation using the "Build, Flash and Monitor" button.

