#include <fcntl.h>
#include <string.h>
#include <network/NetworkMgr.h>
#include <3rd-party/include/lora_api.h>
#include "msg_utils.h"
#include "EventCenter.h"
#include "lora/NetLoraConfMgr.h"
#include "ZigbeeNodeMgr.h"
#include "DeviceMgr.h"
#include "LoraNetworkDefaultMode.h"
#include "serverUtils.h"
#include "AppMsgSender.h"
#include "crc.h"


LoraNetworkDefaultMode LoraNetworkDefaultMode::_inst;

LoraNetworkDefaultMode::LoraNetworkDefaultMode()
{
    _internalStatus = STATUS_SAFE;

    _discoveryModeCmd = NETWORK_MODE_MAX;
    _cmdWithDevFilter = false;
    _cmdFilterDevList.clear();

    _discoverModeStartInMs = 0;

    _appCmd = "lora";
}

LoraNetworkDefaultMode::~LoraNetworkDefaultMode()
{
}

void LoraNetworkDefaultMode::broadcastStatusToApp(const std::string& status)
{
    Json::Value json;
    json["cmd"] = _appCmd;
    json["cmdType"] = "setStatus";
    json["status"] = status;
    std::string info = _discoveryResult;
    json["info"] = info;

    AppMsgSender::getInstance().sendMsgToAllClients(json);
}

void LoraNetworkDefaultMode::setNetworkMode(LoraNetworkMode mode, const std::string& appCmd)
{
    _discoveryModeCmd = mode;
    _cmdWithDevFilter = false;
    _cmdFilterDevList.clear();
    _appCmd = appCmd;
}

void LoraNetworkDefaultMode::setNetworkMode(LoraNetworkMode mode, const std::set<std::string>& deviceList, const std::string& appCmd)
{
    _discoveryModeCmd = mode;
    _cmdWithDevFilter = true;
    _cmdFilterDevList = deviceList;
    _appCmd = appCmd;
}

void LoraNetworkDefaultMode::checkNetworkMode(tSERVER *tpServer) {
    // process command
    if(_discoveryModeCmd == NETWORK_MODE_DEFAULT) { // cmd to enter default mode
        LogInfo("1. ModCmd=%d, status=%d", _discoveryModeCmd, _internalStatus);
        if(_internalStatus == STATUS_SAFE) {
            enterDefaultMode(tpServer);
        } // already in default
        _discoveryModeCmd = NETWORK_MODE_MAX;
    } else if(_discoveryModeCmd == NETWORK_MODE_SAFE) { // cmd to enter safe mode
        LogInfo("2. ModCmd=%d, status=%d", _discoveryModeCmd, _internalStatus);
        if(_internalStatus == STATUS_DEFAULT || _internalStatus == STATUS_WAIT_FORCE_SAFE_CMD) {
            _startToChangeParams();
            _discoveryModeCmd = NETWORK_MODE_MAX;
        } else if(_internalStatus == STATUS_SAFE) {
            _discoveryModeCmd = NETWORK_MODE_MAX;
        } else { // else, not in stable mode, wait
            LogWar("Ignore the change param command");
        }
    } else if(_discoveryModeCmd == NETWORK_MODE_FORCE_SAFE) { // cmd to enter safe mode
        LogInfo("2. ModCmd=%d, status=%d", _discoveryModeCmd, _internalStatus);
        if(_internalStatus != STATUS_SAFE) {
            leaveDefaultMode(tpServer);
        }
        _discoveryModeCmd = NETWORK_MODE_MAX;
    }

        if(_internalStatus != STATUS_SAFE) {
            // check whether to enter safe mode automaticly.
            if (is_timeout(_discoverModeStartInMs, get_currenttime_in_ms(), 10L * 60L * 1000L)) { // 10 minutes
                LogInfo("Stay on defult mode more than 10 minutes, change to safe mode");
                leaveDefaultMode(tpServer);
                _discoveryModeCmd = NETWORK_MODE_MAX;
                return;
            }

            if(_internalStatus == STATUS_CHANGE_PARAM) {
                if(!_changeOneDevicePara()) {
                    if(_checkAllChangeParaAckReceived()) {
                        leaveDefaultMode(tpServer);
                    } else {
                        if(_changeParamRetries < 2) {
                            _changeParamRetries++;
                            _changeParamDevIdx = -1;
                            _changeParamDevInMs = 0;
                            LogWar("ChangeParam failed for some device, retries=%d", _changeParamRetries);
                            _changeOneDevicePara();
                        } else {
                            _internalStatus = STATUS_WAIT_FORCE_SAFE_CMD;
                            broadcastStatusToApp("forceSafe");
                        }
                    }
                }
            } // else, do nothing
        }

}

void LoraNetworkDefaultMode::_startToChangeParams()
{
    const ZigbeeNetworkParams& safeNetworkParams = NetworkMgr::getInstance().getZigbeeSafeNetworkParams();
    LogInfo("LoraNetworkDefaultMode, begin changing parameters, ch=%d, panid=0x%04x, devNum=%d", safeNetworkParams._ch, safeNetworkParams._panId, (int)_discoveredList2.size());

    _changeParamRetries = 0;
    _changeParamDevIdx = -1;
    _changeParamDevInMs = 0;

    _internalStatus = STATUS_CHANGE_PARAM;
}

bool LoraNetworkDefaultMode::_changeOneDevicePara()
{
    uint64_t timeNow = get_currenttime_in_ms();

    if(_changeParamDevIdx >= 0) { // change param in progress
        if(_changeParamDevIdx >= (int)_discoveredList2.size()) { // all done
            return false;
        }

        LoraDiscoveredDevice& dd = _discoveredList2[_changeParamDevIdx];
        if(dd.change_par_ack) {
            if(!dd.reset_sent) {
                _setDevReset(dd.mac, _changeParamDevIdx * 2);
                dd.reset_sent = true;
                return true;
            }
        } else {
            if(timeNow < _changeParamDevInMs + 2000) { // wait timeout
                return true;
            } // else, try next device
        }
    }

    while (++_changeParamDevIdx < (int) _discoveredList2.size()) {
        LoraDiscoveredDevice &dd = _discoveredList2[_changeParamDevIdx];

        if (_cmdWithDevFilter && _cmdFilterDevList.find(dd.macStr) == _cmdFilterDevList.end()) {
            continue;
        }

        if (dd.type != NODE_TYPE_UNKNOWN && !dd.change_par_ack) {
            struct ZMsgChangeZigbeeParam workPara;
            const ZigbeeNetworkParams &safeNetworkParams = NetworkMgr::getInstance().getZigbeeSafeNetworkParams();
            zigbeeNetworkParamsTrans(&safeNetworkParams, &workPara);

            LogInfo("Send ChangePara command, mac=%s, chan=%d\n", dd.macStr.c_str(), workPara.ucCh);
            memcpy(workPara.mac, dd.mac, sizeof(workPara.mac));
            _changePara(&workPara);
            _changeParamDevInMs = timeNow;
            return true;
        }
    }

    return false;
}

bool LoraNetworkDefaultMode::enterDefaultMode(tSERVER *tpServer) {
    LogInfo("LoraNetworkDefaultMode -- try enter default network mode\n");

    if(_internalStatus != STATUS_SAFE) {
        _discoveryResult = "zigbee network not in expect status";
        LogInfo("ZigbeeNetworkDefaultMode, %s, status=%d, abort", _discoveryResult, _internalStatus);
        return false;
    }

    if(!_writeEnterLoraDefaultConf(NetLoraConfMgr::getInstance().getLoraDefault2Params())) {
        _discoveryResult = "Failed to write parameters";
        return false;
    }
    LogInfo("Change lora default mode succ");

    _internalStatus = STATUS_DEFAULT;

    _clearDiscoveredDevices();

    _discoveryResult = "default mode";

    _discoverModeStartInMs = get_currenttime_in_ms();

    LogInfo("LoraNetworkDefaultMode -- set default network mode success\n");

    EventCenter::getInstance().onLoraNetworkModeChanged(_internalStatus);

    return true;
}


bool LoraNetworkDefaultMode::leaveDefaultMode(tSERVER *tpServer) {
    LogInfo("LoraNetworkDefaultMode -- try leave default network mode\n");

    if(_internalStatus == STATUS_SAFE) {
        _discoveryResult = "zigbee network not in expect status";
        LogInfo("LoraNetworkDefaultMode, zigbee network not in expect status=%d, abort", _internalStatus);
        return false;
    }

    _writeExitLoraDefaultConf();

    _internalStatus = STATUS_SAFE;
    _discoveryResult = "ok";

    // clear resources
    _clearDiscoveredDevices();

    LogInfo("LoraNetworkDefaultMode -- set safe network mode success\n");
    EventCenter::getInstance().onLoraNetworkModeChanged(_internalStatus);
    return true;
}

int LoraNetworkDefaultMode::processAuthResp(uint8_t* mac, uint16_t type, uint16_t subType, uint8_t rssi) {
    bool newDevice = true;
    for (std::vector<LoraDiscoveredDevice>::iterator it = _discoveredList2.begin(); it != _discoveredList2.end(); ++it) {
        if (0 == memcmp(it->mac, mac, sizeof(it->mac))) {
            newDevice = false;
            break;
        }
    }

    if(newDevice) {
        LoraDiscoveredDevice dd;
        memcpy(dd.mac, mac, sizeof(dd.mac));
        char macStr[32]={0};
        sprintf(macStr,"%02x%02x%02x%02x%02x%02x%02x%02x", dd.mac[0], dd.mac[1], dd.mac[2], dd.mac[3], dd.mac[4], dd.mac[5], dd.mac[6], dd.mac[7]);
        dd.macStr = macStr;

        dd.change_par_ack = false;
        dd.reset_sent = false;
        dd.type = type;
        dd.subType = subType;
        dd.rssi = rssi;
        _discoveredList2.push_back(dd);
        std::sort(_discoveredList2.begin(), _discoveredList2.end(), DiscoveredDeviceComp());
        _notifyDeviceToAllApp(dd);
    }
    return 0;
}

bool LoraNetworkDefaultMode::getDeviceList(Json::Value& outJson)
{
    Json::Value devList (Json::arrayValue);

    for (std::vector<LoraDiscoveredDevice>::iterator it = _discoveredList2.begin(); it != _discoveredList2.end(); ++it) {
        Json::Value dev(Json::objectValue);

        dev["id"] = it->macStr;
        _translateType(dev, it->type, it->subType, it->macStr);
        dev["rssi"] = it->rssi;
        dev["changeParaAcked"] = it->change_par_ack;

        devList.append(dev);
    }

    outJson["deviceList"] = devList;

    return true;
}

bool LoraNetworkDefaultMode::processChangeParaAck(tSERVER *tpServer, uint8_t* mac, uint16_t receive_crc, int32_t len)
{
    struct ZMsgChangeZigbeeParam workPara;
    const ZigbeeNetworkParams& safeNetworkParams = NetworkMgr::getInstance().getZigbeeSafeNetworkParams();
    zigbeeNetworkParamsTrans(&safeNetworkParams, &workPara);
    memcpy(workPara.mac, mac, sizeof(workPara.mac));

    uint16_t para_crc = crc16_ansi_0618(workPara.aPanid, sizeof(workPara)-2);

    LogDbg("Receive changePara ack from mac : %02x%02x%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7]);
    if(para_crc == receive_crc) {
        for (std::vector<LoraDiscoveredDevice>::iterator it = _discoveredList2.begin();
             it != _discoveredList2.end(); it++) {
            if (0 == memcmp(it->mac, mac, sizeof(it->mac))) {
                LoraDiscoveredDevice& dd = *it;
                LogWar("changePara ack accepted for mac %s", dd.macStr.c_str());
                dd.change_par_ack = true;
                _notifyDeviceToAllApp(dd);
                return true;
            }
        }
    }
    else {
        LogInfo("CRC wrong, para=0x%x, recv=0x%x\n", para_crc, receive_crc);
    }

    return false;
}


bool LoraNetworkDefaultMode::_checkAllChangeParaAckReceived()
{
    for(std::vector<LoraDiscoveredDevice>::iterator it = _discoveredList2.begin(); it != _discoveredList2.end(); it++) {
        if(_cmdWithDevFilter && _cmdFilterDevList.find(it->macStr) == _cmdFilterDevList.end()) {
            continue;
        }

        if(!it->change_par_ack) {
            return false;
        }
    }
    return true;
}

void LoraNetworkDefaultMode::_clearDiscoveredDevices()
{
    _discoveredList2.clear();
}

void LoraNetworkDefaultMode::_notifyDeviceToAllApp(const LoraDiscoveredDevice& dd)
{
    Json::Value json(Json::objectValue);
    Json::Value dev(Json::objectValue);

    json["sequence"] = 1234;
    json["cmd"] = _appCmd;
    json["cmdType"] = "deviceOnline";

    dev["id"] = dd.macStr;
    _translateType(dev, dd.type, dd.subType, dd.macStr);
    dev["rssi"] = dd.rssi;
    dev["changeParaAcked"] = dd.change_par_ack;

    json["device"] = dev;

    AppMsgSender::getInstance().sendMsgToAllClients(json);
}

bool LoraNetworkDefaultMode::_writeEnterLoraDefaultConf(const struct setpara& para)
{
    uint32_t freqLow = NetLoraConfMgr::freqArray2uint(para.rfpara_lowpower_sub.rfpara_lowpower.freq);
    uint32_t freqFull = NetLoraConfMgr::freqArray2uint(para.rfpara_fullwork.freq);

    LogDbg("SetLoraParams, freqLowPower=%d, freqFull=%d", freqLow, freqFull);

    if(!loraSetTempConf(&para)) {
        LogCrit("Failed to set default LoraParams");
        return false;
    }

    return true;
}

bool LoraNetworkDefaultMode::_writeExitLoraDefaultConf()
{
    if(!loraClearTempConf()) {
        LogCrit("Failed to clear default LoraParams");
        return false;
    }

    return true;
}

int LoraNetworkDefaultMode::_changePara(ZMsgChangeZigbeeParam* changeval)
{
    uint16_t para_crc;
    uint32_t chmask;
    
    chmask = changeval->aChMask[0]|(changeval->aChMask[1]<<8)|(changeval->aChMask[2]<<16)|(changeval->aChMask[3]<<24);

    if(0 == ((1ul<<changeval->ucCh)&chmask))
    {
    	LogErr("chmask must be include used channel\n");
    	return -1;
    }
    
    para_crc = crc16_ansi_0618(changeval->aPanid, sizeof(*changeval)-2);
    changeval->aCRC16[0] = para_crc&0xff;
    changeval->aCRC16[1] = (para_crc>>8)&0xff;

    netLoraSendMsg(false, 0xFFFF, (uint8_t *)changeval, sizeof(ZMsgChangeZigbeeParam), MSGHUB_CAT_REQ, ZMSG_CHANGE_PARA, 0, false);

    return 0;
}

int LoraNetworkDefaultMode::_setDevReset(uint8_t* mac, int delayInSec)
{
    LogDbg("Set device Reset,mac %02x%02x%02x%02x%02x%02x%02x%02x, delayInSec=%d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7], delayInSec);
    ZMsgReset msg;
    
    msg._resettime = delayInSec;/*Delay time. for lora it's unnecessary*/
    memcpy(msg._mac, mac, sizeof(msg._mac)); /*mac*/

    uint8_t msgBuf[sizeof(msg)];
    int msgLen = ZMsgResetEncode(&msg, msgBuf, sizeof(msgBuf));
    netLoraSendMsg(false, 0xFFFF, msgBuf, msgLen, MSGHUB_CAT_INFO, ZMSG_ZIGBEE_RESET, 0, false);

    return 0;
}
