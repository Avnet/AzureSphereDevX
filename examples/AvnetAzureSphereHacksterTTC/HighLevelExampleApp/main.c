/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 *   DISCLAIMER
 *
 *   The DevX library supports the Azure Sphere Developer Learning Path:
 *
 *	   1. are built from the Azure Sphere SDK Samples at
 *          https://github.com/Azure/azure-sphere-samples
 *	   2. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   3. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   4. are provided as is and as a convenience to aid the Azure Sphere Developer Learning
 *          experience.
 *
 *   DEVELOPER BOARD SELECTION
 *
 *   The following developer boards are supported.
 *
 *	   1. AVNET Azure Sphere Starter Kit.
 *     2. AVNET Azure Sphere Starter Kit Revision 2.
 *	   3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	   4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 *   ENABLE YOUR DEVELOPER BOARD
 *
 *   Each Azure Sphere developer board manufacturer maps pins differently. You need to select the
 *      configuration that matches your board.
 *
 *   Follow these steps:
 *
 *	   1. Open CMakeLists.txt.
 *	   2. Uncomment the set command that matches your developer board.
 *	   3. Click File, then Save to save the CMakeLists.txt file which will auto generate the
 *          CMake Cache.
 *
 ************************************************************************************************/

#include "hw/sample_appliance.h" // Hardware definition

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_json_serializer.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include "dx_version.h"
#include "applibs_versions.h"
#include <applibs/log.h>
#include <applibs/wificonfig.h>
#include <applibs/powermanagement.h>
#include "build_options.h"
#include "i2c.h"
#include "oled.h"


// Make sure we're using the IOT Hub code for the PNP configuration
#ifdef USE_PNP
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:avnet:mt3620Starterkit;1"
#else
#define IOT_PLUG_AND_PLAY_MODEL_ID ""
#endif

#define NETWORK_INTERFACE "wlan0"
#define SAMPLE_VERSION_NUMBER "1.0"
#define ONE_MS 1000000

// Globals
// Array with messages from Azure
char oled_ms1[CLOUD_MSG_SIZE];
char oled_ms2[CLOUD_MSG_SIZE];
char oled_ms3[CLOUD_MSG_SIZE];
char oled_ms4[CLOUD_MSG_SIZE];

// Variable to hold sensor readings.  We read the sensors very quickly,
// but only send telemetry when the send telemeter timer expires
// Global variables to hold the most recent sensor data
AccelerationgForce acceleration_g;
AngularRateDegreesPerSecond angular_rate_dps;
float lsm6dso_temperature;
float pressure_kPa;
float lps22hh_temperature;
float altitude;
network_var network_data;


// Forward declarations
//static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void dt_gpio_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void dt_oled_message_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void publish_message_handler(void);
static void read_sensors_handler(EventLoopTimer *eventLoopTimer);
static void monitor_wifi_network_handler(EventLoopTimer *eventLoopTimer);
static void delay_restart_timer_handler(EventLoopTimer *eventLoopTimer);
static DX_DIRECT_METHOD_RESPONSE_CODE dm_restart_device_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE dm_set_sensor_poll_period(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE dm_halt_device_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void ReadWifiConfig(bool outputDebug);

DX_USER_CONFIG dx_config;

/****************************************************************************************
 * Telemetry message buffer property sets
*****************************************************************************************/

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
#define JSON_MESSAGE_BYTES 512
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

static DX_MESSAGE_PROPERTY *messageProperties[] =   {&(DX_MESSAGE_PROPERTY){.key = "appid", .value = "SK-Demo"}, 
                                                    &(DX_MESSAGE_PROPERTY){.key = "type", .value = "telemetry"},
                                                    &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

/****************************************************************************************
 * GPIO Peripherals
 ****************************************************************************************/
static DX_GPIO_BINDING buttonA =      {.pin = SAMPLE_BUTTON_1,     .name = "buttonA",      .direction = DX_INPUT,   .detect = DX_GPIO_DETECT_LOW};
static DX_GPIO_BINDING buttonB =      {.pin = SAMPLE_BUTTON_2,     .name = "buttonB",      .direction = DX_INPUT,   .detect = DX_GPIO_DETECT_LOW};
static DX_GPIO_BINDING userLedRed =   {.pin = SAMPLE_RGBLED_RED,   .name = "userLedRed",   .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING userLedGreen = {.pin = SAMPLE_RGBLED_GREEN, .name = "userLedGreen", .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING userLedBlue =  {.pin = SAMPLE_RGBLED_BLUE,  .name = "userLedBlue",  .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING wifiLed =      {.pin = SAMPLE_WIFI_LED,     .name = "WifiLed",      .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING appLed =       {.pin = SAMPLE_APP_LED,      .name = "appLed",       .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING clickRelay1 =  {.pin = RELAY_CLICK2_RELAY1, .name = "relay1",       .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = false};
static DX_GPIO_BINDING clickRelay2 =  {.pin = RELAY_CLICK2_RELAY2, .name = "relay2",       .direction = DX_OUTPUT,  .initialState = GPIO_Value_Low, .invertPin = false};

/****************************************************************************************
 * Device Twins
 ****************************************************************************************/
// Read/Write Device Twin Bindings
static DX_DEVICE_TWIN_BINDING dt_user_led_red =        {.propertyName = "userLedRed",       .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &userLedRed};	
static DX_DEVICE_TWIN_BINDING dt_user_led_green =      {.propertyName = "userLedGreen",     .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &userLedGreen};
static DX_DEVICE_TWIN_BINDING dt_user_led_blue =       {.propertyName = "userLedBlue",      .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &userLedBlue};
static DX_DEVICE_TWIN_BINDING dt_wifi_led =            {.propertyName = "wifiLed",          .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &wifiLed};
static DX_DEVICE_TWIN_BINDING dt_app_led =             {.propertyName = "appLed",           .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &appLed};
static DX_DEVICE_TWIN_BINDING dt_relay1 =              {.propertyName = "clickBoardRelay1", .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &clickRelay1};
static DX_DEVICE_TWIN_BINDING dt_relay2 =              {.propertyName = "clickBoardRelay2", .twinType = DX_DEVICE_TWIN_BOOL,  .handler = dt_gpio_handler, .context = &clickRelay2};
static DX_DEVICE_TWIN_BINDING dt_desired_sample_rate = {.propertyName = "sensorPollPeriod", .twinType = DX_DEVICE_TWIN_INT,   .handler = dt_desired_sample_rate_handler};
static DX_DEVICE_TWIN_BINDING dt_oled_line1 =          {.propertyName = "OledDisplayMsg1", .twinType = DX_DEVICE_TWIN_STRING, .handler = dt_oled_message_handler, .context = oled_ms1 };
static DX_DEVICE_TWIN_BINDING dt_oled_line2 =          {.propertyName = "OledDisplayMsg2", .twinType = DX_DEVICE_TWIN_STRING, .handler = dt_oled_message_handler, .context = oled_ms2 };
static DX_DEVICE_TWIN_BINDING dt_oled_line3 =          {.propertyName = "OledDisplayMsg3", .twinType = DX_DEVICE_TWIN_STRING, .handler = dt_oled_message_handler, .context = oled_ms3 };
static DX_DEVICE_TWIN_BINDING dt_oled_line4 =          {.propertyName = "OledDisplayMsg4", .twinType = DX_DEVICE_TWIN_STRING, .handler = dt_oled_message_handler, .context = oled_ms4 };

// Read only Device Twin Bindings
static DX_DEVICE_TWIN_BINDING dt_version_string = {.propertyName = "versionString", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_manufacturer =   {.propertyName = "manufacturer", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_model =          {.propertyName = "model", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_ssid =           {.propertyName = "ssid", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_freq =           {.propertyName = "freq", .twinType = DX_DEVICE_TWIN_INT};
static DX_DEVICE_TWIN_BINDING dt_bssid =          {.propertyName = "bssid", .twinType = DX_DEVICE_TWIN_STRING};

/****************************************************************************************
 * Direct Methods
 ****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_sensor_poll_time = {.methodName = "setSensorPollTime", .handler = dm_set_sensor_poll_period};
static DX_DIRECT_METHOD_BINDING dm_reboot_control =      {.methodName = "rebootDevice", .handler = dm_restart_device_handler};
static DX_DIRECT_METHOD_BINDING dm_halt_control =       {.methodName = "haltApplication", .handler = dm_halt_device_handler};

/****************************************************************************************
 * Timers
 ****************************************************************************************/
static DX_TIMER_BINDING tmr_monitor_wifi_network = {.period = {30, 0}, .name = "tmr_monitor_wifi_network", .handler = monitor_wifi_network_handler};
static DX_TIMER_BINDING tmr_read_sensors = {.period = {SENSOR_READ_PERIOD_SECONDS, 0}, .name = "tmr_read_sensors", .handler = read_sensors_handler};
static DX_TIMER_BINDING tmr_reboot = {.period = {0, 0}, .name = "tmr_reboot", .handler = delay_restart_timer_handler};


// All bindings referenced in the folowing binding sets are initialised in the InitPeripheralsAndHandlers function
DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {&dt_user_led_red, &dt_user_led_green, &dt_user_led_blue, &dt_wifi_led, 
                                                  &dt_app_led, &dt_relay1, &dt_relay2, &dt_desired_sample_rate, &dt_oled_line1, 
                                                  &dt_oled_line2, &dt_oled_line3, &dt_oled_line4,
                                                  &dt_version_string, &dt_manufacturer, &dt_model, &dt_ssid, &dt_freq, &dt_bssid};

DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {&dm_reboot_control, &dm_sensor_poll_time, &dm_halt_control};
DX_GPIO_BINDING *gpio_bindings[] = {&buttonA, &buttonB, &userLedRed, &userLedGreen, &userLedBlue, &wifiLed, &appLed, &clickRelay1, &clickRelay2};
DX_TIMER_BINDING *timer_bindings[] = {&tmr_monitor_wifi_network, &tmr_read_sensors, &tmr_reboot};


/****************************************************************************************
 * Implementation
 ****************************************************************************************/

static void publish_message_handler(void)
{

#ifdef IOT_HUB_APPLICATION
#ifdef USE_IOT_CONNECT
    // If we have not completed the IoTConnect connect sequence, then don't send telemetry
    if(IoTCConnected){
#endif         

        // Keep track of the first time through this code.  The LSMD6S0 returns bad data the first time
        // we read it.  Don't send that data up in case we're charting the data.
        static bool firstPass = true;

        if(!firstPass){

            #define JSON_BUFFER_SIZE 256

            if (dx_isAzureConnected()) {


//#define USE_DEVX_SERIALIZATION
#ifdef USE_DEVX_SERIALIZATION

                // Serialize telemetry as JSON
                bool serialization_result = dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 11, 
                    DX_JSON_DOUBLE, "gX", acceleration_g.x,
                    DX_JSON_DOUBLE, "gY", acceleration_g.y,
                    DX_JSON_DOUBLE, "gZ", acceleration_g.z,
                    DX_JSON_DOUBLE, "aX", angular_rate_dps.x,
                    DX_JSON_DOUBLE, "aY", angular_rate_dps.y,
                    DX_JSON_DOUBLE, "aZ", angular_rate_dps.z,
                    DX_JSON_DOUBLE, "pressure", pressure_kPa,
                    DX_JSON_DOUBLE, "light_intensity", light_sensor,
                    DX_JSON_DOUBLE, "altitude", altitude,
                    DX_JSON_DOUBLE, "temp", lsm6dso_temperature,
                    DX_JSON_INT, "rssi", network_data.rssi);

                if (serialization_result) {

                    Log_Debug("\n%s\n", msgBuffer);
                    dx_azurePublish(msgBuffer, strlen(msgBuffer), messageProperties, NELEMS(messageProperties), &contentProperties);

                } else {
                    Log_Debug("JSON Serialization failed: Buffer too small\n");
                }

#else // !USE_DEVX_SERIALIZATION

                snprintf(msgBuffer, sizeof(msgBuffer),
                    "{\"gX\":%.2lf, \"gY\":%.2lf, \"gZ\":%.2lf, \"aX\": %.2f, \"aY\": "
                    "%.2f, \"aZ\": %.2f, \"pressure\": %.2f, \"light_intensity\": %.2f, "
                    "\"altitude\": %.2f, \"temp\": %.2f,  \"rssi\": %d}",
                    acceleration_g.x, acceleration_g.y, acceleration_g.z, angular_rate_dps.x,
                    angular_rate_dps.y, angular_rate_dps.z, pressure_kPa, light_sensor, altitude,
                    lsm6dso_temperature, network_data.rssi);                

                    Log_Debug("\n%s\n", msgBuffer);
                    dx_azurePublish(msgBuffer, strlen(msgBuffer), messageProperties, NELEMS(messageProperties), &contentProperties);
#endif                     
            }
        }
    else{
        // It is the first pass, flip the flag
        firstPass = false;

        // On the first pass set the OLED screen to the Avnet graphic!
#ifdef OLED_SD1306
    	oled_state = OLED_NUM_SCREEN;
#endif 
        }
#ifdef USE_IOT_CONNECT        
    }
#endif // USE_IOT_CONNECT
#endif // IOT_HUB_APPLICATION    

}


static void monitor_wifi_network_handler(EventLoopTimer *eventLoopTimer)
{

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    ReadWifiConfig(true);
}

static void read_sensors_handler(EventLoopTimer *eventLoopTimer)
{

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    acceleration_g = lp_get_acceleration();
    Log_Debug("\nLSM6DSO: Acceleration [g]  : %.4lf, %.4lf, %.4lf\n", acceleration_g.x,
              acceleration_g.y, acceleration_g.z);

    angular_rate_dps = lp_get_angular_rate();
    Log_Debug("LSM6DSO: Angular rate [dps] : %4.2f, %4.2f, %4.2f\n", angular_rate_dps.x,
              angular_rate_dps.y, angular_rate_dps.z);

    lsm6dso_temperature = lp_get_temperature();
    Log_Debug("LSM6DSO: Temperature1 [degC]: %.2f\n", lsm6dso_temperature);

  	if (lps22hhDetected) {

        pressure_kPa = lp_get_pressure();
        lps22hh_temperature = lp_get_temperature_lps22h();
    
		Log_Debug("LPS22HH: Pressure     [kPa] : %.2f\n", pressure_kPa);
        Log_Debug("LPS22HH: Temperature2 [degC]: %.2f\n", lps22hh_temperature);
    }
    // LPS22HH was not detected
    else {

        Log_Debug("LPS22HH: Pressure     [kPa] : Not read!\n");
        Log_Debug("LPS22HH: Temperature  [degC]: Not read!\n");
    }

#ifdef M4_INTERCORE_COMMS
    Log_Debug("ALSPT19: Ambient Light[Lux] : %.2f\r\n", light_sensor);
#endif

    // Read the current wifi configuration
    ReadWifiConfig(false);

    // The ALTITUDE value calculated is actually "Pressure Altitude". This lacks correction for
    // temperature (and humidity)
    // "pressure altitude" calculator located at:
    // https://www.weather.gov/epz/wxcalc_pressurealtitude "pressure altitude" formula is defined
    // at: https://www.weather.gov/media/epz/wxcalc/pressureAltitude.pdf altitude in feet =
    // 145366.45 * (1 - (hPa / 1013.25) ^ 0.190284) feet altitude in meters = 145366.45 * 0.3048 *
    // (1 - (hPa / 1013.25) ^ 0.190284) meters
    //
    // weather.com formula
    // altitude = 44307.69396 * (1 - powf((atm / 1013.25), 0.190284));  // pressure altitude in
    // meters
    // Bosch's formula
    altitude = 44330 * (1 - powf(((float)(pressure_kPa * 1000 / 1013.25)),
                                       (float)(1 / 5.255))); // pressure altitude in meters

    // Send the latest readings up as telemetry
    publish_message_handler();
}


static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    int sample_rate_seconds = *(int *)deviceTwinBinding->propertyValue;

    // validate data is sensible range before applying
    if (sample_rate_seconds >= 0 && sample_rate_seconds <= 12*60*60) {

        dx_timerChange(&tmr_read_sensors, &(struct timespec){sample_rate_seconds, 0});

        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_COMPLETED);

    } else {
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_ERROR);
    }

    /*	Casting device twin state examples

            float value = *(float*)deviceTwinBinding->propertyValue;
            double value = *(double*)deviceTwinBinding->propertyValue;
            int value = *(int*)deviceTwinBinding->propertyValue;
            bool value = *(bool*)deviceTwinBinding->propertyValue;
            char* value = (char*)deviceTwinBinding->propertyValue;
    */
}

static void dt_gpio_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    bool gpio_level = *(bool *)deviceTwinBinding->propertyValue;
    DX_GPIO_BINDING *gpio = (DX_GPIO_BINDING*)deviceTwinBinding->context;
    
    if(gpio_level){
        dx_gpioOn(gpio);
    }
    else{
        dx_gpioOff(gpio);
    }

    dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_COMPLETED);
}

static void dt_oled_message_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    bool message_processed = true;
    
    // Verify we have a pointer to the global variable for this oled message
    if(deviceTwinBinding->context != NULL){

        char* ptr_oled_variable = (char*)deviceTwinBinding->context;
        char* new_message = (char*)deviceTwinBinding->propertyValue;

        // Is the message size less than the destination buffer size and printable characters
        if (strlen(new_message) < CLOUD_MSG_SIZE && dx_isStringPrintable(new_message)) {
            strncpy(ptr_oled_variable, new_message, CLOUD_MSG_SIZE);
            Log_Debug("New Oled Message: %s\n", ptr_oled_variable);
            dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_COMPLETED);
        } else {
            message_processed = false;
        }

    } else {
        message_processed = false;
    }

    if (!message_processed){

        Log_Debug("Local copy failed. String too long or invalid data\n");
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_ERROR);
    }
}

static void NetworkConnectionState(bool connected)
{
    static bool first_time = true;

    if (first_time && connected) {
        first_time = false;

        // This is the first connect so update device start time UTC and software version
        if (dx_isAzureConnected()) {

            dx_deviceTwinReportValue(&dt_version_string, "AvnetSK-V2-DevX");
            dx_deviceTwinReportValue(&dt_manufacturer, "Avnet");
            dx_deviceTwinReportValue(&dt_model, "Avnet Starter Kit");
        }
    }
}

// Read the current wifi configuration, output it to debug and send it up as device twin data
static void ReadWifiConfig(bool outputDebug){

    #define BSSID_SIZE 20
    char bssid[BSSID_SIZE];

	WifiConfig_ConnectedNetwork network;
	int result = WifiConfig_GetCurrentNetwork(&network);

	if (result < 0) 
	{
	    // Log_Debug("INFO: Not currently connected to a WiFi network.\n");
		strncpy(network_data.SSID, "Not Connected", 20);
		network_data.frequency_MHz = 0;
		network_data.rssi = 0;
	}
	else 
	{

        network_data.frequency_MHz = network.frequencyMHz;
        network_data.rssi = network.signalRssi;
		snprintf(bssid, BSSID_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
			network.bssid[0], network.bssid[1], network.bssid[2], 
			network.bssid[3], network.bssid[4], network.bssid[5]);

        // Make sure we're connected to the IoTHub before updating the local variable or sending device twin updates
        if (dx_isAzureConnected()) {

            // Check to see if the SSID changed, if so update the SSID and send updated device twins properties
            if (strncmp(network_data.SSID, (char*)&network.ssid, network.ssidLength)!=0 ) {

                // Clear the ssid array
                memset(network_data.SSID, 0, WIFICONFIG_SSID_MAX_LENGTH);
                strncpy(network_data.SSID, network.ssid, network.ssidLength);

#ifdef IOT_HUB_APPLICATION
                // Note that we send up this data to Azure if it changes, but the IoT Central Properties elements only 
                // show the data that was currenet when the device first connected to Azure.
                dx_deviceTwinReportValue(&dt_ssid, &network_data.SSID);
                dx_deviceTwinReportValue(&dt_freq, &network_data.frequency_MHz);
                dx_deviceTwinReportValue(&dt_bssid, &bssid);

#endif // IOT_HUB_APPLICATION

                if(outputDebug){

                    Log_Debug("SSID: %s\n", network_data.SSID);
                    Log_Debug("Frequency: %dMHz\n", network_data.frequency_MHz);
                    Log_Debug("bssid: %s\n", bssid);
                    Log_Debug("rssi: %d\n", network_data.rssi);
                }

                // Since we're connected to a wifi network and have reported the details to the IoTHub
                // modify the timer frequency from the default of 30 seconds to every 5 minutes
                dx_timerChange(&tmr_monitor_wifi_network, &(struct timespec){60*5, 0});
            }
        }
    }
}

/// <summary>
///  Function for rebootDevice directMethod
///  name: rebootDevice
///  Payload: {"delayTime": 0 < delay in seconds > 12*60*60}
///  Start Reboot Device Direct Method 'RebootDevice' {"delayTime":15}
/// </summary>
static DX_DIRECT_METHOD_RESPONSE_CODE dm_restart_device_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    
    char delay_str[] = "delayTime";
    int requested_delay_seconds;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    // check JSON properties sent through are the correct type
    if (!json_object_has_value_of_type(jsonObject, delay_str, JSONNumber)) {
        return DX_METHOD_FAILED;
    }

    requested_delay_seconds = (int)json_object_get_number(jsonObject, delay_str);
    Log_Debug("Reboot Delay %d \n", requested_delay_seconds);

    if (IN_RANGE(requested_delay_seconds, 1, (12*60*60))) {

        // Set the timer to fire after the requested delayTime
        dx_timerOneShotSet(&tmr_reboot, &(struct timespec){requested_delay_seconds, 0});
        return DX_METHOD_SUCCEEDED;
    
    }
    else{
        return DX_METHOD_FAILED;
    }
}

/// <summary>
///  Function for rebootDevice directMethod
///  name: haltApplication
///  Payload: None
/// </summary>
static DX_DIRECT_METHOD_RESPONSE_CODE dm_halt_device_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    
    int requested_delay_seconds = HALT_APPLICATION_DELAY_TIME_SECONDS;
    Log_Debug("Reboot Delay %d \n", requested_delay_seconds);

    // Set the timer to fire after the requested delayTime
    dx_timerOneShotSet(&tmr_reboot, &(struct timespec){requested_delay_seconds, 0});
    return DX_METHOD_SUCCEEDED;
}


/// <summary>
/// Restart the Device
/// </summary>
static void delay_restart_timer_handler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    PowerManagement_ForceSystemReboot();
}

/// </summary>
///  name: setSensor
///  payload: {"pollTime": 0 > integer < 12 hours >}
/// </summary>
static DX_DIRECT_METHOD_RESPONSE_CODE dm_set_sensor_poll_period(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    
    char poll_str[] = "pollTime";
    int requested_poll_time_seconds;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    // check JSON properties sent through are the correct type
    if (!json_object_has_value_of_type(jsonObject, poll_str, JSONNumber)) {
        return DX_METHOD_FAILED;
    }

    requested_poll_time_seconds = (int)json_object_get_number(jsonObject, poll_str);
    Log_Debug("New Telemetry TX period %d \n", requested_poll_time_seconds);

    if (IN_RANGE(requested_poll_time_seconds, 1, (12*60*60))) {

        // Set the timer to fire after the requested delayTime
        dx_timerChange(&tmr_read_sensors, &(struct timespec){requested_poll_time_seconds, 0});
        return DX_METHOD_SUCCEEDED;
    
    }
    else{
        return DX_METHOD_FAILED;    
    }
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timer_bindings.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));

    dx_azureRegisterConnectionChangedNotification(NetworkConnectionState);

    // Initialize the i2c sensors
    lp_imu_initialize();

}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
    dx_deviceTwinUnsubscribe();
    dx_directMethodUnsubscribe();
    dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
    dx_timerEventLoopStop();
    void lp_imu_close(void);

}

int main(int argc, char *argv[])
{
    dx_registerTerminationHandler();

    Log_Debug("Avnet Starter Kit Simple Reference Application starting.\n");

    if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {
        return dx_getTerminationExitCode();
    }

    InitPeripheralsAndHandlers();

    // Main loop
    while (!dx_isTerminationRequired()) {
        int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == -1 && errno != EINTR) {
            dx_terminate(DX_ExitCode_Main_EventLoopFail);
        }
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return dx_getTerminationExitCode();
}
