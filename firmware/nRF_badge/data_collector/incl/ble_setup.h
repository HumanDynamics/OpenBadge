#ifndef BLE_SETUP_H
#define BLE_SETUP_H

#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "softdevice_handler.h"

#include "nrf_drv_config.h"

#include "debug_log.h"          //UART debugging logger

#include "ble_bas.h"  //battery service
#include "ble_nus.h"  //Nordic UART service
 
#include "internal_flash.h"
 
#define IS_SRVC_CHANGED_CHARACT_PRESENT 0                                           /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#ifdef BOARD_PCA10028
    #define DEVICE_NAME                     "NRF51DK"
#else
    #define DEVICE_NAME                     "BADGE"                           /**< Name of device. Will be included in the advertising data. */
#endif

#define APP_ADV_INTERVAL                320                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 200 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      180                                          

#define SEC_PARAM_BOND                  1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                           /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                        /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                           /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                           /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                          /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */


volatile bool isConnected;
volatile bool isAdvertising;



/**
 * Callback function for asserts in the SoftDevice; called in case of SoftDevice assert.
 * Parameters are file name and line number where assert occurred.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name);


/**
 * Function for reporting BLE errors.
 * Parameters are error code and line number where error occurs
 * Use macro below to report proper line number
 */
static void ble_error_handler(uint32_t error_code, uint32_t line_num);

/**
 * Macro for checking error codes.  Defined as a macro so we can report the line number.
 * Calls ble_error_handler if ERR_CODE is not NRF_SUCCESS (i.e. 0)
 */
#define BLE_ERROR_CHECK(ERR_CODE)                           \
    do                                                      \
    {                                                       \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);         \
        if (LOCAL_ERR_CODE != NRF_SUCCESS)                  \
        {                                                   \
            ble_error_handler(LOCAL_ERR_CODE,__LINE__);     \
        }                                                   \
    } while (0)

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void);

/**@brief Function for initializing battery and UART services
 */
static void services_init(void);

/**@brief Function for initializing security parameters.
 */
static void sec_params_init(void);

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt);


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt);


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called after a BLE stack event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt);


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt);


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void);


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void);


/**
 * Initialize BLE structures
 */
void BLEbegin();

/**
 * Stop softdevice
 */
void BLEdisable();

/**
 * Resume softdevice (after stopping it - skips some initialization processes that are in begin)
 */
void BLEresume();

/**
 * Disconnect from server forcefully.
 */
void BLEforceDisconnect();

/**
 * Functions called on connection or disconnection events
 */
void BLEonConnect();
void BLEonDisconnect();

/** 
 * Function for handling incoming data from the BLE UART service
 */
void BLEonReceive(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length);

/**
 * Function for handling BLE advertising reports (from scan)
 * Parameters are 6-byte array of BLE address, and RSSI signal strength
 */
void BLEonAdvReport(uint8_t addr[6], int8_t rssi);

/**
 * Function called on timeout of scan, to finalize and store scan results
 */
void BLEonScanTimeout();

/**
 * Function to return whether the client has yet enabled notifications
 */
bool notificationEnabled();

/** Function to wrap ble_nus_string_send, for convenience
 */
bool BLEwrite(uint8_t* data, uint16_t len);

/** Function to wrap ble_nus_string_send for sending one char, for convenience
 */
bool BLEwriteChar(uint8_t dataChar);




#endif //BLE_SETUP_H