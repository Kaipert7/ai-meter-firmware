FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.*)

idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS "." "../../include"
                    REQUIRES driver esp_eth esp_netif esp_wifi mdns nvs_flash jomjol_helper jomjol_mqtt wpa_supplicant)


