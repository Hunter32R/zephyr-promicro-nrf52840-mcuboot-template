#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>


// LED configuration
#define LED_NODE DT_ALIAS(led0)
#if DT_NODE_HAS_STATUS(LED_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
#else
#error "Unsupported board: led0 alias is not defined"
#endif

int main(void) {
  // Test crash functionality
//   k_panic();


  // LED setup and blink loop
  int ret;
  bool led_state = false;

  // Check if LED device is ready
  if (!device_is_ready(led.port)) {
    printk("Error: LED device %s is not ready\n", led.port->name);
    return -ENODEV;
  }

  // Configure LED GPIO
  ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  if (ret < 0) {
    printk("Error: Failed to configure LED GPIO\n");
    return ret;
  }

  printk("(main) LED configured on pin %d. Starting blink...\n", led.pin);

  // Main LED blink loop
  while (1) {
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0) {
      printk("Error: Failed to toggle LED\n");
      return ret;
    }
    
    led_state = !led_state;
    // printk("LED %s\n", led_state ? "ON" : "OFF");
    
    k_msleep(1000);
  }
  
  return 0;
}