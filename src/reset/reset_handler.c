/*
 * 
 * Marcelo Aleksandravicius 
 *
 * 
 */


#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/types.h>
#include <hal/nrf_power.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(reset_handler);

// Variables for double reset detection
#define RESET_MAGIC 0x12435687
#define DOUBLE_RESET_WINDOW_MS CONFIG_DOUBLE_RESET_WINDOW_MS

__attribute__((section(".retained_ram"))) static uint32_t reset_magic;

extern int __real_main();

// LED definition
#define LED_NODE DT_ALIAS(led0)
#if DT_NODE_HAS_STATUS(LED_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
#else
#error "Unsupported board: led0 alias is not defined"
#endif

static struct k_work advertise_work;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                  0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
                  0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(struct k_work *work) {
  int rc;

  rc = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (rc) {
    LOG_ERR("Advertising failed to start (rc %d)", rc);
    return;
  }

  printf("Advertising successfully started\n");
}

static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    LOG_ERR("Connection failed (err 0x%02x)", err);
  } else {
    printf("Connected\n");
  }

  k_work_submit(&advertise_work);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  printf("Disconnected (reason 0x%02x)\n", reason);
}

static void on_conn_recycled(void) {
  printf("on_conn_recycled\n");
  k_work_submit(&advertise_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = on_conn_recycled,
};

static void bt_ready(int err) {
  if (err != 0) {
    LOG_ERR("Bluetooth failed to initialise: %d", err);
  } else {
    k_work_submit(&advertise_work);
  }
}

void start_smp_bluetooth_adverts(void) {
  int rc;

  k_work_init(&advertise_work, advertise);
  rc = bt_enable(bt_ready);

  if (rc != 0) {
    LOG_ERR("Bluetooth enable failed: %d", rc);
  }
}

static void enable_ble_smp_update_mode(void) {
  printf("Entering update mode with LED blinking...\n");

  // Configure LED if available
  if (device_is_ready(led.port)) {
    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
      printf("Error configuring LED GPIO\n");
    } else {
      printf("LED configured to blink at 5Hz\n");
    }
  } else {
    printf("LED not available - continuing without LED\n");
  }

  // Start Bluetooth to allow updates via MCUmgr
  start_smp_bluetooth_adverts();
  printf("Bluetooth started - update mode active\n");

  // Infinite loop blinking LED at 5Hz (100ms on, 100ms off)
  printf("Waiting for update via Bluetooth... LED blinking\n");
  while (1) {
    if (device_is_ready(led.port)) {
      gpio_pin_toggle_dt(&led);
    }
    k_msleep(CONFIG_DFU_LED_BLINK_PERIOD_MS);  

    static int counter = 0;
    if (++counter >= 50) {  // Every 5 seconds (50 * 100ms)
      counter = 0;
    }
  }
}


bool is_a_double_reset(void) {
  // Get reset reason using nRF HAL function
  uint32_t reset_reason = nrf_power_resetreas_get(NRF_POWER);
  
  // Clear it
  nrf_power_resetreas_clear(NRF_POWER, reset_reason);
  
  // Check if reset was from pin (RESETPIN mask)
  bool is_pin_reset = (reset_reason & NRF_POWER_RESETREAS_RESETPIN_MASK) != 0;
  
  if (!is_pin_reset) {
    printf("RESET NOT FROM PHYSICAL PIN - IGNORING!!!\n");
    reset_magic = 0;
    return false;
  }

  if (reset_magic == RESET_MAGIC) {
    // Second reset detected!
    printf("==============================================\n");
    printf("DOUBLE RESET DETECTED! Entering update mode...\n");
    reset_magic = 0;  // Clear to avoid loops
    return true;

  } else {
    // First reset - set magic and wait for time window
    reset_magic = RESET_MAGIC;
    printf("reset_magic: 0x%08x - reset detected\n", reset_magic);    printf("RESET DETECTED! Waiting for second reset...\n");


    printf("\n┌───────────────────────────────────────────────────────────────┐");
    printf("\n│                Waiting %d ms to detect second reset         │", CONFIG_MAIN_WRAPPER_DELAY_MS);
    printf("\n└───────────────────────────────────────────────────────────────┘\n");
    k_msleep(CONFIG_MAIN_WRAPPER_DELAY_MS);
    k_msleep(DOUBLE_RESET_WINDOW_MS);

    // If we got here, no double reset occurred - clear magic
    reset_magic = 0;
    printf("==============================================\n");
    printf("DOUBLE RESET TIME EXPIRED\n");
    printf("==============================================\n");
  }
  return false;
}


// Main wrapper function
int __wrap_main(void) {
  bool double_reset_detected = false;

  printf("\n╔═══════════════════════════════════════════════════════════════╗");
  printf("\n║                      SYSTEM INITIALIZATION                    ║");
  printf("\n╚═══════════════════════════════════════════════════════════════╝\n");


  double_reset_detected = is_a_double_reset();

  // Only call real main if not in double reset mode
  if (!double_reset_detected) {
    printf("\n╔═══════════════════════════════════════════════════════════════╗");
    printf("\n║                     APPLICATION STARTED                       ║");
    printf("\n║                      Calling Real main                        ║");
    printf("\n╚═══════════════════════════════════════════════════════════════╝\n");

    __real_main();
  } else {
    printf("\n╔═══════════════════════════════════════════════════════════════╗");
    printf("\n║                     UPDATE MODE ACTIVE                        ║");
    printf("\n║                  Real Main Function Skipped                   ║");
    printf("\n╚═══════════════════════════════════════════════════════════════╝\n");

    // Start update mode with LED blinking and Bluetooth
    enable_ble_smp_update_mode();
  }

  return 0;
}
