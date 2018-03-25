//
// Created by Gavin Xiao on 22/01/2017.
//

#include "lora_temp_conf.h"

const struct lora_para_setting g_reference_lora_setting[MAX_REFERENCE_LORA_SETTINGS] =
        {
                [0] = /*for long distance*/
                        {
                                .sf = 9,
                                .bw = 7,/*125khz,7*/
                                .cr = 2,
                                .cad = 7.0,
                                .symbol = 4.096,
                                .bps = 1464.84,
                        },
                [1] = /*for long distance*/
                        {
                                .sf = 8,
                                .bw = 7,/*125khz,7*/
                                .cr = 2,
                                .cad = 3.5,
                                .symbol = 2.048,
                                .bps = 2604.17,
                        },
                [2] = /*recommend setting for full work*/
                        {
                                .sf = 7,
                                .bw = 7,/*125khz,7*/
                                .cr = 2,
                                .cad = 1.8,
                                .symbol = 1.024,
                                .bps = 4557.59,
                        },
                [3] =  /*recommend setting for lowpower*/
                        {
                                .sf = 7,
                                .bw = 8,/*250khz,8*/
                                .cr = 2,
                                .cad = 1.2,
                                .symbol = 0.512,
                                .bps = 9114.58,
                        },
                [4] = /*recommend setting for lowpower*/
                        {
                                .sf = 8,
                                .bw = 8,/*250khz  8*/
                                .cr = 2,
                                .cad = 2.3,
                                .symbol = 1.02,
                                .bps = 5208.33,
                        },
                [5] = /*recommend setting for full work*/
                        {
                                .sf = 8,
                                .bw = 9,/*500khz  9*/
                                .cr = 2,
                                .cad = 1.7,
                                .symbol = 0.51,
                                .bps = 12500,

                        }
                /*  [0: 7.8kHz,1:10.4kHz,2:15.6 kHz,3: 20.8 kHz,4:31.2kHz,5:41.6 kHz,6:62.5kHz,
                      7: 125kHz,8:250 kHz,9:500kHz,other: Reserved]
              */
        };

