// Self Include
#include "js_audio.h"

// Library Includes
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Defines
#define TAG "js_audio"

// Types
typedef struct
{
	int predictor;
	int step_index;
} ima_state_t;

// Forward Declarations
static i2s_chan_handle_t tx_chan;
static int16_t ima_decode_nibble(uint8_t nibble, ima_state_t *st);
static void skip_wav_header(FILE *f);
static void audio_play_task(void *arg);

/** Initialize JS Audio
 * Init the I2S interface for audio output
 */
esp_err_t js_audio_init(void)
{
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
void js_audio_play(const char *path)
{
	ESP_LOGI(TAG, "Starting audio play task for file: %s", path);
	xTaskCreate(audio_play_task, "audio_play_task", 4096, (void *)path, 10, NULL);
}

static void audio_play_task(void *arg)
{
	const char *path = (const char *)arg;
	ESP_LOGI(TAG, "Playing audio file: %s", path);

	FILE *f = fopen(path, "rb");
	if (!f)
	{
		ESP_LOGE("AUDIO", "Failed to open file: %s", path);
		vTaskDelete(NULL);
		return;
	}
	// Check file size before and after header skip
	fseek(f, 0, SEEK_END);
	long file_size = ftell(f);
	fseek(f, 0, SEEK_SET);
	ESP_LOGI(TAG, "File size: %ld bytes", file_size);

	skip_wav_header(f);

	long after_header = ftell(f);
	ESP_LOGI(TAG, "After header skip: position %ld", after_header);

	ima_state_t st = {0};
	uint8_t in[256];
	int16_t out[512];
	size_t bytes;

	while ((bytes = fread(in, 1, sizeof(in), f)) > 0)
	{
		int out_samples = 0;

		for (size_t i = 0; i < bytes; i++)
		{
			out[out_samples++] = ima_decode_nibble(in[i] & 0x0F, &st);
			out[out_samples++] = ima_decode_nibble(in[i] >> 4, &st);
		}

		size_t written;
		i2s_channel_write(
			tx_chan,
			out,
			out_samples * sizeof(int16_t),
			&written,
			portMAX_DELAY);
	}

	ESP_LOGI(TAG, "Finished playing audio file: %s", path);
	fclose(f);
	vTaskDelete(NULL); // Delete self when done
}

/* ********************* Local I2S/Audio Codex Functions ********************* */
static const int step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
	34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337,
	371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
	2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
	12635, 13899, 15289, 16818, 18500, 20350, 22385,
	24623, 27086, 29794, 32767};

static const int index_table[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8};

static int16_t ima_decode_nibble(uint8_t nibble, ima_state_t *st)
{
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

// Skip to the end of the wave header. This is NORMALLY 44 bytes but may be different
#include <string.h> // add this include at top for memcmp

static uint32_t read_le32(FILE *f)
{
	uint8_t b[4];
	if (fread(b, 1, 4, f) != 4)
		return 0;
	return ((uint32_t)b[0]) |
		   ((uint32_t)b[1] << 8) |
		   ((uint32_t)b[2] << 16) |
		   ((uint32_t)b[3] << 24);
}

static void skip_wav_header(FILE *f)
{
	uint8_t id[4];

	// RIFF header: "RIFF" <u32 size> "WAVE"
	if (fread(id, 1, 4, f) != 4)
		return;
	uint32_t riff_size = read_le32(f);
	(void)riff_size;

	if (memcmp(id, "RIFF", 4) != 0)
	{
		// Not a RIFF WAV; rewind and treat as raw
		fseek(f, 0, SEEK_SET);
		return;
	}

	if (fread(id, 1, 4, f) != 4)
		return;
	if (memcmp(id, "WAVE", 4) != 0)
	{
		fseek(f, 0, SEEK_SET);
		return;
	}

	// Subchunks: <4cc> <u32 size> <data...> (padded to even)
	while (fread(id, 1, 4, f) == 4)
	{
		uint32_t chunk_size = read_le32(f);

		ESP_LOGI(TAG, "Chunk: %.4s, size: %lu", (char *)id, (unsigned long)chunk_size);

		if (memcmp(id, "data", 4) == 0)
		{
			// File pointer is now at start of audio data
			return;
		}

		// Skip chunk payload (+ pad byte if odd size)
		long skip = (long)chunk_size + (chunk_size & 1);
		if (fseek(f, skip, SEEK_CUR) != 0)
		{
			return;
		}
	}
}