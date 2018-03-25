//
// Created by Gavin Xiao on 7/18/16.
//

#include <3rd-party/include/json/json.h>
#include "serverUtils.h"
#include "server_common.h"
#include "db2/DbSystemProperties2.h"
#include "ZigbeeNodeMgr.h"
#include "lora_temp_conf.h"
#include "InitOnce.h"
#include "NetLoraConfMgr.h"
#include "ZigbeeNodeInfo.h"

static std::vector< std::pair<uint32_t /*freq*/, int /* setting idx*/> > s_lora_confs;
struct InitLoraParamSet {
    static bool initOnce() {
        for(int i=1; i<50; i++) {
            s_lora_confs.push_back(std::make_pair((uint32_t)(779078125 + i * 156250),  2));
        }

        for(int i=0; i<25; i++) {
            s_lora_confs.push_back(std::make_pair((uint32_t)(779156250 + i * 312500),  4));
        }

        if((s_lora_confs.size()%2) != 0) {
            LogWar("The configuration number is odd, remove one");
            s_lora_confs.pop_back();
        }

        return true;
    }
};

static InitOnce<InitLoraParamSet> s_initLoraParam;

#if 0
static uint32_t recommend_freq_bw125[] = {
    779234375, 779390625, 779546875, 779703125, 779859375,
    780015625, 780171875, 780328125, 780484375, 780640625,
    780796875, 780953125, 781109375, 781265625, 781421875,
    781578125, 781734375, 781890625, 782046875, 782203125,
    782359375, 782515625, 782671875, 782828125, 782984375,
    783140625, 783296875, 783453125, 783609375, 783765625,
    783921875, 784078125, 784234375, 784390625, 784546875,
    784703125, 784859375, 785015625, 785171875, 785328125,
    785484375, 785640625, 785796875, 785953125, 786109375,
    786265625, 786421875, 786578125, 786734375
};

static uint32_t recommend_freq_bw250[] = {
    779156250, 779468750, 779781250, 780093750, 780406250,
    780718750, 781031250, 781343750, 781656250, 781968750,
    782281250, 782593750, 782906250, 783218750, 783531250,
    783843750, 784156250, 784468750, 784781250, 785093750,
    785406250, 785718750, 786031250, 786343750, 786656250
};
#endif

NetLoraConfMgr NetLoraConfMgr::_inst;

bool NetLoraConfMgr::fromJson(struct setpara& loraConf, const std::string& confStr)
{
    try {
        Json::Value json;
        Json::Reader jsonReader;
        jsonReader.parse(confStr, json, false);

        bzero(&loraConf, sizeof(loraConf));

        loraConf.PA_BOOST_select = jsonGetInt(json, "paBoost", 0);
        loraConf.loraTxPower = jsonGetInt(json, "txPower", 8);
        loraConf.spare= 0;
        loraConf.default_working_mode = jsonGetInt(json, "recvMode", 0);
        loraConf.broadcast_filter_onoff = jsonGetInt(json, "enableBroadcastFilter", 0);
        loraConf.nodeid_filter_onoff = jsonGetInt(json, "enableNodeidMask", 0);
        loraConf.netid = jsonGetInt(json, "netid", 0);
        loraConf.nodeid = jsonGetInt(json, "nodeid", 0);

        Json::Value& jsonFull = json["fullMode"];
        _fromJson(loraConf.rfpara_fullwork, jsonFull);

        Json::Value& jsonWakeup = json["wakeupMode"];
        _fromJson(loraConf.rfpara_lowpower_sub.rfpara_lowpower, jsonWakeup);
        loraConf.rfpara_lowpower_sub.waketimes = jsonGetInt(json, "wakeuptimes", 3);

        return true;
    } catch (std::exception e) {
        LogWar("Failed to parse the Lora conf, e='%s'", e.what());
    }

    return false;
}

std::string NetLoraConfMgr::toJson(struct setpara& loraConf) const
{
    Json::Value json;
    Json::FastWriter writer;

    json["paBoost"] = loraConf.PA_BOOST_select;
    json["txPower"] = (int) loraConf.loraTxPower;
    json["recvMode"] = loraConf.default_working_mode;
    json["enableBroadcastFilter"] = loraConf.broadcast_filter_onoff;
    json["enableNodeidMask"] = loraConf.nodeid_filter_onoff;
    json["netid"] = loraConf.netid;
    json["nodeid"] = loraConf.nodeid;

    Json::Value jsonFullMode;
    _toJson(loraConf.rfpara_fullwork, jsonFullMode);
    json["fullMode"] = jsonFullMode;

    Json::Value jsonWakeupMode;
    _toJson(loraConf.rfpara_lowpower_sub.rfpara_lowpower, jsonWakeupMode);
    jsonWakeupMode["wakeuptimes"] = loraConf.rfpara_lowpower_sub.waketimes;
    json["wakeupMode"] = jsonWakeupMode;

    return writer.write(json);
}

bool NetLoraConfMgr::init(uint16_t panid)
{
    bool inited = false;
    db2::DbSystemProperties2 db;

    std::string loraConfStr = db.getProperty(db2::DbSystemProperties2::SP_LORA2_PARAMS, "");
    if(loraConfStr.length() != 0) {
        LogInfo("Read lora conf from DB. '%s'", loraConfStr.c_str());
        if(fromJson(_globalLoraConf, loraConfStr)) {
            if(_globalLoraConf.netid != 0 ) {
                inited = true;
            }
        }
    }

    if (!inited) {
        LogInfo("Init the Lora params at the first time");
        bzero(&_globalLoraConf, sizeof(_globalLoraConf));

        _globalLoraConf.netid = panid;
        _globalLoraConf.nodeid = 1;
        _globalLoraConf.broadcast_filter_onoff = 1;
        _globalLoraConf.nodeid_filter_onoff = 1;
        _globalLoraConf.PA_BOOST_select = 1;
        _globalLoraConf.loraTxPower = 8;
        _globalLoraConf.spare = 0;

        // full mode
        struct lorapara& confFull = _globalLoraConf.rfpara_fullwork;

        int indexFull = rand() % (s_lora_confs.size());
        indexFull = (indexFull >> 1) << 1;
        uint32_t freqFull = s_lora_confs[indexFull].first;
        int seed = s_lora_confs[indexFull].second;
        LogInfo("panId=%d, fullIdx=%d, seed=%d, freq=%d", panid, indexFull, seed, freqFull);

        const struct lora_para_setting& settingsFull = g_reference_lora_setting[seed];
        uint2freqArray(confFull.freq, freqFull);

        confFull.signal_bw = settingsFull.bw;
        confFull.spreading_factor = settingsFull.sf;
        confFull.error_coding = settingsFull.cr;

        confFull.bps = settingsFull.bps;

        LogWar("init, full-mode, freq=%d, bw=%d, sf=%d, ec=%d, bps=%d", freqFull, confFull.signal_bw, confFull.spreading_factor,  confFull.error_coding, confFull.bps);

        // wakeup mode
        struct lorapara_lowrev& confWakeup = _globalLoraConf.rfpara_lowpower_sub;

        int indexWakeup = (indexFull + 5)  % (s_lora_confs.size());
        uint32_t freqWakeup = s_lora_confs[indexWakeup].first;
        seed = s_lora_confs[indexWakeup].second;
        LogInfo("panId=%d, wakeupIdx=%d, seed=%d, freq=%d", panid, indexWakeup, seed, freqWakeup);

        const struct lora_para_setting& settingsWakeup = g_reference_lora_setting[seed];
        uint2freqArray(confWakeup.rfpara_lowpower.freq, freqWakeup);

        confWakeup.rfpara_lowpower.signal_bw = settingsWakeup.bw;
        confWakeup.rfpara_lowpower.spreading_factor = settingsWakeup.sf;
        confWakeup.rfpara_lowpower.error_coding = settingsWakeup.cr;

        confWakeup.rfpara_lowpower.bps = settingsWakeup.bps;

        confWakeup.waketimes = 3;
        LogWar("init, lp-mode, freq=%d, bw=%d, sf=%d, ec=%d, bps=%d", freqWakeup, confWakeup.rfpara_lowpower.signal_bw,
               confWakeup.rfpara_lowpower.spreading_factor,  confWakeup.rfpara_lowpower.error_coding, confWakeup.rfpara_lowpower.bps
        );

        loraConfStr = toJson(_globalLoraConf);
        LogDbg("Save lora conf to db, str='%s'", loraConfStr.c_str());
        db.setProperty(db2::DbSystemProperties2::SP_LORA2_PARAMS, loraConfStr);
    }

    inited = false;
    std::string loraDefaultConfStr = db.getProperty(db2::DbSystemProperties2::SP_LORA2_DEFAULT_MODE_PARAMS, "");
    if(loraDefaultConfStr.length() != 0) {
        LogInfo("Read lora default conf from DB. '%s'", loraDefaultConfStr.c_str());
        if(fromJson(_globalLoraDefaultConf, loraDefaultConfStr)) {
            if(_globalLoraDefaultConf.netid != 0 ) {
                inited = true;
            }
        }
    }

    if (!inited) {
        LogInfo("Init the Lora default params at the first time");
        bzero(&_globalLoraDefaultConf, sizeof(_globalLoraDefaultConf));

        _globalLoraDefaultConf.PA_BOOST_select = 1;
        _globalLoraDefaultConf.default_working_mode = 0;
        _globalLoraDefaultConf.broadcast_filter_onoff = 1;
        _globalLoraDefaultConf.nodeid_filter_onoff = 1;
        _globalLoraDefaultConf.netid = 0xffff;
        _globalLoraDefaultConf.nodeid = 1;

        // default mode
        struct lorapara& confDefault = _globalLoraDefaultConf.rfpara_fullwork;
        //const struct lora_para_setting& fullSettings = g_reference_lora_setting[4];

        uint32_t freq = LORA_DEFAULT_FREQ;
        uint2freqArray(confDefault.freq, freq);
        confDefault.signal_bw = LORA_DEFAULT_BW;
        confDefault.spreading_factor = LORA_DEFAULT_SF;
        confDefault.error_coding = LORA_DEFAULT_CR;

        confDefault.bps = LORA_DEFAULT_BPS;

        // use the same config in low power for default mode
        _globalLoraDefaultConf.rfpara_lowpower_sub.rfpara_lowpower = _globalLoraDefaultConf.rfpara_fullwork;
        _globalLoraDefaultConf.rfpara_lowpower_sub.waketimes = 1;

        LogWar("init, default-mode, freq=%d, bw=%d, sf=%d, ec=%d, bps=%d", freq, confDefault.signal_bw, confDefault.spreading_factor,  confDefault.error_coding, confDefault.bps);

        loraDefaultConfStr = toJson(_globalLoraDefaultConf);
        LogDbg("Save lora default conf to db, str='%s'", loraDefaultConfStr.c_str());
        db.setProperty(db2::DbSystemProperties2::SP_LORA2_DEFAULT_MODE_PARAMS, loraDefaultConfStr);
    }

    return true;
}

bool NetLoraConfMgr::getLoraConfItem(ZMsgCommNodeConf& conf, uint16_t addr, uint8_t nodeType, uint8_t nodeSubType)
{
    conf._params = _globalLoraConf;
    if(ZigbeeNodeMgr::getInstance().isBatteryPoweredNode(nodeType, nodeSubType)) {
        conf._params.loraTxPower = 8; // 13 dbm
    }

    conf._params.nodeid = addr;
    conf._params.nodeid_filter_onoff = 1;

    if(ZigbeeNodeMgr::isLoraWakeupNode(nodeType, nodeSubType)) {
        conf._params.broadcast_filter_onoff = 0;
        conf._params.default_working_mode = 1;
    } else {
        conf._params.broadcast_filter_onoff = 1;
        conf._params.default_working_mode = 0;
    }

    return true;
}


bool NetLoraConfMgr::setLoraTxPowerDbm(uint8_t dbm)
{
    if(dbm <= 6) {
        dbm = 1;
    } else if(dbm >= 20) {
        dbm = 15;
    } else {
        dbm -= 5;
    }

    if(dbm != _globalLoraConf.loraTxPower) {
        _globalLoraConf.loraTxPower = dbm;

        std::string loraConfStr = toJson(_globalLoraConf);
        db2::DbSystemProperties2 db;
        LogDbg("Save lora conf to db2, str='%s'", loraConfStr.c_str());
        db.setProperty(db2::DbSystemProperties2::SP_LORA2_PARAMS, loraConfStr);

        return true;
    } else {
        return false;
    }

}

#if 0
static uint8_t cal_par_checksum(struct set433para * para)
{
    uint8_t checksum;

    checksum = 0;
    for(int i=0;i<sizeof(struct set433para)-1-VERSION_LENS;i++)
    {
        checksum  ^= ((uint8_t *)(&para->PA_BOOST_select))[i];
    }
    return checksum;
}
#endif


void NetLoraConfMgr::_toJson(const struct lorapara& conf, Json::Value& json) const
{
    uint32_t freq = freqArray2uint(conf.freq);
    json["freq"] = freq;

    json["signal_bw"] = (int)conf.signal_bw;
    json["spreading_factor"] = (int)conf.spreading_factor;
    json["error_coding"] = (int)conf.error_coding;

    json["bps"] = conf.bps;
}

bool NetLoraConfMgr::_fromJson(struct lorapara& conf, const Json::Value& json)
{
    uint32_t freq = jsonGetInt(json, "freq", 0);
    uint2freqArray(conf.freq, freq);

    conf.signal_bw = jsonGetInt(json, "signal_bw", 0);
    conf.spreading_factor = jsonGetInt(json, "spreading_factor", 0);
    conf.error_coding = jsonGetInt(json, "error_coding", 0);

    conf.bps = jsonGetInt(json, "bps", 0);

    return true;
}

uint32_t NetLoraConfMgr::freqArray2uint(const FreqArrayType &freqArray)
{
    uint32_t freq = freqArray[3];
    freq = (freq<<8) | freqArray[2];
    freq = (freq<<8) | freqArray[1];
    freq = (freq<<8) | freqArray[0];

    return freq;
}

void NetLoraConfMgr::uint2freqArray(FreqArrayType& freq, uint32_t freqUint)
{
    freq[0] = (freqUint) & 0xff;
    freq[1] = (freqUint >> 8) & 0xff;
    freq[2] = (freqUint >> 16) & 0xff;
    freq[3] = (freqUint >> 24) & 0xff;
}
