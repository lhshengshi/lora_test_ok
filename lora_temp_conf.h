//
// Created by Gavin Xiao on 22/02/2017.
//

#ifndef GATEWAY2_LORA_TEMP_CONF_H
#define GATEWAY2_LORA_TEMP_CONF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*433.05-434.79 ,433.92*//*430-432*/
struct lora_para_setting {
    uint8_t sf;
    uint8_t bw;
    uint8_t cr;
    float cad;
    float symbol;
    float bps;
};

#define MAX_REFERENCE_LORA_SETTINGS 6
extern const struct lora_para_setting g_reference_lora_setting[MAX_REFERENCE_LORA_SETTINGS];


#ifdef __cplusplus
}
#endif

#endif //GATEWAY2_LORA_TEMP_CONF_H
