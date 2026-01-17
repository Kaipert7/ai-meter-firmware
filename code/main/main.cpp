#include "sdkconfig.h"

#include <iostream>
#include <string>
#include <vector>
#include <regex>

#include "esp_psram.h"
#include "esp_pm.h"
#include "psram.h"

#include "esp_chip_info.h"

#include "esp_vfs_fat.h"
#include "ffconf.h"
#include "driver/sdmmc_host.h"

#include "defines.h"
#include "Helper.h"

#include "read_network_config.h"
#include "network_init.h"
#include "server_ota.h"

#include "MainFlowControl.h"
#include "ClassLogFile.h"

#include "time_sntp.h"

#include "statusled.h"
#include "sdcard_check.h"

#ifdef DISABLE_BROWNOUT_DETECTOR
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

extern const char *GIT_TAG;
extern const char *GIT_REV;
extern const char *GIT_BRANCH;
extern const char *BUILD_TIME;

extern std::string getFwVersion(void);
extern std::string getHTMLversion(void);
extern std::string getHTMLcommit(void);

bool setCpuFrequency(void);

static const char *TAG = "MAIN";

bool init_nvs_sd_card()
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_LOGD(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
    // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply first.
#if SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return false;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    sdmmc_slot_config_t slot_config = {
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
    };
#else
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#endif

    // Set bus width to use:
#ifdef __SD_USE_ONE_LINE_MODE__
    slot_config.width = 1;
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = GPIO_SDCARD_CLK;
    slot_config.cmd = GPIO_SDCARD_CMD;
    slot_config.d0 = GPIO_SDCARD_D0;
#endif // end CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
#else  // else __SD_USE_ONE_LINE_MODE__
    slot_config.width = 4;
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.d1 = GPIO_SDCARD_D1;
    slot_config.d2 = GPIO_SDCARD_D2;
    slot_config.d3 = GPIO_SDCARD_D3;
#endif // end CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
#endif // end __SD_USE_ONE_LINE_MODE__

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // Der PullUp des GPIO13 wird durch slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    // nicht gesetzt, da er eigentlich nicht benötigt wird,
    // dies führt jedoch bei schlechten Kopien des AI_THINKER Boards
    // zu Problemen mit der SD Initialisierung und eventuell sogar zur reboot-loops.
    // Um diese Probleme zu kompensieren, wird der PullUp manuel gesetzt.
    gpio_set_pull_mode(GPIO_SDCARD_D3, GPIO_PULLUP_ONLY); // HS2_D3

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 12,           // previously -> 2022-09-21: 5, 2023-01-02: 7
        .allocation_unit_size = 0, // 0 = auto
        .disk_status_check_enable = 0,
    };

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing production applications.
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount FAT filesystem on SD card. Check SD card filesystem (only FAT supported) or try another card");
            set_status_led(SDCARD_INIT, 1, true);
        }
        else if (ret == 263)
        {
            // Error code: 0x107 --> usually: SD not found
            ESP_LOGE(TAG, "SD card init failed. Check if SD card is properly inserted into SD card slot or try another card");
            set_status_led(SDCARD_INIT, 2, true);
        }
        else
        {
            ESP_LOGE(TAG, "SD card init failed. Check error code or try another card");
            set_status_led(SDCARD_INIT, 3, true);
        }
        return false;
    }

    // sdmmc_card_print_info(stdout, card);  // With activated CONFIG_NEWLIB_NANO_FORMAT --> capacity not printed correctly anymore
    save_sd_card_info(card);
    return true;
}

esp_err_t init_psram(void)
{
    // Init external PSRAM
    // ********************************************
    esp_err_t result = esp_psram_init();
    if (result == ESP_FAIL)
    {
        // ESP_FAIL -> Failed to init PSRAM
        ESP_LOGE(TAG, "PSRAM init failed (%s)! PSRAM not found or defective", std::to_string(result).c_str());
        set_system_statusflag(SYSTEM_STATUS_PSRAM_BAD);
        set_status_led(PSRAM_INIT, 1, true);
        return ESP_FAIL;
    }
    else
    {
        // ESP_OK -> PSRAM init OK --> continue to check PSRAM size
        size_t psram_size = esp_psram_get_size();
        ESP_LOGI(TAG, "PSRAM size: %s byte (%s MB / %s MBit)", std::to_string(psram_size).c_str(), std::to_string(psram_size / 1024 / 1024).c_str(), std::to_string(psram_size / 1024 / 1024 * 8).c_str());

        // Check PSRAM size
        // ********************************************
        if (psram_size < (4 * 1024 * 1024))
        {
            // PSRAM is below 4 MBytes (32Mbit)
            ESP_LOGE(TAG, "PSRAM size >= 4MB (32Mbit) is mandatory to run this application");
            set_system_statusflag(SYSTEM_STATUS_PSRAM_BAD);
            set_status_led(PSRAM_INIT, 2, true);
            return ESP_FAIL;
        }
        else
        {
            // PSRAM size OK --> continue to check heap size
            size_t _hsize = get_heapsize();
            ESP_LOGI(TAG, "Total heap: %s byte", std::to_string(_hsize).c_str());

            // Check heap memory
            // ********************************************
            if (_hsize < 4000000)
            {
                // Check available Heap memory for a bit less than 4 MB (a test on a good device showed 4187558 bytes to be available)
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Total heap >= 4000000 byte is mandatory to run this application");
                set_system_statusflag(SYSTEM_STATUS_HEAP_TOO_SMALL);
                set_status_led(PSRAM_INIT, 3, true);
                return ESP_FAIL;
            }
            else
            {
                // HEAP size OK --> continue to reserve shared memory block and check camera init
                // Allocate static PSRAM memory regions
                if (reserve_psram_shared_region() == false)
                {
                    ESP_LOGI(TAG, "Allocate static PSRAM memory regions failed, heap too small!");
                    set_system_statusflag(SYSTEM_STATUS_HEAP_TOO_SMALL);
                    set_status_led(PSRAM_INIT, 3, true);
                    return ESP_FAIL;
                }
                else
                {
                    ESP_LOGI(TAG, "Allocate static PSRAM memory regions ok");
                    return ESP_OK;
                }
            }
        }
    }

    return ESP_OK;
}

esp_err_t init_camera(void)
{
    Camera.set_flash_light_on_off(false);
    Camera.power_reset_camera();
    esp_err_t result = Camera.init_camera();

    // Check camera init
    // ********************************************
    if (result != ESP_OK)
    {
        // Camera init failed, retry to init
        char camStatusHex[33];
        sprintf(camStatusHex, "0x%02x", result);
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Camera init failed (" + std::string(camStatusHex) + "), retrying...");

        Camera.power_reset_camera();
        result = Camera.init_camera();
        Camera.set_flash_light_on_off(false);

        if (result != ESP_OK)
        {
            // Camera init failed again
            sprintf(camStatusHex, "0x%02x", result);
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Camera init failed (" + std::string(camStatusHex) + ")! Check camera module and/or proper electrical connection");
            set_system_statusflag(SYSTEM_STATUS_CAM_BAD);
            Camera.set_flash_light_on_off(false); // make sure flashlight is off
            set_status_led(CAM_INIT, 1, true);
            return ESP_FAIL;
        }
    }

    Camera.set_flash_light_on_off(false); // make sure flashlight is off before start of flow

    // Print camera infos
    // ********************************************
    char caminfo[50];
    sensor_t *sensor = esp_camera_sensor_get();
    sprintf(caminfo, "PID: 0x%02x, VER: 0x%02x, MIDL: 0x%02x, MIDH: 0x%02x", sensor->id.PID, sensor->id.VER, sensor->id.MIDH, sensor->id.MIDL);
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Camera info: " + std::string(caminfo));

    return ESP_OK;
}

extern "C" void app_main(void)
{
#ifdef DISABLE_BROWNOUT_DETECTOR
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
#endif

#if (defined(BOARD_ESP32_S3_ETH_V1) || defined(BOARD_ESP32_S3_ETH_V2))
    // Configure IO Pad as General Purpose IO,
    // so that it can be connected to internal Matrix,
    // then combined with one or more peripheral signals.
    gpio_pad_select_gpio(PER_ENABLE);

    gpio_set_direction(PER_ENABLE, GPIO_MODE_OUTPUT);
    gpio_set_level(PER_ENABLE, 1);
#endif

    // ********************************************
    // Highlight start of app_main
    // ********************************************
    ESP_LOGI(TAG, "\n\n\n\n================ Start app_main =================");

    // Init SD card
    // ********************************************
    if (!init_nvs_sd_card())
    {
        ESP_LOGE(TAG, "!!! Device init aborted at step: init_nvs_sd_card() !!!");
        set_system_statusflag(SYSTEM_STATUS_SDCARD_CHECK_BAD);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        return; // No way to continue without working SD card!
    }

    // Init external PSRAM
    // ********************************************
    if (init_psram() != ESP_OK)
    {
        ESP_LOGE(TAG, "!!! Device init aborted at step: init_psram() !!!");
        set_system_statusflag(SYSTEM_STATUS_PSRAM_BAD);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        return; // No way to continue without working PSRAM!
    }

    // SD card: basic R/W check
    // ********************************************
    int iSDCardStatus = SDCardCheckRW();
    if (iSDCardStatus < 0)
    {
        if (iSDCardStatus <= -1 && iSDCardStatus >= -2)
        {
            // write error
            set_status_led(SDCARD_CHECK, 1, true);
        }
        else if (iSDCardStatus <= -3 && iSDCardStatus >= -5)
        {
            // read error
            set_status_led(SDCARD_CHECK, 2, true);
        }
        else if (iSDCardStatus == -6)
        {
            // delete error
            set_status_led(SDCARD_CHECK, 3, true);
        }

        ESP_LOGE(TAG, "!!! Device init aborted at step: SDCardCheckRW() !!!");
        set_system_statusflag(SYSTEM_STATUS_SDCARD_CHECK_BAD);
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        return; // No way to continue without working SD card!
    }

    // SD card: Create log directories (if not already existing)
    // ********************************************
    LogFile.CreateLogDirectories(); // mandatory for logging + image saving

    // SD card: Create further mandatory directories (if not already existing)
    // Correct creation of these folders will be checked with function "SDCardCheckFolderFilePresence"
    // ********************************************
    make_dir("/sdcard/firmware");     // mandatory for OTA firmware update
    make_dir("/sdcard/img_tmp");      // mandatory for setting up alignment marks
    make_dir("/sdcard/demo");         // mandatory for demo mode
    make_dir("/sdcard/config/certs"); // mandatory for mqtt certificates

    // SD card: Check presence of some mandatory folders / files
    // ********************************************
    if (!SDCardCheckFolderFilePresence())
    {
        set_status_led(SDCARD_CHECK, 4, true);
        set_system_statusflag(SYSTEM_STATUS_FOLDER_CHECK_BAD); // reduced web interface going to be loaded
    }

    // ********************************************
    // Highlight start of logfile logging
    // Default Log Level: INFO -> Everything which needs to be logged during boot should be have level INFO, WARN OR ERROR
    // ********************************************
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "=================================================");
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "==================== Start ======================");
    LogFile.WriteToFile(ESP_LOG_INFO, TAG, "=================================================");

    // Check for updates
    // ********************************************
    ESP_LOGI(TAG, "check for updates");
    CheckOTAUpdateStatus();
    if (!CheckOTAUpdateAvailability())
    {
        // Check reboot reason
        // ********************************************
        ESP_LOGI(TAG, "check reboot reason");
        CheckIsPlannedReboot();
        if (!getIsPlannedReboot() && (esp_reset_reason() == ESP_RST_PANIC))
        {
            // If system reboot was not triggered by user and reboot was caused by execption
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Reset reason: " + get_reset_reason());
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Device was rebooted due to a software exception! Log level is set to DEBUG until the next reboot. "
                                                   "Flow init is delayed by 5 minutes to check the logs or do an OTA update");
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Keep device running until crash occurs again and check logs after device is up again");
            LogFile.setLogLevel(ESP_LOG_DEBUG);
        }
        else
        {
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Reset reason: " + get_reset_reason());
        }

        // Set CPU Frequency
        // ********************************************
        ESP_LOGI(TAG, "set CPU Frequency");
        setCpuFrequency();

#ifdef CONFIG_SOC_TEMP_SENSOR_SUPPORTED
        ESP_LOGI(TAG, "init Tempsensor");
        init_tempsensor();
#endif

        // Init camera
        // ********************************************
        ESP_LOGI(TAG, "init camera");
        if (init_camera() != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "!!! Error at step: init_camera() !!!");
        }

        // Check version information
        // ********************************************
        ESP_LOGI(TAG, "check version information");
        std::string versionFormated = getFwVersion() + ", Date/Time: " + std::string(BUILD_TIME) + ", Web UI: " + getHTMLversion();

        if (std::string(GIT_TAG) != "")
        {
            // We are on a tag, add it as prefix
            versionFormated = "Tag: '" + std::string(GIT_TAG) + "', " + versionFormated;
        }
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, versionFormated);

        if (getHTMLcommit().substr(0, 7) == "?")
        {
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, std::string("Failed to read file html/version.txt to parse Web UI version"));
        }

        if (getHTMLcommit().substr(0, 7) != std::string(GIT_REV).substr(0, 7))
        {
            // Compare the first 7 characters of both hashes
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Web UI version (" + getHTMLcommit() + ") does not match firmware version (" + std::string(GIT_REV) + ")");
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Recommendation: Repeat installation using AI-on-the-edge-device__update__*.zip");
        }

        // Start webserver + register handler
        // ********************************************
        ESP_LOGI(TAG, "starting Webserver");
        if (init_network() != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Webserver init failed. Device init aborted!");
            set_status_led(WLAN_INIT, 3, true);
            return;
        }

        // Set log level for wifi component to WARN level (default: INFO; only relevant for serial console)
        // ********************************************
        esp_log_level_set("wifi", ESP_LOG_WARN);

        // Init time (as early as possible, but SD card needs to be initialized)
        // ********************************************
        if (network_config.connection_type != NETWORK_CONNECTION_DISCONNECT)
        {
            ESP_LOGI(TAG, "setup Time");
            setupTime(); // NTP time service: Status of time synchronization will be checked after every round (server_tflite.cpp)
        }

        TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
        ESP_LOGD(TAG, "main: sleep for: %ldms", (long)xDelay * CONFIG_FREERTOS_HZ / portTICK_PERIOD_MS);
        vTaskDelay(xDelay);

        // manual reset the time
        // ********************************************
        if (network_config.connection_type != NETWORK_CONNECTION_DISCONNECT)
        {
            ESP_LOGI(TAG, "manual reset the time");
            if (!time_manual_reset_sync())
            {
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Manual Time Sync failed during startup");
            }
        }

        // Print Device info
        // ********************************************
        ESP_LOGI(TAG, "print Device info");
        esp_chip_info_t chipInfo;
        esp_chip_info(&chipInfo);

        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Device info: CPU cores: " + std::to_string(chipInfo.cores) + ", Chip revision: " + std::to_string(chipInfo.revision));

        // Print SD-Card info
        // ********************************************
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, "SD card info: Name: " + get_sd_card_name() + ", Capacity: " + get_sd_card_capacity() + "MB, Free: " + get_sd_card_free_partition_space() + "MB");

        xDelay = 2000 / portTICK_PERIOD_MS;
        ESP_LOGD(TAG, "main: sleep for: %ldms", (long)xDelay * CONFIG_FREERTOS_HZ / portTICK_PERIOD_MS);
        vTaskDelay(xDelay);

        // Check main init + start TFlite task
        // ********************************************
        ESP_LOGI(TAG, "check main init");
        if (get_system_status() == 0)
        {
            // No error flag is set
            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "Initialization completed successfully");
            InitializeFlowTask();
        }
        else if (is_set_system_statusflag(SYSTEM_STATUS_CAM_FB_BAD) || is_set_system_statusflag(SYSTEM_STATUS_NTP_BAD))
        {
            // Non critical errors occured, we try to continue...
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Initialization completed with non-critical errors!");
            InitializeFlowTask();
        }
        else
        {
            // Any other error is critical and makes running the flow impossible. Init is going to abort.
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Initialization failed. Flow task start aborted. Loading reduced web interface...");
        }
    }
}

bool setCpuFrequency(void)
{
    FILE *pFile = fopen(CONFIG_FILE, "r");
    if (pFile == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "No ConfigFile defined - exit setCpuFrequency()!");
        return false;
    }

    ClassFlow classFlow;
    std::string aktparamgraph = "";
    while (classFlow.GetNextParagraph(pFile, aktparamgraph))
    {
        if ((to_upper(aktparamgraph).compare("[SYSTEM]") == 0) || (to_upper(aktparamgraph).compare(";[SYSTEM]") == 0))
        {
            break;
        }
    }

    if ((to_upper(aktparamgraph).compare("[SYSTEM]") != 0) && (to_upper(aktparamgraph).compare(";[SYSTEM]") != 0))
    {
        fclose(pFile);
        return false;
    }

    string cpuFrequency = "160";
    esp_pm_config_t pm_config;

    std::vector<std::string> splitted;

    while (classFlow.getNextLine(pFile, &aktparamgraph) && !classFlow.isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (_param == "CPUFREQUENCY")
            {
                cpuFrequency = splitted[1];
            }
        }
    }
    fclose(pFile);

    if (esp_pm_get_configuration(&pm_config) != ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Failed to read CPU Frequency!");
        return false;
    }

    if (cpuFrequency == "160")
    {
        // 160 is the default
        // No change needed
    }
    else if (cpuFrequency == "240")
    {
        pm_config.max_freq_mhz = 240;
        pm_config.min_freq_mhz = pm_config.max_freq_mhz;
        
        if (esp_pm_configure(&pm_config) != ESP_OK)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Failed to set new CPU frequency!");
            return false;
        }
    }
    else
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Unknown CPU frequency: " + cpuFrequency + "! It must be 160 or 240!");
        return false;
    }

    if (esp_pm_get_configuration(&pm_config) == ESP_OK)
    {
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, string("CPU frequency: ") + to_string(pm_config.max_freq_mhz) + " MHz");
    }

    return true;
}
