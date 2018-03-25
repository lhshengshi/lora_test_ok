#pragma once

#include <string>
#include <list>
#include <set>
#include <stdint.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include "zigbee_api_ex.h"
#include "json/json.h"
#include "server.h"
#include "NetworkDefaultMode.h"
#include "zigbee_codec.h"

class LoraNetworkDefaultMode : public NetworkDefaultMode {
public:
    enum LoraNetworkMode {
        NETWORK_MODE_SAFE = 0,
        NETWORK_MODE_DEFAULT,
        NETWORK_MODE_FORCE_SAFE,
        NETWORK_MODE_MAX
    };

    enum InternalStatus {
        STATUS_SAFE,
        STATUS_DEFAULT,
        STATUS_CHANGE_PARAM,
        STATUS_WAIT_FORCE_SAFE_CMD
    };

    static LoraNetworkDefaultMode& getInstance() {
        return _inst;
    }

protected:
    // command from app
    LoraNetworkMode _discoveryModeCmd;
    bool _cmdWithDevFilter; // only used when change to safe mode
    std::set<std::string> _cmdFilterDevList; // only used when change to safe mode
    std::string _appCmd;

    // status
    // internal status
    InternalStatus _internalStatus;
    uint64_t _discoverModeStartInMs;

    // for change params
    int _changeParamRetries;
    int _changeParamDevIdx;
    uint64_t _changeParamDevInMs;

    const char* _discoveryResult;
public:
    LoraNetworkDefaultMode();
    ~LoraNetworkDefaultMode();

    void checkNetworkMode(tSERVER *tpServer);
    InternalStatus getInternalStatus() const {
        return _internalStatus;
    }

    bool enterDefaultMode(struct tSERVER_S *tpServer);
    bool leaveDefaultMode(struct tSERVER_S *tpServer);

    int processAuthResp(uint8_t* mac, uint16_t type, uint16_t subType, uint8_t rssi);
    bool processChangeParaAck(struct tSERVER_S* pServer, uint8_t* mac, uint16_t receive_crc, int32_t len);

    void setNetworkMode(LoraNetworkMode mode, const std::string& appCmd);
    void setNetworkMode(LoraNetworkMode mode, const std::set<std::string>& deviceList, const std::string& appCmd);

    bool getDeviceList(Json::Value& outJson);

    void broadcastStatusToApp(const std::string& status);

protected:
    void _startToChangeParams();
    bool _changeOneDevicePara();
    bool _checkAllChangeParaAckReceived();

    void _clearDiscoveredDevices();
    int _changePara(ZMsgChangeZigbeeParam* changeval);
    int _setDevReset(uint8_t* mac, int delayInSec);

    bool _writeEnterLoraDefaultConf(const struct setpara& para);
    bool _writeExitLoraDefaultConf();

    struct LoraDiscoveredDevice : public DiscoveredDevice {
        uint8_t mac[8];
        uint8_t rssi;
        bool reset_sent;
    };

    void _notifyDeviceToAllApp(const LoraDiscoveredDevice& dd);

    std::vector<LoraDiscoveredDevice> _discoveredList2;

    static LoraNetworkDefaultMode _inst;


};

