// Self Include
#include "js_audio.h"

// Library Includes
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Defines
#define TAG "js_audio"

// Types
typedef struct
{
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    long data_offset;
    uint32_t data_size;
} wav_info_t; // WAV file information from the header

typedef struct
{
    int predictor;
    int step_index;
} ima_state_t;

// Forward Declarations
static bool _is_playing = false;
static bool _stop_requested = false;
static i2s_chan_handle_t tx_chan;
static int16_t ima_decode_nibble(uint8_t nibble, ima_state_t *st);
static bool parse_wav_header(FILE *f, wav_info_t *info);
static void audio_play_task(void *arg);
static void stop_audio(void); // Stop the audio playback with silence

/** Initialize JS Audio
 * Init the I2S interface for audio output
 */
esp_err_t js_audio_init(void) {
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "js_audio_init...");

    /* I2S config */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_GOTO_ON_ERROR(i2s_new_channel(&chan_cfg, &tx_chan, NULL), error, TAG, "Failed to create I2S channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_5,
            .ws = GPIO_NUM_6,
            .dout = GPIO_NUM_4,
            .din = I2S_GPIO_UNUSED,
        },
    };

    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(tx_chan, &std_cfg), error, TAG, "Failed to initialize I2S channel");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(tx_chan), error, TAG, "Failed to enable I2S channel");

    // Return OK
    return ESP_OK;

error:
    return ret;
}

/* ************************** Global Functions ************************** */
/** Play the audio file with passed in path */
void js_audio_play_pause(const char *path) {
    if (_is_playing) {
        ESP_LOGI(TAG, "Stopping audio playback");
        _is_playing = false;
        _stop_requested = true;
    } else {
        ESP_LOGI(TAG, "Starting audio play task for file: %s", path);
        _is_playing = true;
        _stop_requested = false;
        char *path_copy = strdup(path); // Send a copy incase this changes
        xTaskCreate(audio_play_task, "audio_play_task", 4096, path_copy, 10, NULL);
    }
}

static void audio_play_task(void *arg) {
    char *path = (char *)arg;
    ESP_LOGI(TAG, "Playing audio file: %s", path);

    // Open the file
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE("AUDIO", "Failed to open file: %s", path);
        goto cleanup;
    }

    // Read the WAV header and fill in the wav_info_t structure
    wav_info_t wi;
    if (!parse_wav_header(f, &wi)) {
        ESP_LOGE(TAG, "Bad WAV header");
        goto cleanup;
    }
    ESP_LOGI(TAG, "fmt=%u ch=%u sr=%lu bps=%u align=%u data=%lu+%lu",
             wi.audio_format, wi.channels, (unsigned long)wi.sample_rate,
             wi.bits_per_sample, wi.block_align,
             wi.data_offset, (unsigned long)wi.data_size);

    // Validate the header
    if (wi.audio_format != 0x0011 || wi.channels != 1 || wi.sample_rate != 16000 || wi.bits_per_sample != 4) {
        ESP_LOGE(TAG, "Unsupported WAV (need IMA ADPCM mono 16kHz 4-bit)");
        goto cleanup;
    }

    // seek to data
    fseek(f, wi.data_offset, SEEK_SET);

    // (optional) set I2S rate from header
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(wi.sample_rate);
    i2s_channel_disable(tx_chan);
    i2s_channel_reconfig_std_clock(tx_chan, &clk);
    i2s_channel_enable(tx_chan);

    const int block_bytes = wi.block_align;
    const int samples_per_block = 1 + (block_bytes - 4) * 2;

    uint8_t *blk = malloc(block_bytes);
    int16_t *pcm = malloc(samples_per_block * sizeof(int16_t));

    while (1) {
        if (_stop_requested) break;

        size_t got = fread(blk, 1, block_bytes, f);
        if (got != (size_t)block_bytes) break;

        // block header
        int16_t predictor = (int16_t)(blk[0] | (blk[1] << 8));
        uint8_t step_index = blk[2];

        ima_state_t st = {
            .predictor = predictor,
            .step_index = step_index > 88 ? 88 : step_index,
        };

        int out_samples = 0;
        pcm[out_samples++] = (int16_t)st.predictor; // first sample is the predictor

        // decode packed nibbles
        for (int i = 4; i < block_bytes; i++) {
            uint8_t b = blk[i];
            pcm[out_samples++] = ima_decode_nibble(b & 0x0F, &st);
            pcm[out_samples++] = ima_decode_nibble(b >> 4, &st);
        }

        size_t written = 0;
        i2s_channel_write(tx_chan, pcm, out_samples * sizeof(int16_t), &written, portMAX_DELAY);
    }

    free(pcm);
    free(blk);

    ESP_LOGI(TAG, "Finished playing audio file: %s", path);

cleanup:
    stop_audio(); // Make sure the audio is stopped properly
    _stop_requested = false;
    _is_playing = false;
    if (path) free(path);
    if (f) fclose(f);
    vTaskDelete(NULL); // Delete self when done
}

/* ********************* Local I2S/Audio Codex Functions ********************* */
static const int step_table[] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337,
    371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385,
    24623, 27086, 29794, 32767};

static const int index_table[] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8};

static int16_t ima_decode_nibble(uint8_t nibble, ima_state_t *st) {
    int step = step_table[st->step_index];
    int diff = step >> 3;

    if (nibble & 1)
        diff += step >> 2;
    if (nibble & 2)
        diff += step >> 1;
    if (nibble & 4)
        diff += step;
    if (nibble & 8)
        diff = -diff;

    st->predictor += diff;
    if (st->predictor > 32767)
        st->predictor = 32767;
    if (st->predictor < -32768)
        st->predictor = -32768;

    st->step_index += index_table[nibble & 0x0F];
    if (st->step_index < 0)
        st->step_index = 0;
    if (st->step_index > 88)
        st->step_index = 88;

    return (int16_t)st->predictor;
}

// Stop Playing Audio cleanly
static void stop_audio(void) {
    // Drop any queued samples immediately
    i2s_channel_disable(tx_chan);
    i2s_channel_enable(tx_chan);

    // Then feed a little silence while clocks are running
    int16_t zeros[256] = {0};
    size_t w;
    for (int i = 0; i < 8; i++) { // ~128ms at 16kHz (close enough)
        i2s_channel_write(tx_chan, zeros, sizeof(zeros), &w, portMAX_DELAY);
    }
}

// Helper function to convert little-endian 16-bit value to host byte order
static uint16_t read_le16(FILE *f) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    return (uint16_t)(b[0] | (b[1] << 8));
}

// Helper function to convert little-endian 32-bit value to host byte order
static uint32_t read_le32(FILE *f) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

// Parse the WAV file header and fill in the wav_info_t structure
static bool parse_wav_header(FILE *f, wav_info_t *info) {
    memset(info, 0, sizeof(*info));

    uint8_t id[4];
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4) != 0) return false;
    (void)read_le32(f); // riff size
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4) != 0) return false;

    bool have_fmt = false;

    while (fread(id, 1, 4, f) == 4) {
        uint32_t chunk_size = read_le32(f);

        if (memcmp(id, "fmt ", 4) == 0) {
            info->audio_format = read_le16(f);
            info->channels = read_le16(f);
            info->sample_rate = read_le32(f);
            (void)read_le32(f); // byte_rate
            info->block_align = read_le16(f);
            info->bits_per_sample = read_le16(f);

            // skip any extra fmt bytes
            uint32_t consumed = 16;
            if (chunk_size > consumed) fseek(f, chunk_size - consumed, SEEK_CUR);
            have_fmt = true;
        } else if (memcmp(id, "data", 4) == 0) {
            info->data_offset = ftell(f);
            info->data_size = chunk_size;
            return have_fmt; // done
        } else {
            long skip = (long)chunk_size + (chunk_size & 1);
            if (fseek(f, skip, SEEK_CUR) != 0) return false;
        }
    }

    return false;
}
