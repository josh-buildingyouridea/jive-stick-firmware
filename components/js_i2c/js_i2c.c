// Self Include
#include "js_i2c.h"

// Library Includes

#include "esp_check.h"
#include "esp_log.h"

// Local Includes
// #include "js_leds.h"

// Defines
#define TAG "js_i2c"
#define I2C_SDA_GPIO GPIO_NUM_20
#define I2C_SCL_GPIO GPIO_NUM_21
#define I2C_PORT 0

// Forward Declarations
i2c_master_bus_handle_t i2c_bus_handle = NULL;

/** Initialize JS I2C */
esp_err_t js_i2c_init(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "js_i2c_init...");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_GOTO_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_bus_handle), error, TAG, "Failed to initialize I2C bus");

    return ESP_OK;

error:
    return ret;
}
/* ************************** Global Functions ************************** */

/* ************************** Local Functions ************************** */
