#include "main.h"
#include "net.h"
#include "parameters.h"

static const char *TAG = "PARAMETERS";

location_t device_location;
stepper_t device_stepper;

esp_err_t nvs_err;
uint32_t my_handle; // NVS handle

void command_ota(void){
    saveParameter();
    //xTaskCreate(&ota_task, "ota_task", 16384, NULL, 3, NULL);
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
}

esp_err_t command_reset(){
    saveParameter();
    esp_restart();
    return ESP_OK;
}

// Save stepper tuning info to NVS
esp_err_t command_set_stall(stepper_cfg_t stepper_cfg_in) {
    nvs_set_value("stall", stepper_cfg_in.stall);
    nvs_set_value("tcool", stepper_cfg_in.tcool);
    nvs_set_value("tpwm", stepper_cfg_in.tpwm);
    return ESP_OK;
}

// Get stepper tuning info from NVS
stepper_cfg_t command_get_stall() {
    stepper_cfg_t stepper_cfg;
    stepper_cfg.stall = nvs_get_value("stall");
    stepper_cfg.tcool = nvs_get_value("tcool");
    stepper_cfg.tpwm = nvs_get_value("tpwm");
    return stepper_cfg;
}

//PARAMTER SAVING
void nvs_init(){
    // Initialize NVS.
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "Failed to init NVS, erasing and trying again");
        // 1.OTA app partition table has a smaller NVS partitionb size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
}
int32_t nvs_get_value(char* name){

    int32_t value = 0;

    nvs_err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (nvs_err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(nvs_err));
    } else {
        printf("Done\n");

        // Read
        printf("Reading restart counter from NVS ... ");
        nvs_err = nvs_get_i32(my_handle, name, &value);
        switch (nvs_err) {
            case ESP_OK:
                ESP_LOGI(TAG,"Done\n");
                ESP_LOGI(TAG,"Retrieved value counter = %d\n", value);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG,"The value is not initialized yet!\n");
                break;
            default :
                ESP_LOGI(TAG,"Error (%s) reading!\n", esp_err_to_name(nvs_err));
        }
    }
     // Close
     nvs_close(my_handle);
     return value;

}
void nvs_set_value(char* name, int32_t value){

    nvs_err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (nvs_err != ESP_OK) {
        ESP_LOGI(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(nvs_err));
    } else {
        ESP_LOGI(TAG,"Done\n");

    // Write
        nvs_err = nvs_set_i32(my_handle, name, value);

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        ESP_LOGI(TAG,"Committing updates in NVS ... ");
        nvs_err = nvs_commit(my_handle);
        // Close
        nvs_close(my_handle);

    }

}
//LOCATION SAVING
location_t command_init_location(){

  location_t location;

  ESP_LOGI(TAG, "LOAD LOCATION");

  location.x = nvs_get_value("location_x");
  location.y = nvs_get_value("location_y");
  location.z = nvs_get_value("location_z");
  return location;
}
esp_err_t command_set_location(location_t location){

    nvs_set_value("location_x",location.x);
    nvs_set_value("location_y",location.y);
    nvs_set_value("location_z",location.z);
    device_location = command_init_location();

    return ESP_OK;
}
//STEPPER SAVING
stepper_t command_init_stepper(){

  ESP_LOGI(TAG, "LOAD STEPPER");
  stepper_t stepper;
  stepper.current = nvs_get_value("stepper_current");
  stepper.min = nvs_get_value("stepper_min");
  stepper.max = nvs_get_value("stepper_max");
  stepper.target = nvs_get_value("stepper_target");
  stepper.number = nvs_get_value("stepper_number");
  return stepper;
}
esp_err_t command_set_stepper(stepper_t stepper){

    nvs_set_value("stepper_current",stepper.current);
    nvs_set_value("stepper_min",stepper.min);
    nvs_set_value("stepper_max",stepper.max);
    nvs_set_value("stepper_target",stepper.target);
    nvs_set_value("stepper_number",stepper.number);
    device_stepper = command_init_stepper();

    return ESP_OK;
}
void setParameter(int type, int value){

    ESP_LOGI(TAG, "SET PARAMETER %d : %d", type, value);

    if (type==1) device_stepper.current = value;
    if (type==2) device_stepper.min = value;
    if (type==3) device_stepper.max = value;
    if (type==4) device_stepper.target = value;
    if (type==5) device_stepper.number = value;
    
}
void saveParameter(){
    stepper_t step;
    step.current = device_stepper.current;
    step.min = device_stepper.min;
    step.max = device_stepper.max;
    step.target = device_stepper.target;
    step.number = device_stepper.number;
    command_set_stepper(step);

    ESP_LOGI(TAG, "SET PARAMETER %d : %d : %d : %d : %d", step.current, step.min, step.max, step.target,step.number);
}