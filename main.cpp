#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string>
#include <sstream>
#include <vector>

#include "mbed.h"
#include "mbedtls/entropy_poll.h"
#include "ublox_modem_driver/UbloxCellularDriverGenAtData.h"
#include "simpleclient.h"
#include "security.h"

// Status indication
DigitalOut red_led(LED_RED);

Ticker status_ticker;
void blinky() {
    red_led = !red_led;
}

// Resource values for the Device Object
struct MbedClientDevice device = {
        "Manufacturer", // Manufacturer
        "Type",         // Type
        "ModelNumber",  // ModelNumber
        "SerialNumber"  // SerialNumber
        };

// Instantiate the LWM2M Client API class
MbedClient mbed_client(device);

// Instantiate the Cellular class
UbloxCellularDriverGenAtData cell;

int main() {

    status_ticker.attach_us(blinky, 250000);

    unsigned int seed;
    size_t len;

#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT
    // Used to randomize source port
    mbedtls_hardware_poll(NULL, (unsigned char *) &seed, sizeof seed, &len);
#elif defined MBEDTLS_TEST_NULL_ENTROPY
    // Used to randomize source port
    mbedtls_null_entropy_poll(NULL, (unsigned char *) &seed, sizeof seed, &len);
#else
#error "This hardware does not have entropy, endpoint will not register to Connector."
#endif

    srand(seed);

    cell.set_sim_pin(MBED_CONF_APP_CELL_PIN);
    cell.set_credentials(MBED_CONF_APP_CELL_APN, MBED_CONF_APP_CELL_USER, MBED_CONF_APP_CELL_PASS);
    cell.connect();

    // Create endpoint interface to manage register and unregister
    mbed_client.create_interface("coap://api.connector.mbed.com:5684", &cell);

    // Create Objects of varying types, see simpleclient.h for more details on implementation.
    M2MSecurity *register_object = mbed_client.create_register_object(); // server object specifying connector info
    M2MDevice *device_object = mbed_client.create_device_object();       // device resources object

    // Create list of Objects to register
    M2MObjectList object_list;

    // Add objects to list
    object_list.push_back(device_object);

    // Set endpoint registration object
    mbed_client.set_register_object(register_object);

    // Register on the server
    mbed_client.test_register(register_object, object_list);

    while (true) {

        Thread::wait(25000);

        // Cellular keepalive
        cell.connect();

        if (!cell.is_connected()) {
            continue;
        }

        if (!mbed_client.register_successful()) {
            mbed_client.test_register(register_object, object_list);
            continue;
        }

        mbed_client.test_update_register();
    }

    // Not reached
    mbed_client.test_unregister();
    cell.disconnect();
    status_ticker.detach();
}

