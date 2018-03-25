/**
  ******************************************************************************
  * @file    main.c
  * @author  zjj
  * @version V1.0
  * @date    2014-10-22
  * @brief   串口中断接收测试
  ******************************************************************************
  * @attention
  *
  * 实验平台: STM32 开发板
  * 论坛    :
  * 淘宝    :
  *
  ******************************************************************************
  */ 
 
#include "sys_config.h"

#define BUFFER_SIZE     30                          // Define the payload size here

static uint16_t BufferSize = BUFFER_SIZE;			// RF buffer size
static uint8_t  Buffer[BUFFER_SIZE];				// RF buffer

static uint8_t EnableMaster = true; 				// Master/Slave selection

tRadioDriver *Radio = NULL;

const uint8_t PingMsg[] = "PING";
const uint8_t PongMsg[] = "PONG";
const uint8_t MY_TEST_Msg[] = "SX1278_TEST";


void OnMaster( void );
void OnSlave( void );
/*
 * Manages the master operation
 */
void OnMaster( void )
{
    uint8_t i;
    
    switch( Radio->Process( ) )
    {
    case RF_RX_TIMEOUT:
        // Send the next PING frame
        Buffer[0] = 'P';
        Buffer[1] = 'I';
        Buffer[2] = 'N';
        Buffer[3] = 'G';
        for( i = 4; i < BufferSize; i++ )
        {
            Buffer[i] = i - 4;
        }
        Radio->SetTxPacket( Buffer, BufferSize );   //RFLR_STATE_TX_INIT
        break;
    case RF_RX_DONE:
        Radio->GetRxPacket( Buffer, ( uint16_t* )&BufferSize );

//		for(i=0;i<BufferSize;i++)
//		{
//			USART_putchar(USART1,Buffer[i]);
//		}
//		printf("\n");
    
		if( BufferSize > 0 )
        {
            if( strncmp( ( const char* )Buffer, ( const char* )PongMsg, 4 ) == 0 )
            {
                // Indicates on a LED that the received frame is a PONG
//                LedToggle( LED_GREEN );
				LED0_TOGGLE;

                // Send the next PING frame            
                Buffer[0] = 'P';
                Buffer[1] = 'I';
                Buffer[2] = 'N';
                Buffer[3] = 'G';
                // We fill the buffer with numbers for the payload 
                for( i = 4; i < BufferSize; i++ )
                {
                    Buffer[i] = i - 4;
                }
                Radio->SetTxPacket( Buffer, BufferSize );    //RFLR_STATE_TX_INIT
            }
            else if( strncmp( ( const char* )Buffer, ( const char* )PingMsg, 4 ) == 0 )
            { // A master already exists then become a slave
                EnableMaster = false;
//                LedOff( LED_RED );
				LED0_OFF;
            }
        }            
        break;
    case RF_TX_DONE:
        // Indicates on a LED that we have sent a PING
//        LedToggle( LED_RED );
		LED0_TOGGLE;
        Radio->StartRx( );   //RFLR_STATE_RX_INIT
        break;
    default:
        break;
    }
}

/*
 * Manages the slave operation
 */
void OnSlave( void )
{
    uint8_t i;

    switch( Radio->Process( ) )
    {
    case RF_RX_DONE:
        Radio->GetRxPacket( Buffer, ( uint16_t* )&BufferSize );
    
        if( BufferSize > 0 )
        {
            if( strncmp( ( const char* )Buffer, ( const char* )PingMsg, 4 ) == 0 )
            {
                // Indicates on a LED that the received frame is a PING
//                LedToggle( LED_GREEN );
				LED0_TOGGLE;

               // Send the reply to the PONG string
                Buffer[0] = 'P';
                Buffer[1] = 'O';
                Buffer[2] = 'N';
                Buffer[3] = 'G';
                // We fill the buffer with numbers for the payload 
                for( i = 4; i < BufferSize; i++ )
                {
                    Buffer[i] = i - 4;
                }

                Radio->SetTxPacket( Buffer, BufferSize );      //RFLR_STATE_TX_INIT
            }
        }
        break;
    case RF_TX_DONE:
        // Indicates on a LED that we have sent a PONG
//        LedToggle( LED_RED );
		LED0_TOGGLE;
        Radio->StartRx( );   //RFLR_STATE_RX_INIT
        break;
    default:
        break;
    }
}


#define SX1278_RX
//#define SX1278_TX

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
	uint8_t i;
	//stm32 config
	sys_Configuration();
	
	BoardInit( );
    
    Radio = RadioDriverInit( );
    
    Radio->Init( );
	
#if defined (SX1278_RX)
    Radio->StartRx( );   //RFLR_STATE_RX_INIT
#elif defined (SX1278_TX)
	Radio->SetTxPacket( MY_TEST_Msg, 11 );
#endif
	
	while(1)
	{
#if defined (SX1278_RX)
		while( Radio->Process( ) == RF_RX_DONE)
		{
			Radio->GetRxPacket( Buffer, ( uint16_t* )&BufferSize );
			if( strncmp( ( const char* )Buffer, ( const char* )MY_TEST_Msg, BufferSize ) == 0 )
			{
				for(i=0;i < BufferSize;i++)
				{
					USART_putchar(USART1,Buffer[i]);
				}
				printf("\n");
				LED0_TOGGLE;
				
//				LED0_ON;
//				Soft_delay_ms(100);
//				LED0_OFF;
//				Soft_delay_ms(100);
				for(i=0;i<BUFFER_SIZE;i++)
					Buffer[i] = 0;
			}
			Radio->StartRx( );
		}
#elif defined (SX1278_TX)
		while(Radio->Process( ) == RF_TX_DONE)
		{
			printf("RF_LoRa_TX_OK! \n");
			LED0_TOGGLE;
			Soft_delay_ms(500);
			Radio->SetTxPacket( MY_TEST_Msg, 11 );   //RFLR_STATE_TX_INIT
		}		
#endif
		
	}
}



/*********************************************END OF FILE**********************/
