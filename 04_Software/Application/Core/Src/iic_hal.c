#include "iic_hal.h"
#include "delay.h"

/**
  * @brief SDA锟斤拷锟斤拷锟斤拷模式锟斤拷锟斤拷
  * @param None
  * @retval None
  */
void SDA_Input_Mode(iic_bus_t *bus)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.Pin = bus->IIC_SDA_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = GPIO_PULLUP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);
}

/**
  * @brief SDA锟斤拷锟斤拷锟侥Ｊ斤拷锟斤拷锟�
  * @param None
  * @retval None
  */
void SDA_Output_Mode(iic_bus_t *bus)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.Pin = bus->IIC_SDA_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);
}

/**
  * @brief SDA锟斤拷锟斤拷锟揭伙拷锟轿�
  * @param val 锟斤拷锟斤拷锟斤拷锟斤拷锟�
  * @retval None
  */
void SDA_Output(iic_bus_t *bus, uint16_t val)
{
    if ( val )
    {
        bus->IIC_SDA_PORT->BSRR |= bus->IIC_SDA_PIN;
    }
    else
    {
        bus->IIC_SDA_PORT->BSRR = (uint32_t)bus->IIC_SDA_PIN << 16U;
    }
}

/**
  * @brief SCL锟斤拷锟斤拷锟揭伙拷锟轿�
  * @param val 锟斤拷锟斤拷锟斤拷锟斤拷锟�
  * @retval None
  */
void SCL_Output(iic_bus_t *bus, uint16_t val)
{
    if ( val )
    {
        bus->IIC_SCL_PORT->BSRR |= bus->IIC_SCL_PIN;
    }
    else
    {
         bus->IIC_SCL_PORT->BSRR = (uint32_t)bus->IIC_SCL_PIN << 16U;
    }
}

/**
  * @brief SDA锟斤拷锟斤拷一位
  * @param None
  * @retval GPIO锟斤拷锟斤拷一位
  */
uint8_t SDA_Input(iic_bus_t *bus)
{
	if(HAL_GPIO_ReadPin(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN) == GPIO_PIN_SET){
		return 1;
	}else{
		return 0;
	}
}

/**
  * @brief IIC锟斤拷始锟脚猴拷
  * @param None
  * @retval None
  */
void IICStart(iic_bus_t *bus)
{
    SDA_Output(bus,1);
    //delay1(DELAY_TIME);
		delay_us(2);
    SCL_Output(bus,1);
		delay_us(1);
    SDA_Output(bus,0);
		delay_us(1);
    SCL_Output(bus,0);
		delay_us(1);
}

/**
  * @brief IIC锟斤拷锟斤拷锟脚猴拷
  * @param None
  * @retval None
  */
void IICStop(iic_bus_t *bus)
{
    SCL_Output(bus,0);
		delay_us(2);
    SDA_Output(bus,0);
		delay_us(1);
    SCL_Output(bus,1);
		delay_us(1);
    SDA_Output(bus,1);
		delay_us(1);

}

/**
  * @brief IIC锟饺达拷确锟斤拷锟脚猴拷
  * @param None
  * @retval None
  */
unsigned char IICWaitAck(iic_bus_t *bus)
{
    unsigned short cErrTime = 5;
    SDA_Input_Mode(bus);
    SCL_Output(bus,1);
    while(SDA_Input(bus))
    {
        cErrTime--;
				delay_us(1);
        if (0 == cErrTime)
        {
            SDA_Output_Mode(bus);
            IICStop(bus);
            return ERROR;
        }
    }
    SDA_Output_Mode(bus);
    SCL_Output(bus,0);
		delay_us(2);
    return SUCCESS;
}

/**
  * @brief IIC锟斤拷锟斤拷确锟斤拷锟脚猴拷
  * @param None
  * @retval None
  */
void IICSendAck(iic_bus_t *bus)
{
    SDA_Output(bus,0);
		delay_us(1);
    SCL_Output(bus,1);
		delay_us(1);
    SCL_Output(bus,0);
		delay_us(1);
	
}

/**
  * @brief IIC锟斤拷锟酵凤拷确锟斤拷锟脚猴拷
  * @param None
  * @retval None
  */
void IICSendNotAck(iic_bus_t *bus)
{
    SDA_Output(bus,1);
		delay_us(1);
    SCL_Output(bus,1);
		delay_us(1);
    SCL_Output(bus,0);
		delay_us(2);

}

/**
  * @brief IIC锟斤拷锟斤拷一锟斤拷锟街斤拷
  * @param cSendByte 锟斤拷要锟斤拷锟酵碉拷锟街斤拷
  * @retval None
  */
void IICSendByte(iic_bus_t *bus,unsigned char cSendByte)
{
    unsigned char  i = 8;
    while (i--)
    {
        SCL_Output(bus,0);
        delay_us(2);
        SDA_Output(bus, cSendByte & 0x80);
				delay_us(1);
        cSendByte += cSendByte;
				delay_us(1);
        SCL_Output(bus,1);
				delay_us(1);
    }
    SCL_Output(bus,0);
		delay_us(2);
}

/**
  * @brief IIC锟斤拷锟斤拷一锟斤拷锟街斤拷
  * @param None
  * @retval 锟斤拷锟秸碉拷锟斤拷锟街斤拷
  */
unsigned char IICReceiveByte(iic_bus_t *bus)
{
    unsigned char i = 8;
    unsigned char cR_Byte = 0;
    SDA_Input_Mode(bus);
    while (i--)
    {
        cR_Byte += cR_Byte;
        SCL_Output(bus,0);
				delay_us(2);
        SCL_Output(bus,1);
				delay_us(1);
        cR_Byte |=  SDA_Input(bus);
    }
    SCL_Output(bus,0);
    SDA_Output_Mode(bus);
    return cR_Byte;
}

uint8_t IIC_Write_One_Byte(iic_bus_t *bus, uint8_t daddr,uint8_t reg,uint8_t data)
{				   	  	    																 
  IICStart(bus);  
	
	IICSendByte(bus,daddr<<1);	    
	if(IICWaitAck(bus))	//锟饺达拷应锟斤拷
	{
		IICStop(bus);		 
		return 1;		
	}
	IICSendByte(bus,reg);
	IICWaitAck(bus);	   	 										  		   
	IICSendByte(bus,data);     						   
	IICWaitAck(bus);  		    	   
  IICStop(bus);
	delay_us(1);
	return 0;
}

uint8_t IIC_Write_Multi_Byte(iic_bus_t *bus, uint8_t daddr,uint8_t reg,uint8_t length,uint8_t buff[])
{			
	unsigned char i;	
  IICStart(bus);  
	
	IICSendByte(bus,daddr<<1);	    
	if(IICWaitAck(bus))
	{
		IICStop(bus);
		return 1;
	}
	IICSendByte(bus,reg);
	IICWaitAck(bus);	
	for(i=0;i<length;i++)
	{
		IICSendByte(bus,buff[i]);     						   
		IICWaitAck(bus); 
	}		    	   
  IICStop(bus);
	delay_us(1);
	return 0;
} 

unsigned char IIC_Read_One_Byte(iic_bus_t *bus, uint8_t daddr,uint8_t reg)
{
	unsigned char dat;
	IICStart(bus);
	IICSendByte(bus,daddr<<1);
	IICWaitAck(bus);
	IICSendByte(bus,reg);
	IICWaitAck(bus);
	
	IICStart(bus);
	IICSendByte(bus,(daddr<<1)+1);
	IICWaitAck(bus);
	dat = IICReceiveByte(bus);
	IICSendNotAck(bus);
	IICStop(bus);
	return dat;
}


uint8_t IIC_Read_Multi_Byte(iic_bus_t *bus, uint8_t daddr, uint8_t reg, uint8_t length, uint8_t buff[])
{
	unsigned char i;
	IICStart(bus);
	IICSendByte(bus,daddr<<1);
	if(IICWaitAck(bus))
	{
		IICStop(bus);		 
		return 1;		
	}
	IICSendByte(bus,reg);
	IICWaitAck(bus);
	
	IICStart(bus);
	IICSendByte(bus,(daddr<<1)+1);
	IICWaitAck(bus);
	for(i=0;i<length;i++)
	{
		buff[i] = IICReceiveByte(bus);
		if(i<length-1)
		{IICSendAck(bus);}
	}
	IICSendNotAck(bus);
	IICStop(bus);
	return 0;
}


//
void IICInit(iic_bus_t *bus)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    if (bus == NULL) {
        return;
    }

    /* 软件 I2C：SDA/SCL 均开漏 + 上拉（外部 4.7k 典型；内部上拉较弱可作辅助） */
    GPIO_InitStructure.Pin = bus->IIC_SDA_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStructure.Pull = GPIO_PULLUP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->IIC_SDA_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = bus->IIC_SCL_PIN;
    HAL_GPIO_Init(bus->IIC_SCL_PORT, &GPIO_InitStructure);

    HAL_GPIO_WritePin(bus->IIC_SDA_PORT, bus->IIC_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(bus->IIC_SCL_PORT, bus->IIC_SCL_PIN, GPIO_PIN_SET);
}
