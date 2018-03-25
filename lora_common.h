//
// Created by Gavin Xiao on 03/12/2016.
//

#ifndef SERVER_LORA_H
#define SERVER_LORA_H

#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack (1)

#define VERSION_LENS 3

/***************************************/
/***********LORA PARAMETER*************/
typedef struct lorapara {
	uint8_t freq[4];/*hz*/
	uint8_t signal_bw;                   // LORA [0: 7.8 kHz, 1: 10.4 kHz, 2: 15.6 kHz, 3: 20.8 kHz, 4: 31.2 kHz,
	// 5: 41.6 kHz, 6: 62.5 kHz, 7: 125 kHz, 8: 250 kHz, 9: 500 kHz, other: Reserved]
	uint8_t spreading_factor;            // LORA [6: 64, 7: 128, 8: 256, 9: 512, 10: 1024, 11: 2048, 12: 4096  chips]
	uint8_t error_coding;                // LORA [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
	uint16_t bps;
} tLoraPara;

typedef struct lorapara_lowrev {
	struct lorapara rfpara_lowpower;
	uint8_t waketimes;/*50ms,100ms,200ms,400ms,600ms,1s,1.5s,2s,2.5s,4s,5s*/
} tLorapara_Lowrev;

typedef struct setpara {
	uint8_t version[VERSION_LENS];/*only read,set will be ignored,this must be first*/
	uint8_t PA_BOOST_select:1;/*hardware */
	uint8_t loraTxPower:4;/*hardware */
	uint8_t spare: 3;
	uint8_t default_working_mode;/*0:full_work,1:lowpower receive,3:Lora RF_OFF*/
	uint8_t broadcast_filter_onoff;/*0:filter not work,ignore all msg,1:filter work*/
	uint8_t nodeid_filter_onoff;/*0:filter not work,receive all msg,1:filter work*/
	uint16_t netid;
	uint16_t nodeid;/* nodeid:0x0000-0x0fff groupid 0xf000~0xf0ff*/
	struct lorapara rfpara_fullwork;/*normal transmit and receive parameter*/
	struct lorapara_lowrev rfpara_lowpower_sub;/*lowpower wakeup and receive parameter*/
	//uint8_t checksum;/*start from PA_BOOST_select, xor*/
} tSetPara;

typedef struct groupid_para {
	uint16_t groupid_lens;/*max 255,just uesed for check par*/
	uint8_t groupid[32];/*bit mask*/
} tGroupid_Para;

/***********LORA PARAMETER*************/
enum LORA_WORK_MODE {
	FULL_WORK = 0x00,
	LOW_POWER,
	RF_OFF
};

typedef struct Lora_send_para {
	uint8_t tx_power : 4;/*0-15db*/
	uint8_t tx_priority : 3;/*7-0 ,0:no collision detection*/
	uint8_t tx_mode : 1;/*0:full work 1:wakeup mode*/
} tLora_send_para;

typedef struct Lora_TX_tail {
	uint8_t tail_tag;/*0xff*/
	uint8_t AF_Sequence;
	struct Lora_send_para send_para;
} tLora_TX_tail;

typedef struct Lora_RX_tail {
	uint8_t tail_tag;/*0xff*/
	uint8_t AF_Sequence;
	uint8_t AF_Rssi;
	uint16_t src_addr;
	int8_t SNR;/*-127 - 128*/
} tLora_RX_tail;


typedef enum LoraParaSetCmdType {
	LOCAL_READ_PARA_CMD = 0xe1,/*read lora module para*/
	LOCAL_WRITE_PARA_CMD = 0xe2, /*set lora module para*/
	LOCAL_WRITE_TEMP_PARA_CMD = 0xe3, /*set a temporary lora module para,will not save to flash*/
	LOCAL_EXIT_TEMP_PARA_CMD = 0xe4, /*exit temporary lora module para*/
	LOCAL_READ_GROUPID_CMD = 0xe5, /*set lora module groupid*/
	LOCAL_WRITE_GROUPID_CMD = 0xe6, /*set lora module groupid*/
	LOCAL_LORA_RF_SET_CMD = 0xe7,/*payload:"cmd"+"seq"+"tLORARFSET"*/
	LOCAL_READ_STATUS_CMD = 0xe8, /*read lora module status*/
} tLoraParaSetCmdType;

/***************SET*****************/

//LOCAL_LORA_RF_SET_CMD
typedef struct LORARFSET {
	uint8_t LoraRfOnOff;/*1:on 0:off*/
} tLORARFSET;

typedef struct LoraReadParaHead {
	uint8_t LoraParaSetCmdType; // tLoraParaSetCmdType
	uint8_t ucSeq;
	uint8_t _frameResult; // LoraOperateResult
} tLoraReadParaHead;

typedef struct LoraSetParaHead {
	uint8_t LoraParaSetCmdType; // tLoraParaSetCmdType
	uint8_t ucSeq;
} tLoraSetParaHead;


/*
USER GUIDE

SET:
typedef struct PARA_SET_MSG
{
	tLoraSetParaHead head;
	tSetPara||tGroupid_Para;
}tPARA_SET_MSG;

READ:
typedef struct PARA_READ_MSG
{
	tLoraReadParaHead head;
	tSetPara||tGroupid_Para;
}tPARA_READ_MSG;

*/

/***************MSG*****************/
enum LoraFrameType {
	LORA_FRAME_CONFIRMED_DATA = 0x01,
	LORA_FRAME_CONFIRMED_DATA_LINK_ACK = 0x02, // mac ack from peer endpoint
	LORA_FRAME_MULTICAST_DATA = 0x03,
	LORA_FRAME_MULTICAST_DATA_LOCAL_ACK = 0x04,
	LORA_FRAME_UNCONFIRMED_DATA = 0x05,
	LORA_FRAME_UNCONFIRMED_DATA_LOCAL_ACK = 0x06,

	LORA_FRAME_SETTING = 0x11,
	LORA_FRAME_SETTING_ACK = 0x12,
	LORA_FRAME_QUERY_REQ = 0x13,
	LORA_FRAME_QUERY_RESP = 0x14,
	LORA_FRAME_NOTIFY = 0X15
};


//*********************RF MSG
enum LoraFrameResult {
	LORA_FRAME_RESULT_SUCC = 0,
	LORA_FRAME_RESULT_BUSY = 1,
	LORA_FRAME_RESULT_NOACK = 2,
};

// LORA_FRAME_CONFIRMED_DATA_ACK
typedef struct LoraConfirmedDataAck {
	uint8_t _seq;
	uint8_t _frameResult; // LoraFrameResult
	uint16_t _destAddr;
	uint8_t _rssiPeer;
	uint8_t _rssiLocal;
} tLoraConfirmedDataAck;

// LoraUnConfirmedDataAck/ multicast data ack
typedef struct LoraUnConfirmedDataAck {
	uint8_t _seq;
	uint8_t _frameResult; // LoraFrameResult
	uint16_t _destAddr;
} tLoraUnConfirmedDataAck;


//************** LORA_FRAME_SETTING_ACK
enum LoraOperateResult {
	LORA_OPERATE_RESULT_SUCC = 0,
	LORA_OPERATE_RESULT_FAILED = 1,
};

typedef struct LoraModuleSimpleRsp {/*setting*/
	uint8_t _cmd;
	uint8_t _seq;
	uint8_t _frameResult; // LoraOperateResult
} tLoraModuleSimpleRsp;

//*************LORA_FRAME_NOTIFY
typedef struct LoraNotifyMsg {
	uint8_t _notifyType; /* LoraNotifyType*/
	uint8_t _notifyValue;/*maybe use*/
} tLoraNotifyMsg;

enum LoraNotifyType {
	LORA_ERROR_TYPE_RESERVED = 0X00,
	LORA_ERROR_TYPE_RESET = 0x01,
	LORA_ERROR_TYPE_UART_MSG_ERROR = 0x02,
	LORA_ERROR_TYPE_UART_OVR_ERROR = 0x03,/*over run*/
	LORA_ERROR_TYPE_UART_TX_LOCK = 0x04,/*send error*/
	LORA_ERROR_TYPE_TX_LOCK = 0x05,
	LORA_ERROR_TYPE_MODECHANGE = 0x06,
	LORA_ERROR_TYPE_ERROR = 0x07
};

#pragma pack()

#define LORA_SND_TEST_FREQ   	780000000
#define LORA_REC_TEST_FREQ   	(780000000+300000)
#define LORA_DEFAULT_FREQ		779078125
#define LORA_DEFAULT_SF			7
#define LORA_DEFAULT_BW			7
#define LORA_DEFAULT_CR			2
#define LORA_DEFAULT_BPS		4557

/*default parameter*/
/*
para->PA_BOOST_select = 1;
para->default_working_mode = 0;
para->broadcast_filter_onoff = 1;
para->nodeid_filter_onoff= 1;
para->netid = 0xffff;
para->nodeid = XXXX;

para->rfpara_fullwork.freq[0] = (LORA_DEFAULT_FREQ>>0)&0xff;
para->rfpara_fullwork.freq[1] = (LORA_DEFAULT_FREQ>>8)&0xff;
para->rfpara_fullwork.freq[2] = (LORA_DEFAULT_FREQ>>16)&0xff;
para->rfpara_fullwork.freq[3] = (LORA_DEFAULT_FREQ>>24)&0xff;
para->rfpara_fullwork.signal_bw = LORA_DEFAULT_BW;
para->rfpara_fullwork.spreading_factor = LORA_DEFAULT_SF;
para->rfpara_fullwork.error_coding = LORA_DEFAULT_CR;
para->rfpara_fullwork.bps = LORA_DEFAULT_BPS;

para->rfpara_lowpower_sub.waketimes = 3;
para->rfpara_lowpower_sub.rfpara_lowpower.freq[0] = (LORA_DEFAULT_FREQ>>0)&0xff;
para->rfpara_lowpower_sub.rfpara_lowpower.freq[1] = (LORA_DEFAULT_FREQ>>8)&0xff;
para->rfpara_lowpower_sub.rfpara_lowpower.freq[2] = (LORA_DEFAULT_FREQ>>16)&0xff;
para->rfpara_lowpower_sub.rfpara_lowpower.freq[3] = (LORA_DEFAULT_FREQ>>24)&0xff;
para->rfpara_lowpower_sub.rfpara_lowpower.signal_bw = LORA_DEFAULT_BW;
para->rfpara_lowpower_sub.rfpara_lowpower.spreading_factor = LORA_DEFAULT_SF;
para->rfpara_lowpower_sub.rfpara_lowpower.error_coding = LORA_DEFAULT_CR;
para->rfpara_lowpower_sub.rfpara_lowpower.bps = LORA_DEFAULT_BPS;
*/

#ifdef __cplusplus
}
#endif

#endif //SERVER_LORA_H
