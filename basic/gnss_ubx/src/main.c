#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gnss_parser, LOG_LEVEL_INF);

#define UART_NODE DT_NODELABEL(arduino_serial)
const struct device *uart = DEVICE_DT_GET(UART_NODE);

#define GNSS_FIX_TIMEOUT  60

K_SEM_DEFINE(gnss_fix_done, 0, 1);

/* UBX message definitions */
#define UBX_SYNC1 0xB5
#define UBX_SYNC2 0x62
#define UBX_NAV_PVT_CLASS 0x01
#define UBX_NAV_PVT_ID    0x07
#define UBX_NAV_PVT_PAYLOAD_LENGTH 92
#define UBX_NAV_PVT_MSG_LENGTH (6 + UBX_NAV_PVT_PAYLOAD_LENGTH + 2)

enum ubx_state {
    UBX_STATE_SYNC1,
    UBX_STATE_SYNC2,
    UBX_STATE_HEADER,
    UBX_STATE_PAYLOAD,
    UBX_STATE_CHECKSUM
};

static uint8_t ubx_buf[UBX_NAV_PVT_MSG_LENGTH];
static uint16_t ubx_buf_index = 0;
static uint16_t ubx_payload_length = 0;
static enum ubx_state ubx_state = UBX_STATE_SYNC1;

const char* get_fix_type_str(uint8_t fixType)
{
    switch (fixType) {
    case 0:  return "No Fix";
    case 1:  return "Dead Reckoning";
    case 2:  return "2D Fix";
    case 3:  return "3D Fix";
    case 4:  return "GNSS + DR Combined";
    case 5:  return "Time Only Fix";
    default: return "Unknown";
    }
}

/* Process a complete UBX message */
static void process_ubx_message(uint8_t *buf, uint16_t len)
{
    uint8_t ck_a = 0, ck_b = 0;
    for (int i = 2; i < len - 2; i++) {
        ck_a += buf[i];
        ck_b += ck_a;
    }
    if (ck_a != buf[len - 2] || ck_b != buf[len - 1]) {
        LOG_WRN("UBX checksum error");
        return;
    }
    
    if (buf[2] == UBX_NAV_PVT_CLASS && buf[3] == UBX_NAV_PVT_ID) {
        uint8_t *p = &buf[6];
        //uint32_t iTOW = *(uint32_t *)&p[0]; // Milliseconds into the week (not used here)
        uint16_t year = *(uint16_t *)&p[4];
        uint8_t month = p[6];
        uint8_t day = p[7];
        uint8_t hour = p[8];
        uint8_t min = p[9];
        uint8_t sec = p[10];
        uint8_t fixType = p[20];
        uint8_t numSV = p[23];
        int32_t lon = *(int32_t *)&p[24];
        int32_t lat = *(int32_t *)&p[28];
        int32_t height = *(int32_t *)&p[32];

        double longitude = lon * 1e-7;
        double latitude = lat * 1e-7;
        double altitude = height / 1000.0; // Convert from mm to meters

        LOG_INF("UBX NAV-PVT Fix Acquired!");
        LOG_INF("----------------------------------");
        LOG_INF(" Date & Time   : %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
        LOG_INF(" Fix Type      : %s", get_fix_type_str(fixType));
        LOG_INF(" Fix Type      : %d", fixType);
        LOG_INF(" Latitude      : %.7f°", latitude);
        LOG_INF(" Longitude     : %.7f°", longitude);
        LOG_INF(" Altitude      : %.2f m", altitude);
        LOG_INF(" Satellites    : %d", numSV);
        LOG_INF("----------------------------------");

        k_sem_give(&gnss_fix_done);  // Signal that a valid fix has been acquired
    }
}

/* UART interrupt callback: state-machine to parse incoming UBX binary stream */
static void uart_cb(const struct device *dev, void *user_data)
{
    uart_irq_update(dev);
    if (uart_irq_is_pending(dev)) {
        uint8_t byte;
        while (uart_irq_rx_ready(dev)) {
            uart_fifo_read(dev, &byte, 1);

            switch (ubx_state) {
            case UBX_STATE_SYNC1:
                if (byte == UBX_SYNC1) {
                    ubx_buf_index = 0;
                    ubx_buf[ubx_buf_index++] = byte;
                    ubx_state = UBX_STATE_SYNC2;
                }
                break;

            case UBX_STATE_SYNC2:
                if (byte == UBX_SYNC2) {
                    ubx_buf[ubx_buf_index++] = byte;
                    ubx_state = UBX_STATE_HEADER;
                } else {
                    ubx_state = UBX_STATE_SYNC1;
                }
                break;

            case UBX_STATE_HEADER:
                ubx_buf[ubx_buf_index++] = byte;
                if (ubx_buf_index == 6) {
                    /* Bytes 4 and 5 (index 4 & 5) represent payload length (little-endian) */
                    ubx_payload_length = ubx_buf[4] | (ubx_buf[5] << 8);
                    ubx_state = UBX_STATE_PAYLOAD;
                }
                break;

            case UBX_STATE_PAYLOAD:
                ubx_buf[ubx_buf_index++] = byte;
                if (ubx_buf_index == 6 + ubx_payload_length) {
                    ubx_state = UBX_STATE_CHECKSUM;
                }
                break;

            case UBX_STATE_CHECKSUM:
                ubx_buf[ubx_buf_index++] = byte;
                if (ubx_buf_index == 6 + ubx_payload_length + 2) {
                    process_ubx_message(ubx_buf, ubx_buf_index);
                    ubx_state = UBX_STATE_SYNC1;
                    ubx_buf_index = 0;
                }
                break;

            default:
                ubx_state = UBX_STATE_SYNC1;
                break;
            }
        }
    }
}

/* Send a UBX command via UART */
static int send_ubx_command(const uint8_t *cmd, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart, cmd[i]);
    }
    return 0;
}

/* Configure the GNSS module: disable NMEA messages and enable UBX-NAV-PVT output */
static int configure_gnss(void)
{
    const uint8_t ubx_cfg_valset[] = {
        0xB5, 0x62,        // UBX Sync Header
        0x06, 0x8A,        // Class: CFG (0x06), ID: VALSET (0x8A)
        0x22, 0x00,        // Payload Length (34 bytes)
        0x00,              // Version (0x00)
        0x01,              // Layers (0x01 = RAM)
        0x00, 0x00,        // Reserved

        // Disable NMEA output (CFG_MSGOUT_NMEA Standard Messages)
        0xC5, 0x00, 0x91, 0x20, 0x00,  // Disable GGA on UART1
        0xCA, 0x00, 0x91, 0x20, 0x00,  // Disable RMC on UART1
        0xC0, 0x00, 0x91, 0x20, 0x00,  // Disable GLL on UART1
        0xB1, 0x00, 0x91, 0x20, 0x00,  // Disable VTG on UART1
        0xAC, 0x00, 0x91, 0x20, 0x00,  // Disable GSA on UART1
        0xBB, 0x00, 0x91, 0x20, 0x00,  // Disable GSV on UART1

        // Checksum (CK_A, CK_B)
        0x40, 0xDA
    };

    const uint8_t ubx_nav_pvt_req[] = {
        0xB5, 0x62, 0x06, 0x01, 0x03, 0x00,  // UBX-CFG-MSG header
        0x01, 0x07, 0x01,                    // Enable NAV-PVT (Message Class = 0x01, ID = 0x07, Rate = 1)
        0x13, 0x51                           // Checksum
    };

    send_ubx_command(ubx_cfg_valset, sizeof(ubx_cfg_valset));
    k_sleep(K_MSEC(100));  // Delay to allow the module to process the command
    send_ubx_command(ubx_nav_pvt_req, sizeof(ubx_nav_pvt_req));
    k_sleep(K_MSEC(100));
    return 0;
}

/* Initialize the UART interface */
static int uart_init(void)
{
    if (!device_is_ready(uart)) {
        LOG_ERR("UART device not ready");
        return -1;
    }
    uart_irq_callback_set(uart, uart_cb);
    return 0;
}

/* Acquire a GNSS fix by waiting for a valid UBX-NAV-PVT message */
int acquire_gnss_fix(void)
{
    LOG_DBG("Enabling UART Peripheral...");
    k_sem_reset(&gnss_fix_done);
    pm_device_action_run(uart, PM_DEVICE_ACTION_RESUME);
    uart_irq_rx_enable(uart);

    int ret = k_sem_take(&gnss_fix_done, K_SECONDS(GNSS_FIX_TIMEOUT));

    LOG_DBG("Disabling UART Peripheral...");
    uart_irq_rx_disable(uart);
    pm_device_action_run(uart, PM_DEVICE_ACTION_SUSPEND);

    if (ret < 0) {
        LOG_ERR("GNSS Fix Timed Out!");
        return -1;
    }
    return 0;
}

int main(void)
{
    LOG_INF("GNSS UBX Mode Initialized. Waiting for Fix Requests...");

    int err = uart_init();
    if (err < 0) {
        LOG_ERR("UART Initialization Failed!");
        return -1;
    }
    /* Send configuration commands to switch off NMEA and enable UBX-NAV-PVT */
    configure_gnss();

    /* Attempt to acquire the first GNSS fix */
    acquire_gnss_fix();

    while (1) {
        k_sleep(K_SECONDS(5));  // Adjust sleep period as needed

        if (acquire_gnss_fix() < 0) {
            LOG_WRN("GNSS Fix Failed! Trying again in the next cycle.");
        }
    }
    return 0;
}
