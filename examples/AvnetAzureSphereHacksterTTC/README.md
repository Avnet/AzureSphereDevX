# Avnet Azure Sphere Starter Kit Out of Box Example
This is a port of the Avnet Azure Sphere Starter Kit Out of Box Example.  This example implements the same features as previous OOB examples but using the DevX Azure Sphere software enablement libraries.
  
This sample application was developed to demonstrate the Avnet Azure Sphere Starter Kit and DevX. 

The Starter Kit is available for purchase [here](http://avnet.me/mt3620-kit).

We've put together four different blogs to showcase the Avnet Azure Sphere Starter Kit and how it can be used for your next IoT project.  The blogs all leverage this example application.

* Blog #1: Simple non-connected demo
   * Reads on-board sensors every 1 second
   * Reports sensor readings to the Visual Studio debug console
   * [Link to blog](http://avnet.me/mt3620-kit-OOB-ref-design-blog)
* Blog #2: Hands-on connected demo using a generic IoT Hub and Time Series Insights
   * Must complete blog 1 demo before moving on to blog 2
   * Configures IoT Hub and Time Series Insights Azure resources
   * Manipulate the device twin
   * Visualize data using Time Series Insights
   * [Link to blog](http://avnet.me/mt3620-kit-OOB-ref-design-blog-p2)
* Blog #3: Hands-on, connected demo using IoT Central (this blog)
   * Must complete blog 1 before moving on to part 3
   * Walks the user though configuring a IoT Central Application to create a custom visualization and device control application
   * [Link to blog](http://avnet.me/mt3620-kit-OOB-ref-design-blog-p3)
* Advanced Blog: Hands-on, connected demo using IoT Central
   * Must complete blog 1 before moving on to the Advanced Blog
   * Adds OLED functionality to the Starter Kit
   * Walks through using a real-time Bare-Metal M4 application to read the on-board light sensor
   * Uses a IoT Central Template to quickly stand up a new IoT Central Application
   * [Link to blog](http://avnet.me/azsphere-tutorial)

## Application Features

* Supports multiple Azure Connectivity options
   * No Azure Connection (reads on-board sensors and outputs data to debug terminal)
   * IoT Hub Connection using the Azure Device Provisioning Service
   * IoT Hub Connection with Azure Plug and Play (PnP) functionality
   * Avnet IoTConnect Platform Connection

### Sensors

* Reads the Starter Kit on-board sensors
  * ST LPS22HH pressure sensor (I2C)
  * ST LSM6DSO 3D accelerometer and 3D gyroscope (I2C)
  * Everlight ALS-PT19 light sensor from the pre-built Azure RTOS application (ADC)

### Button Features

The Avnet Starter Kit includes two user buttons, ButtonA and ButtonB

* When using the optional OLED display
   * Button A: Move to the last OLED screen
   * Button B: Move to the next OLED screen

### Connected Features

**IMPORTANT**: For all **connected** build options this application requires customization before it will compile and run. Follow the instructions in this README and in IoTCentral.md and/or IoTHub.md to perform the necessary steps.

* Sends sensor readings up as telemetry
* Sends button press events as telemetry
* Implements Device Twins
   * Control all user LEDs
   * Control an optional Relay Click board in Click Socket #2
   * Configure custom message to display on an optional OLED display
* Implements three direct methods
  * ```setSensorPollTime {"pollTime": <integer > 0>}```
  * ```rebootDevice {"delayTime": <integer > 0>}```
  * ```haltApplication {}```
   
### Optional Hardware

The application supports the following optional hardware to enhance the example
* [0.96" I2C OLED LEC Display](https://www.amazon.com/gp/product/B06XRCQZRX/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
   * Verify that the pinout for your display matches the pinout on the Starter Kit
* [Relay Click Board](https://www.mikroe.com/relay-click)

## Build Options

The application can be configured for multiple different deployments.  Build options are defined in the build_options.h header file.  To enable an option remove the comment delimiters ```//``` before the ```#define``` statement. 

### IOT_HUB_APPLICATION
* Enble for IoTHub connected functionality

### USE_PNP 
The application supports a public PnP definition: [link to model](https://github.com/Azure/iot-plugandplay-models/blob/main/dtmi/avnet/mt3620starterkit-1.json)

* Enable to use the Azure IoTHub Plug and Play functionality
* Enable to connect to an Azure IoT Central application
* When using the PnP build
   * Setup your Azure Resources as you would for a DPS connectoin

### USE_IOT_CONNECT
* Enable to include the functionality required to connect to Avnet's IoTConnect platform

### OLED_1306
* Enable to include the functionality required to drive the optional OLED display

### M4_INTERCORE_COMMS
* Enable to include the functionality required to communicate with the partner M4 Realtime application that reads the on-board light sensor

## Prerequisites

The sample requires the following software:

- Azure Sphere SDK version 21.07 or higher. At the command prompt, run **azsphere show-version** to check. Install [the Azure Sphere SDK](https://docs.microsoft.com/azure-sphere/install/install-sdk), if necessary.
- An Azure subscription. If your organization does not already have one, you can set up a [free trial subscription](https://azure.microsoft.com/free/?v=17.15).

## Preparation

**Note:** By default, this sample targets the [Avnet Azue Sphere Starter Kit Rev1](http://avnet.me/mt3620-kit) board. To build the sample for the Rev2 board, change the Target Hardware Definition Directory in the CMakeLists.txt file.

* Rev1: ```set(AVNET TRUE "AVNET Azure Sphere Starter Kit Revision 1 ")```
* Rev2: ```set(AVNET_REV_2 TRUE "AVNET Azure Sphere Starter Kit Revision 2 ")```

1. Set up your Azure Sphere device and development environment as described in [Azure Sphere documentation](https://docs.microsoft.com/azure-sphere/install/overview).
1. Clone the Azure Sphere Samples repository on GitHub and navigate to the examples/AvnetAzureSphereHacksterTTC/HighLevelExampleApp folder.
1. Connect your Azure Sphere device to your computer by USB.
1. Add a Wi-Fi network on your Azure Sphere device and verify that it is connected to the internet.
1. Open an Azure Sphere Developer Command Prompt and enable application development on your device if you have not already done so:

   **azsphere device enable-development**

## Run the sample

- [Run the sample with Azure IoT Central](./HighLevelExampleApp/IoTCentral.md)
- [Run the sample with an Azure IoT Hub](./HighLevelExampleApp/IoTHub.md)
 
