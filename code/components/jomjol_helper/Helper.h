#pragma once

#ifndef HELPER_H
#define HELPER_H

#include "sdkconfig.h"

#include <string>
#include <fstream>
#include <vector>

#include "sdmmc_cmd.h"

#include "defines.h"

#ifdef CONFIG_SOC_TEMP_SENSOR_SUPPORTED
#include <driver/temperature_sensor.h>
#endif

#if (ESP_IDF_VERSION_MAJOR >= 5)
#include "soc/periph_defs.h"
#include "esp_private/periph_ctrl.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "esp_rom_gpio.h"
#define gpio_pad_select_gpio esp_rom_gpio_pad_select_gpio
#define gpio_matrix_in(a, b, c) esp_rom_gpio_connect_in_signal(a, b, c)
#define gpio_matrix_out(a, b, c, d) esp_rom_gpio_connect_out_signal(a, b, c, d)
#define ets_delay_us(a) esp_rom_delay_us(a)
#endif

using namespace std;

/* Error bit fields
   One bit per error
   Make sure it matches https://jomjol.github.io/AI-on-the-edge-device-docs/Error-Codes */
enum SystemStatusFlag_t
{                                           // One bit per error
                                            // First Byte
   SYSTEM_STATUS_PSRAM_BAD = 1 << 0,        //  1, Critical Error
   SYSTEM_STATUS_HEAP_TOO_SMALL = 1 << 1,   //  2, Critical Error
   SYSTEM_STATUS_CAM_BAD = 1 << 2,          //  4, Critical Error
   SYSTEM_STATUS_SDCARD_CHECK_BAD = 1 << 3, //  8, Critical Error
   SYSTEM_STATUS_FOLDER_CHECK_BAD = 1 << 4, //  16, Critical Error

   // Second Byte
   SYSTEM_STATUS_CAM_FB_BAD = 1 << (0 + 8), //  8, Flow still might work
   SYSTEM_STATUS_NTP_BAD = 1 << (1 + 8),    //  9, Flow will work but time will be wrong
};

void string_to_ip4(const char *ip, int &a, int &b, int &c, int &d);
std::string bssid_to_string(const char *c);

std::string format_filename(std::string input);
std::size_t file_size(const std::string &file_name);
void find_replace(std::string &line, std::string &oldString, std::string &newString);

bool copy_file(string input, string output);
bool delete_file(string filename);
bool rename_file(string from, string to);
bool rename_folder(string from, string to);
bool make_dir(std::string _what);
bool file_exists(string filename);
bool folder_exists(string foldername);

string round_output(double _in, int _anzNachkomma);

size_t find_delimiter_pos(string input, string delimiter);

std::string trim_string_left_right(std::string istring, std::string adddelimiter = "");
std::string trim_string_left(std::string istring, std::string adddelimiter = "");
std::string trim_string_right(std::string istring, std::string adddelimiter = "");

bool ctype_space(const char c, string adddelimiter);

string get_file_type(string filename);
string get_file_full_filename(string filename);
string get_directory(string filename);

int mkdir_r(const char *dir, const mode_t mode);
int remove_folder(const char *folderPath, const char *logTag);

string to_lower(string in);
string to_upper(string in);

static float temp_sens_value = -1;
#ifdef CONFIG_SOC_TEMP_SENSOR_SUPPORTED
void init_tempsensor(void);
#endif
float read_tempsensor(void);

time_t add_days(time_t startTime, int days);
void mem_copy_gen(uint8_t *_source, uint8_t *_target, int _size);

std::vector<std::string> split_string(const std::string &str);
std::vector<std::string> split_line(std::string input, std::string _delimiter = "=");

///////////////////////////
size_t get_internal_heapsize();
size_t get_heapsize();
string get_heapinfo();

/////////////////////////////
string get_sd_card_partition_size();
string get_sd_card_free_partition_space();
string get_sd_card_partition_allocation_size();

void save_sd_card_info(sdmmc_card_t *card);
string sd_card_parse_manufacturer_ids(int);
string get_sd_card_manufacturer();
string get_sd_card_name();
string get_sd_card_capacity();
string get_sd_card_sector_size();

string get_mac(void);

void set_system_statusflag(SystemStatusFlag_t flag);
void clear_system_statusflag(SystemStatusFlag_t flag);
int get_system_status(void);
bool is_set_system_statusflag(SystemStatusFlag_t flag);

time_t get_uptime(void);
string get_reset_reason(void);
std::string get_formated_uptime(bool compact);

const char *get404(void);

std::string url_decode(const std::string &value);

void replace_all(std::string &s, const std::string &toReplace, const std::string &replaceWith);
bool replace_string(std::string &s, std::string const &toReplace, std::string const &replaceWith);
bool replace_string(std::string &s, std::string const &toReplace, std::string const &replaceWith, bool logIt);

std::string encrypt_decrypt_string(std::string toEncrypt);
std::string encrypt_pw_string(std::string toEncrypt);
std::string decrypt_pw_string(std::string toDecrypt);
esp_err_t encrypt_decrypt_pw_on_sd(bool _encrypt, std::string filename);

bool is_in_string(std::string &s, std::string const &toFind);
bool is_string_numeric(std::string &input);
bool is_string_alphabetic(std::string &input);
bool is_string_alphanumeric(std::string &input);
bool alphanumeric_to_boolean(std::string &input);

int clip_int(int input, int high, int low);
bool numeric_str_to_boolean(std::string input);
bool string_to_boolean(std::string input);

#endif // HELPER_H
