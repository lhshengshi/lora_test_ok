//
// Created by Gavin Xiao on 7/18/16.
//

#ifndef SERVER_LORACONFMGR_H
#define SERVER_LORACONFMGR_H

#include <stdint.h>
#include <string>
#include <include/zigbee_codec.h>
#include "json/json.h"
#include "lora/lora_common.h"


class NetLoraConfMgr {

public:
    static NetLoraConfMgr& getInstance() { return _inst; }

    bool init(uint16_t panid);

    bool getLoraConfItem(ZMsgCommNodeConf& conf, uint16_t addr, uint8_t nodeType, uint8_t nodeSubType);

    bool setLoraTxPowerDbm(uint8_t dbm);
    uint8_t getLoraTxPowerDbm() const {
        return _globalLoraConf.loraTxPower + 5;
    }

    enum LoraConfType {
        LoraConfFull,
        LoraConfWakeup,
        LoraConfMax
    };
    const struct setpara& getLora2Params() {
        return _globalLoraConf;
    }

    const struct setpara& getLoraDefault2Params() {
        return _globalLoraDefaultConf;
    }

    // handy tools
    typedef uint8_t FreqArrayType[4];
    static uint32_t freqArray2uint(const FreqArrayType &freq);
    static void uint2freqArray(FreqArrayType& freq, uint32_t freqUint);

protected:

    std::string toJson(struct setpara& loraConf) const ;
    bool fromJson(struct setpara& loraConf, const std::string& confStr);

    void _toJson(const struct lorapara& loraConf, Json::Value& json) const ;
    bool _fromJson(struct lorapara& loraConf, const Json::Value& json);

    static NetLoraConfMgr _inst;

    struct setpara _globalLoraConf;
    struct setpara _globalLoraDefaultConf;
};


#endif //SERVER_LORACONFMGR_H
