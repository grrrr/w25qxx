
#include "w25qxx.h"
#include "w25qxxConf.h"

#if (_W25QXX_DEBUG==1)
#include <stdio.h>
#endif

#define W25QXX_DUMMY_BYTE         0xA5

w25qxx_t	w25qxx;

#if (_W25QXX_USE_FREERTOS==1)
#define	W25qxx_Delay(delay)		osDelay(delay)
#include "cmsis_os.h"
#else
#define	W25qxx_Delay(delay)		HAL_Delay(delay)
#endif

#define W25qxx_Wait() __NOP()  /* wait only a little */

#define W25qxx_Lock() (w25qxx.Lock = 1)
#define W25qxx_Unlock() (w25qxx.Lock = 0)
#define W25qxx_WaitAndLock() do { if(!w25qxx.Lock) { W25qxx_Lock(); break; } else W25qxx_Wait(); } while(1)
#define W25qxx_IsLocked() (w25qxx.Lock)

// re-#define often used HAL function for speedup (esp. with constant PinState)
#define HAL_GPIO_WritePin(GPIOx,GPIO_Pin,PinState) \
((GPIOx)->BSRR = ((PinState) != GPIO_PIN_RESET)?(GPIO_Pin):((uint32_t)(GPIO_Pin) << 16U))

#define W25qxxSet() HAL_GPIO_WritePin(_W25QXX_CS_GPIO,_W25QXX_CS_PIN,GPIO_PIN_RESET)
#define W25qxxUnset() HAL_GPIO_WritePin(_W25QXX_CS_GPIO,_W25QXX_CS_PIN,GPIO_PIN_SET)

//###################################################################################################################
static int W25qxx_AddressCmds(uint8_t *cmdbuf, uint32_t SectorAddr)
{
  uint8_t *cmdp = cmdbuf;
  if(w25qxx.ID >= W25Q256)
    (*cmdp++) = (SectorAddr & 0xFF000000) >> 24;
  (*cmdp++) = (SectorAddr & 0xFF0000) >> 16;
  (*cmdp++) = (SectorAddr & 0xFF00) >> 8;
  (*cmdp++) = SectorAddr & 0xFF;
  return cmdp-cmdbuf;
}
//###################################################################################################################
static uint8_t W25qxx_Spi(uint8_t Data)
{
  uint8_t	ret;
  HAL_SPI_TransmitReceive(&_W25QXX_SPI,&Data,&ret,1,100);
  return ret;
}
//###################################################################################################################
static void W25qxx_SpiTx(uint8_t *Data, uint16_t size, uint32_t timeout)
{
  HAL_SPI_Transmit(&_W25QXX_SPI,Data,size,timeout);
}
//###################################################################################################################
static void W25qxx_SpiRx(uint8_t *Data, uint16_t size, uint32_t timeout)
{
  HAL_SPI_Receive(&_W25QXX_SPI,Data,size,timeout);
}
//###################################################################################################################
uint32_t W25qxx_ReadID(void)
{
  uint32_t Temp = 0, Temp0 = 0, Temp1 = 0, Temp2 = 0;
  W25qxxSet();
  W25qxx_Spi(0x9F);
  Temp0 = W25qxx_Spi(W25QXX_DUMMY_BYTE);
  Temp1 = W25qxx_Spi(W25QXX_DUMMY_BYTE);
  Temp2 = W25qxx_Spi(W25QXX_DUMMY_BYTE);
  W25qxxUnset();
  Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;
  return Temp;
}
//###################################################################################################################
void W25qxx_ReadUniqID(void)
{
  W25qxxSet();
  W25qxx_Spi(0x4B);
      for(uint8_t	i=0;i<4;i++)
	      W25qxx_Spi(W25QXX_DUMMY_BYTE);
      for(uint8_t	i=0;i<8;i++)
	      w25qxx.UniqID[i] = W25qxx_Spi(W25QXX_DUMMY_BYTE);
  W25qxxUnset();
}
//###################################################################################################################
static void W25qxx_WriteEnable(void)
{
  W25qxxSet();
  W25qxx_Spi(0x06);
  W25qxxUnset();
	W25qxx_Wait();
}
//###################################################################################################################
static void W25qxx_WriteDisable(void)
{
  W25qxxSet();
  W25qxx_Spi(0x04);
  W25qxxUnset();
	W25qxx_Wait();
}
//###################################################################################################################
static uint8_t W25qxx_ReadStatusRegister(uint8_t	SelectStatusRegister_1_2_3)
{
	uint8_t	status=0;
  W25qxxSet();
	if(SelectStatusRegister_1_2_3==1)
	{
		W25qxx_Spi(0x05);
		status=W25qxx_Spi(W25QXX_DUMMY_BYTE);	
		w25qxx.StatusRegister1 = status;
	}
	else if(SelectStatusRegister_1_2_3==2)
	{
		W25qxx_Spi(0x35);
		status=W25qxx_Spi(W25QXX_DUMMY_BYTE);	
		w25qxx.StatusRegister2 = status;
	}
	else
	{
		W25qxx_Spi(0x15);
		status=W25qxx_Spi(W25QXX_DUMMY_BYTE);	
		w25qxx.StatusRegister3 = status;
	}	
  W25qxxUnset();
	return status;
}
//###################################################################################################################
static void W25qxx_WriteStatusRegister(uint8_t	SelectStatusRegister_1_2_3,uint8_t Data)
{
  W25qxxSet();
	if(SelectStatusRegister_1_2_3==1)
	{
		W25qxx_Spi(0x01);
		w25qxx.StatusRegister1 = Data;
	}
	else if(SelectStatusRegister_1_2_3==2)
	{
		W25qxx_Spi(0x31);
		w25qxx.StatusRegister2 = Data;
	}
	else
	{
		W25qxx_Spi(0x11);
		w25qxx.StatusRegister3 = Data;
	}
	W25qxx_Spi(Data);
  W25qxxUnset();
}
//###################################################################################################################
static bool W25qxx_CheckForWriteEnd(void)
{
  W25qxx_Wait();
  W25qxxSet();
  // Read status register 1
  W25qxx_Spi(0x05);
  w25qxx.StatusRegister1 = W25qxx_Spi(W25QXX_DUMMY_BYTE);
  W25qxxUnset();
  // Bit 1 set means busy
  bool rdy = !(w25qxx.StatusRegister1 & 0x01);
  return rdy;
}
//###################################################################################################################
static void W25qxx_WaitForWriteEnd(void)
{
	W25qxx_Wait();
	W25qxxSet();
	W25qxx_Spi(0x05);
  do
  {
    w25qxx.StatusRegister1 = W25qxx_Spi(W25QXX_DUMMY_BYTE);
		W25qxx_Wait();
  }
  while (w25qxx.StatusRegister1 & 0x01);
 W25qxxUnset();
}
//###################################################################################################################
#if (_W25QXX_DEBUG==1)
  static uint32_t StartTime;
#endif

bool W25qxx_CheckForWriteEndAndUnlock()
{
  bool rdy = W25qxx_CheckForWriteEnd();
  if(rdy) {
#if (_W25QXX_DEBUG==1)
    printf("w25qxx write process done after %d ms\r\n",HAL_GetTick()-StartTime);
#endif
    W25qxx_Unlock();
  }
  return rdy;
}
//###################################################################################################################
bool	W25qxx_Init(void)
{
	w25qxx.Lock=1;	
	while(HAL_GetTick()<100)
		W25qxx_Wait();
  W25qxxUnset();
  W25qxx_Delay(100);
	uint32_t	id;
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx Init Begin...\r\n");
	#endif
	id=W25qxx_ReadID();
	
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx ID:0x%X\r\n",id);
	#endif
	switch(id&0x0000FFFF)
	{
		case 0x401A:	// 	w25q512
			w25qxx.ID=W25Q512;
			w25qxx.BlockCount=1024;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q512\r\n");
			#endif
		break;
		case 0x4019:	// 	w25q256
			w25qxx.ID=W25Q256;
			w25qxx.BlockCount=512;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q256\r\n");
			#endif
		break;
		case 0x4018:	// 	w25q128
			w25qxx.ID=W25Q128;
			w25qxx.BlockCount=256;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q128\r\n");
			#endif
		break;
		case 0x4017:	//	w25q64
			w25qxx.ID=W25Q64;
			w25qxx.BlockCount=128;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q64\r\n");
			#endif
		break;
		case 0x4016:	//	w25q32
			w25qxx.ID=W25Q32;
			w25qxx.BlockCount=64;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q32\r\n");
			#endif
		break;
		case 0x4015:	//	w25q16
			w25qxx.ID=W25Q16;
			w25qxx.BlockCount=32;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q16\r\n");
			#endif
		break;
		case 0x4014:	//	w25q80
			w25qxx.ID=W25Q80;
			w25qxx.BlockCount=16;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q80\r\n");
			#endif
		break;
		case 0x4013:	//	w25q40
			w25qxx.ID=W25Q40;
			w25qxx.BlockCount=8;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q40\r\n");
			#endif
		break;
		case 0x4012:	//	w25q20
			w25qxx.ID=W25Q20;
			w25qxx.BlockCount=4;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q20\r\n");
			#endif
		break;
		case 0x4011:	//	w25q10
			w25qxx.ID=W25Q10;
			w25qxx.BlockCount=2;
			#if (_W25QXX_DEBUG==1)
			printf("w25qxx Chip: w25q10\r\n");
			#endif
		break;
		default:
				#if (_W25QXX_DEBUG==1)
				printf("w25qxx Unknown ID\r\n");
				#endif
			W25qxx_Unlock();
			return false;
				
	}		
	w25qxx.PageSize=256;
	w25qxx.SectorSize=0x1000;
	w25qxx.SectorCount=w25qxx.BlockCount*16;
	w25qxx.PageCount=(w25qxx.SectorCount*w25qxx.SectorSize)/w25qxx.PageSize;
	w25qxx.BlockSize=w25qxx.SectorSize*16;
	w25qxx.CapacityInKiloByte=(w25qxx.SectorCount*w25qxx.SectorSize)/1024;
	W25qxx_ReadUniqID();
	W25qxx_ReadStatusRegister(1);
	W25qxx_ReadStatusRegister(2);
	W25qxx_ReadStatusRegister(3);
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx Page Size: %d Bytes\r\n",w25qxx.PageSize);
	printf("w25qxx Page Count: %d\r\n",w25qxx.PageCount);
	printf("w25qxx Sector Size: %d Bytes\r\n",w25qxx.SectorSize);
	printf("w25qxx Sector Count: %d\r\n",w25qxx.SectorCount);
	printf("w25qxx Block Size: %d Bytes\r\n",w25qxx.BlockSize);
	printf("w25qxx Block Count: %d\r\n",w25qxx.BlockCount);
	printf("w25qxx Capacity: %d KiloBytes\r\n",w25qxx.CapacityInKiloByte);
	printf("w25qxx Init Done\r\n");
	#endif
  W25qxx_Unlock();
	return true;
}	
//###################################################################################################################
bool W25qxx_EraseChip_Initiate()
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
#if (_W25QXX_DEBUG==1)
  StartTime=HAL_GetTick();
  printf("w25qxx EraseChip Begin...\r\n");
#endif
  W25qxx_WriteEnable();
  W25qxxSet();
  W25qxx_Spi(0xC7);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void	W25qxx_EraseChip()
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_EraseChip_Initiate()) {}
  // Block until ready
  while(!W25qxx_CheckForWriteEndAndUnlock()) {}
}
//###################################################################################################################
bool W25qxx_EraseSector_Initiate(uint32_t SectorAddr)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
#if (_W25QXX_DEBUG==1)
  StartTime=HAL_GetTick();
  printf("w25qxx EraseSector %d Begin...\r\n",SectorAddr);
#endif
  W25qxx_WaitForWriteEnd();
  uint32_t Addr = SectorAddr * w25qxx.SectorSize;
  W25qxx_WriteEnable();
  W25qxxSet();
  uint8_t cmd[16], *cmdp = cmd;
  (*cmdp++) = (0x20);
  cmdp += W25qxx_AddressCmds(cmdp, Addr);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_EraseSector(uint32_t SectorAddr)
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_EraseSector_Initiate(SectorAddr)) {}
  // Block until ready
  while(!W25qxx_CheckForWriteEndAndUnlock()) {}
}
//###################################################################################################################
bool W25qxx_EraseBlock_Initiate(uint32_t BlockAddr)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
#if (_W25QXX_DEBUG==1)
  printf("w25qxx EraseBlock %d Begin...\r\n",BlockAddr);
  W25qxx_Delay(100);
  StartTime=HAL_GetTick();
#endif
  W25qxx_WaitForWriteEnd();
  uint8_t cmd[16], *cmdp = cmd;
  uint32_t Addr = BlockAddr * w25qxx.SectorSize*16;
  W25qxx_WriteEnable();
  W25qxxSet();
  (*cmdp++) = (0xD8);
  cmdp += W25qxx_AddressCmds(cmdp, Addr);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_EraseBlock(uint32_t BlockAddr)
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_EraseBlock_Initiate(BlockAddr)) {}
  // Block until ready
  while(!W25qxx_CheckForWriteEndAndUnlock()) {}
}
//###################################################################################################################
bool W25qxx_Erase32kBlock_Initiate(uint32_t BlockAddr)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
#if (_W25QXX_DEBUG==1)
  printf("w25qxx EraseBlock %d Begin...\r\n",BlockAddr);
  W25qxx_Delay(100);
  StartTime=HAL_GetTick();
#endif
  W25qxx_WaitForWriteEnd();
  uint8_t cmd[16], *cmdp = cmd;
  uint32_t Addr = BlockAddr * w25qxx.SectorSize*8;
  W25qxx_WriteEnable();
  W25qxxSet();
  (*cmdp++) = (0x52);
  cmdp += W25qxx_AddressCmds(cmdp, Addr);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_Erase32kBlock(uint32_t BlockAddr)
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_Erase32kBlock_Initiate(BlockAddr)) {}
  // Block until ready
  while(!W25qxx_CheckForWriteEndAndUnlock()) {}
}
//###################################################################################################################
uint32_t	W25qxx_PageToSector(uint32_t	PageAddress)
{
	return ((PageAddress*w25qxx.PageSize)/w25qxx.SectorSize);
}
//###################################################################################################################
uint32_t	W25qxx_PageToBlock(uint32_t	PageAddress)
{
	return ((PageAddress*w25qxx.PageSize)/w25qxx.BlockSize);
}
//###################################################################################################################
uint32_t	W25qxx_SectorToBlock(uint32_t	SectorAddress)
{
	return ((SectorAddress*w25qxx.SectorSize)/w25qxx.BlockSize);
}
//###################################################################################################################
uint32_t	W25qxx_SectorToPage(uint32_t	SectorAddress)
{
	return (SectorAddress*w25qxx.SectorSize)/w25qxx.PageSize;
}
//###################################################################################################################
uint32_t	W25qxx_BlockToPage(uint32_t	BlockAddress)
{
	return (BlockAddress*w25qxx.BlockSize)/w25qxx.PageSize;
}
//###################################################################################################################
bool 	W25qxx_IsEmptyPage(uint32_t Page_Address,uint32_t OffsetInByte,uint32_t NumByteToCheck_up_to_PageSize)
{
  W25qxx_WaitAndLock();
	if(((NumByteToCheck_up_to_PageSize+OffsetInByte)>w25qxx.PageSize)||(NumByteToCheck_up_to_PageSize==0))
		NumByteToCheck_up_to_PageSize=w25qxx.PageSize-OffsetInByte;
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckPage:%d, Offset:%d, Bytes:%d begin...\r\n",Page_Address,OffsetInByte,NumByteToCheck_up_to_PageSize);
	W25qxx_Delay(100);
	StartTime=HAL_GetTick();
	#endif		
	uint8_t	pBuffer[32];
	uint32_t	WorkAddress;
	uint32_t	i;
	for(i=OffsetInByte; i<w25qxx.PageSize; i+=sizeof(pBuffer))
	{
	    uint8_t cmd[16], *cmdp = cmd;
		W25qxxSet();
		WorkAddress=(i+Page_Address*w25qxx.PageSize);
		(*cmdp++) = (0x0B);
		  cmdp += W25qxx_AddressCmds(cmdp, WorkAddress);
		(*cmdp++) = (0);
		  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
		  W25qxx_SpiRx(pBuffer,sizeof(pBuffer), 100);
		W25qxxUnset();
		for(uint8_t x=0;x<sizeof(pBuffer);x++)
		{
			if(pBuffer[x]!=0xFF)
				goto NOT_EMPTY;		
		}			
	}	
	if((w25qxx.PageSize+OffsetInByte)%sizeof(pBuffer)!=0)
	{
		i-=sizeof(pBuffer);
		for( ; i<w25qxx.PageSize; i++)
		{
		    uint8_t cmd[16], *cmdp = cmd;
			W25qxxSet();
			WorkAddress=(i+Page_Address*w25qxx.PageSize);
			(*cmdp++) = (0x0B);
			  cmdp += W25qxx_AddressCmds(cmdp, WorkAddress);
			(*cmdp++) = (0);
			  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
			  W25qxx_SpiRx(pBuffer, 1, 100);
			W25qxxUnset();
			if(pBuffer[0]!=0xFF)
				goto NOT_EMPTY;
		}
	}	
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckPage is Empty in %d ms\r\n",HAL_GetTick()-StartTime);
	W25qxx_Delay(100);
	#endif	
  W25qxx_Unlock();
	return true;	
	NOT_EMPTY:
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckPage is Not Empty in %d ms\r\n",HAL_GetTick()-StartTime);
	W25qxx_Delay(100);
	#endif	
  W25qxx_Unlock();
  return false;
}
//###################################################################################################################
bool 	W25qxx_IsEmptySector(uint32_t Sector_Address,uint32_t OffsetInByte,uint32_t NumByteToCheck_up_to_SectorSize)
{
  W25qxx_WaitAndLock();
	if((NumByteToCheck_up_to_SectorSize>w25qxx.SectorSize)||(NumByteToCheck_up_to_SectorSize==0))
		NumByteToCheck_up_to_SectorSize=w25qxx.SectorSize;
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckSector:%d, Offset:%d, Bytes:%d begin...\r\n",Sector_Address,OffsetInByte,NumByteToCheck_up_to_SectorSize);
	W25qxx_Delay(100);
	StartTime=HAL_GetTick();
	#endif		
	uint8_t	pBuffer[32];
	uint32_t	WorkAddress;
	uint32_t	i;
	for(i=OffsetInByte; i<w25qxx.SectorSize; i+=sizeof(pBuffer))
	{
	    uint8_t cmd[16], *cmdp = cmd;
		W25qxxSet();
		WorkAddress=(i+Sector_Address*w25qxx.SectorSize);
		(*cmdp++) = (0x0B);
		  cmdp += W25qxx_AddressCmds(cmdp, WorkAddress);
		(*cmdp++) = (0);
		  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
		  W25qxx_SpiRx(pBuffer, sizeof(pBuffer), 100);
		W25qxxUnset();
		for(uint8_t x=0;x<sizeof(pBuffer);x++)
		{
			if(pBuffer[x]!=0xFF)
				goto NOT_EMPTY;		
		}			
	}	
	if((w25qxx.SectorSize+OffsetInByte)%sizeof(pBuffer)!=0)
	{
		i-=sizeof(pBuffer);
		for( ; i<w25qxx.SectorSize; i++)
		{
		    uint8_t cmd[16], *cmdp = cmd;
			W25qxxSet();
			WorkAddress=(i+Sector_Address*w25qxx.SectorSize);
			(*cmdp++) = (0x0B);
			  cmdp += W25qxx_AddressCmds(cmdp, WorkAddress);
			(*cmdp++) = (0);
			  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
			  W25qxx_SpiRx(pBuffer, 1, 100);
			W25qxxUnset();
			if(pBuffer[0]!=0xFF)
				goto NOT_EMPTY;
		}
	}	
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckSector is Empty in %d ms\r\n",HAL_GetTick()-StartTime);
	W25qxx_Delay(100);
	#endif	
  W25qxx_Unlock();
	return true;	
	NOT_EMPTY:
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckSector is Not Empty in %d ms\r\n",HAL_GetTick()-StartTime);
	W25qxx_Delay(100);
	#endif	
  W25qxx_Unlock();
	return false;
}
//###################################################################################################################
bool 	W25qxx_IsEmptyBlock(uint32_t Block_Address,uint32_t OffsetInByte,uint32_t NumByteToCheck_up_to_BlockSize)
{
  W25qxx_WaitAndLock();
	if((NumByteToCheck_up_to_BlockSize>w25qxx.BlockSize)||(NumByteToCheck_up_to_BlockSize==0))
		NumByteToCheck_up_to_BlockSize=w25qxx.BlockSize;
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckBlock:%d, Offset:%d, Bytes:%d begin...\r\n",Block_Address,OffsetInByte,NumByteToCheck_up_to_BlockSize);
	W25qxx_Delay(100);
	StartTime=HAL_GetTick();
	#endif		
	uint8_t	pBuffer[32];
	uint32_t	WorkAddress;
	uint32_t	i;
	for(i=OffsetInByte; i<w25qxx.BlockSize; i+=sizeof(pBuffer))
	{
	    uint8_t cmd[16], *cmdp = cmd;
		W25qxxSet();
		WorkAddress=(i+Block_Address*w25qxx.BlockSize);
		(*cmdp++) = (0x0B);
		  cmdp += W25qxx_AddressCmds(cmdp, WorkAddress);
		(*cmdp++) = (0);
		  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
		  W25qxx_SpiRx(pBuffer, sizeof(pBuffer), 100);
		W25qxxUnset();
		for(uint8_t x=0;x<sizeof(pBuffer);x++)
		{
			if(pBuffer[x]!=0xFF)
				goto NOT_EMPTY;		
		}			
	}	
	if((w25qxx.BlockSize+OffsetInByte)%sizeof(pBuffer)!=0)
	{
		i-=sizeof(pBuffer);
		for( ; i<w25qxx.BlockSize; i++)
		{
		    uint8_t cmd[16], *cmdp = cmd;
			W25qxxSet();
			WorkAddress=(i+Block_Address*w25qxx.BlockSize);
			(*cmdp++) = (0x0B);
			  cmdp += W25qxx_AddressCmds(cmdp, WorkAddress);
			(*cmdp++) = (0);
			  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
			  W25qxx_SpiRx(pBuffer, 1, 100);
			W25qxxUnset();
			if(pBuffer[0]!=0xFF)
				goto NOT_EMPTY;
		}
	}	
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckBlock is Empty in %d ms\r\n",HAL_GetTick()-StartTime);
	W25qxx_Delay(100);
	#endif	
  W25qxx_Unlock();
	return true;	
	NOT_EMPTY:
	#if (_W25QXX_DEBUG==1)
	printf("w25qxx CheckBlock is Not Empty in %d ms\r\n",HAL_GetTick()-StartTime);
	W25qxx_Delay(100);
	#endif	
  W25qxx_Unlock();
	return false;
}
//###################################################################################################################
bool W25qxx_WriteByte_Initiate(uint8_t pBuffer, uint32_t WriteAddr_inBytes)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
#if (_W25QXX_DEBUG==1)
  StartTime=HAL_GetTick();
  printf("w25qxx WriteByte 0x%02X at address %d begin...",pBuffer,WriteAddr_inBytes);
#endif
  W25qxx_WaitForWriteEnd();
  W25qxx_WriteEnable();
  W25qxxSet();
  uint8_t cmd[16], *cmdp = cmd;
  (*cmdp++) = (0x02);
  cmdp += W25qxx_AddressCmds(cmdp, WriteAddr_inBytes);
  (*cmdp++) = (pBuffer);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_WriteByte(uint8_t pBuffer, uint32_t WriteAddr_inBytes)
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_WriteByte_Initiate(pBuffer, WriteAddr_inBytes)) {}
  // Block until ready
  while(!W25qxx_CheckForWriteEndAndUnlock()) {}
}
//###################################################################################################################
bool W25qxx_WritePage_Initiate(uint8_t *pBuffer	,uint32_t Page_Address,uint32_t OffsetInByte,uint32_t NumByteToWrite_up_to_PageSize)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
  if(((NumByteToWrite_up_to_PageSize+OffsetInByte)>w25qxx.PageSize)||(NumByteToWrite_up_to_PageSize==0))
	  NumByteToWrite_up_to_PageSize=w25qxx.PageSize-OffsetInByte;
  if((OffsetInByte+NumByteToWrite_up_to_PageSize) > w25qxx.PageSize)
	  NumByteToWrite_up_to_PageSize = w25qxx.PageSize-OffsetInByte;
#if (_W25QXX_DEBUG==1)
  printf("w25qxx WritePage:%d, Offset:%d ,Writes %d Bytes, begin...\r\n",Page_Address,OffsetInByte,NumByteToWrite_up_to_PageSize);
  W25qxx_Delay(100);
  StartTime=HAL_GetTick();
#endif
  uint32_t Addr = (Page_Address*w25qxx.PageSize)+OffsetInByte;
  W25qxx_WaitForWriteEnd();
  W25qxx_WriteEnable();
  W25qxxSet();
  uint8_t cmd[16], *cmdp = cmd;
  (*cmdp++) = (0x02);
  cmdp += W25qxx_AddressCmds(cmdp, Addr);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxx_SpiTx(pBuffer,NumByteToWrite_up_to_PageSize, 100);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_WritePage(uint8_t *pBuffer, uint32_t Page_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_PageSize)
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_WritePage_Initiate(pBuffer, Page_Address, OffsetInByte, NumByteToWrite_up_to_PageSize)) {}
  // Block until ready
  while(!W25qxx_CheckForWriteEndAndUnlock()) {}
}
//###################################################################################################################
void W25qxx_WriteSector(uint8_t *pBuffer	,uint32_t Sector_Address,uint32_t OffsetInByte	,uint32_t NumByteToWrite_up_to_SectorSize)
{
	if((NumByteToWrite_up_to_SectorSize>w25qxx.SectorSize)||(NumByteToWrite_up_to_SectorSize==0))
		NumByteToWrite_up_to_SectorSize=w25qxx.SectorSize;
	#if (_W25QXX_DEBUG==1)
	printf("+++w25qxx WriteSector:%d, Offset:%d ,Write %d Bytes, begin...\r\n",Sector_Address,OffsetInByte,NumByteToWrite_up_to_SectorSize);
	W25qxx_Delay(100);
	#endif	
	if(OffsetInByte>=w25qxx.SectorSize)
	{
		#if (_W25QXX_DEBUG==1)
		printf("---w25qxx WriteSector Faild!\r\n");
		W25qxx_Delay(100);
		#endif	
		return;
	}	
	uint32_t	StartPage;
	int32_t		BytesToWrite;
	uint32_t	LocalOffset;	
	if((OffsetInByte+NumByteToWrite_up_to_SectorSize) > w25qxx.SectorSize)
		BytesToWrite = w25qxx.SectorSize-OffsetInByte;
	else
		BytesToWrite = NumByteToWrite_up_to_SectorSize;	
	StartPage = W25qxx_SectorToPage(Sector_Address)+(OffsetInByte/w25qxx.PageSize);
	LocalOffset = OffsetInByte%w25qxx.PageSize;	
	do
	{		
		W25qxx_WritePage(pBuffer,StartPage,LocalOffset,BytesToWrite);
		StartPage++;
		BytesToWrite-=w25qxx.PageSize-LocalOffset;
		pBuffer+=w25qxx.PageSize;	
		LocalOffset=0;
	}while(BytesToWrite>0);		
	#if (_W25QXX_DEBUG==1)
	printf("---w25qxx WriteSector Done\r\n");
	W25qxx_Delay(100);
	#endif	
}
//###################################################################################################################
void 	W25qxx_WriteBlock	(uint8_t* pBuffer ,uint32_t Block_Address	,uint32_t OffsetInByte	,uint32_t	NumByteToWrite_up_to_BlockSize)
{
	if((NumByteToWrite_up_to_BlockSize>w25qxx.BlockSize)||(NumByteToWrite_up_to_BlockSize==0))
		NumByteToWrite_up_to_BlockSize=w25qxx.BlockSize;
	#if (_W25QXX_DEBUG==1)
	printf("+++w25qxx WriteBlock:%d, Offset:%d ,Write %d Bytes, begin...\r\n",Block_Address,OffsetInByte,NumByteToWrite_up_to_BlockSize);
	W25qxx_Delay(100);
	#endif	
	if(OffsetInByte>=w25qxx.BlockSize)
	{
		#if (_W25QXX_DEBUG==1)
		printf("---w25qxx WriteBlock Faild!\r\n");
		W25qxx_Delay(100);
		#endif	
		return;
	}	
	uint32_t	StartPage;
	int32_t		BytesToWrite;
	uint32_t	LocalOffset;	
	if((OffsetInByte+NumByteToWrite_up_to_BlockSize) > w25qxx.BlockSize)
		BytesToWrite = w25qxx.BlockSize-OffsetInByte;
	else
		BytesToWrite = NumByteToWrite_up_to_BlockSize;	
	StartPage = W25qxx_BlockToPage(Block_Address)+(OffsetInByte/w25qxx.PageSize);
	LocalOffset = OffsetInByte%w25qxx.PageSize;	
	do
	{		
		W25qxx_WritePage(pBuffer,StartPage,LocalOffset,BytesToWrite);
		StartPage++;
		BytesToWrite-=w25qxx.PageSize-LocalOffset;
		pBuffer+=w25qxx.PageSize;	
		LocalOffset=0;
	}while(BytesToWrite>0);		
	#if (_W25QXX_DEBUG==1)
	printf("---w25qxx WriteBlock Done\r\n");
	W25qxx_Delay(100);
	#endif	
}
//###################################################################################################################
bool W25qxx_ReadByte_Initiate(uint8_t *pBuffer,uint32_t Bytes_Address)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
#if (_W25QXX_DEBUG==1)
  StartTime=HAL_GetTick();
  printf("w25qxx ReadByte at address %d begin...\r\n",Bytes_Address);
#endif
  W25qxxSet();
  uint8_t cmd[16], *cmdp = cmd;
  (*cmdp++) = (0x0B);
  cmdp += W25qxx_AddressCmds(cmdp, Bytes_Address);
  (*cmdp++) = (0);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  *pBuffer = W25qxx_Spi(W25QXX_DUMMY_BYTE);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_ReadByte(uint8_t *pBuffer,uint32_t Bytes_Address)
{
  // Block until lock released, then initiate erasing
  while(!W25qxx_ReadByte_Initiate(pBuffer, Bytes_Address)) {}
#if (_W25QXX_DEBUG==1)
  printf("w25qxx ReadByte 0x%02X done after %d ms\r\n",*pBuffer,HAL_GetTick()-StartTime);
#endif
  W25qxx_Unlock();
}
//###################################################################################################################
bool W25qxx_ReadBytes_Initiate(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
  #if (_W25QXX_DEBUG==1)
    StartTime=HAL_GetTick();
    printf("w25qxx ReadBytes at Address:%d, %d Bytes  begin...\r\n",ReadAddr,NumByteToRead);
  #endif
  W25qxxSet();
  uint8_t cmd[16], *cmdp = cmd;
  (*cmdp++) = (0x0B);
  cmdp += W25qxx_AddressCmds(cmdp, ReadAddr);
  (*cmdp++) = (0);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxx_SpiRx(pBuffer,NumByteToRead,2000);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_ReadBytes(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)
{
  // Block until lock released, then initiate process
  while(!W25qxx_ReadBytes_Initiate(pBuffer, ReadAddr, NumByteToRead)) {}
#if (_W25QXX_DEBUG==1)
  uint32_t elapsed = HAL_GetTick()-StartTime;
  for(uint32_t i=0;i<NumByteToRead ; i++)
  {
    if((i%8==0)&&(i>2))
    {
      printf("\r\n");
      W25qxx_Delay(10);
    }
    printf("0x%02X,",pBuffer[i]);
  }
  printf("\r\n");
  printf("w25qxx ReadBytes done after %d ms\r\n",elapsed);
  W25qxx_Delay(100);
#endif
  W25qxx_Unlock();
}
//###################################################################################################################
bool W25qxx_ReadPage_Initiate(uint8_t *pBuffer,uint32_t Page_Address,uint32_t OffsetInByte,uint32_t NumByteToRead_up_to_PageSize)
{
  if(W25qxx_IsLocked()) return false;
  W25qxx_Lock();
  if((NumByteToRead_up_to_PageSize>w25qxx.PageSize)||(NumByteToRead_up_to_PageSize==0))
	  NumByteToRead_up_to_PageSize=w25qxx.PageSize;
  if((OffsetInByte+NumByteToRead_up_to_PageSize) > w25qxx.PageSize)
	  NumByteToRead_up_to_PageSize = w25qxx.PageSize-OffsetInByte;
  #if (_W25QXX_DEBUG==1)
  printf("w25qxx ReadPage:%d, Offset:%d ,Read %d Bytes, begin...\r\n",Page_Address,OffsetInByte,NumByteToRead_up_to_PageSize);
  W25qxx_Delay(100);
  StartTime=HAL_GetTick();
  #endif
  uint32_t Addr = Page_Address*w25qxx.PageSize+OffsetInByte;
  W25qxxSet();
  uint8_t cmd[16], *cmdp = cmd;
  (*cmdp++) = (0x0B);
  cmdp += W25qxx_AddressCmds(cmdp, Addr);
  (*cmdp++) = (0);
  W25qxx_SpiTx(cmd, cmdp-cmd, 100);
  W25qxx_SpiRx(pBuffer,NumByteToRead_up_to_PageSize, 100);
  W25qxxUnset();
  return true;
}
//###################################################################################################################
void W25qxx_ReadPage(uint8_t *pBuffer,uint32_t Page_Address,uint32_t OffsetInByte,uint32_t NumByteToRead_up_to_PageSize)
{
  // Block until lock released, then initiate process
  while(!W25qxx_ReadPage_Initiate(pBuffer, Page_Address, OffsetInByte, NumByteToRead_up_to_PageSize)) {}
#if (_W25QXX_DEBUG==1)
  uint32_t elapsed = HAL_GetTick()-StartTime;
  for(uint32_t i=0;i<NumByteToRead_up_to_PageSize ; i++)
  {
	  if((i%8==0)&&(i>2))
	  {
		  printf("\r\n");
		  W25qxx_Delay(10);
	  }
	  printf("0x%02X,",pBuffer[i]);
  }
  printf("\r\n");
  printf("w25qxx ReadPage done after %d ms\r\n",elapsed);
  W25qxx_Delay(100);
#endif
  W25qxx_Unlock();
}
//###################################################################################################################
void 	W25qxx_ReadSector(uint8_t *pBuffer,uint32_t Sector_Address,uint32_t OffsetInByte,uint32_t NumByteToRead_up_to_SectorSize)
{	
	if((NumByteToRead_up_to_SectorSize>w25qxx.SectorSize)||(NumByteToRead_up_to_SectorSize==0))
		NumByteToRead_up_to_SectorSize=w25qxx.SectorSize;
	#if (_W25QXX_DEBUG==1)
	printf("+++w25qxx ReadSector:%d, Offset:%d ,Read %d Bytes, begin...\r\n",Sector_Address,OffsetInByte,NumByteToRead_up_to_SectorSize);
	W25qxx_Delay(100);
	#endif	
	if(OffsetInByte>=w25qxx.SectorSize)
	{
		#if (_W25QXX_DEBUG==1)
		printf("---w25qxx ReadSector Faild!\r\n");
		W25qxx_Delay(100);
		#endif	
		return;
	}	
	uint32_t	StartPage;
	int32_t		BytesToRead;
	uint32_t	LocalOffset;	
	if((OffsetInByte+NumByteToRead_up_to_SectorSize) > w25qxx.SectorSize)
		BytesToRead = w25qxx.SectorSize-OffsetInByte;
	else
		BytesToRead = NumByteToRead_up_to_SectorSize;	
	StartPage = W25qxx_SectorToPage(Sector_Address)+(OffsetInByte/w25qxx.PageSize);
	LocalOffset = OffsetInByte%w25qxx.PageSize;	
	do
	{		
		W25qxx_ReadPage(pBuffer,StartPage,LocalOffset,BytesToRead);
		StartPage++;
		BytesToRead-=w25qxx.PageSize-LocalOffset;
		pBuffer+=w25qxx.PageSize;	
		LocalOffset=0;
	}while(BytesToRead>0);		
	#if (_W25QXX_DEBUG==1)
	printf("---w25qxx ReadSector Done\r\n");
	W25qxx_Delay(100);
	#endif	
}
//###################################################################################################################
void 	W25qxx_ReadBlock(uint8_t* pBuffer,uint32_t Block_Address,uint32_t OffsetInByte,uint32_t	NumByteToRead_up_to_BlockSize)
{
	if((NumByteToRead_up_to_BlockSize>w25qxx.BlockSize)||(NumByteToRead_up_to_BlockSize==0))
		NumByteToRead_up_to_BlockSize=w25qxx.BlockSize;
	#if (_W25QXX_DEBUG==1)
	printf("+++w25qxx ReadBlock:%d, Offset:%d ,Read %d Bytes, begin...\r\n",Block_Address,OffsetInByte,NumByteToRead_up_to_BlockSize);
	W25qxx_Delay(100);
	#endif	
	if(OffsetInByte>=w25qxx.BlockSize)
	{
		#if (_W25QXX_DEBUG==1)
		printf("w25qxx ReadBlock Faild!\r\n");
		W25qxx_Delay(100);
		#endif	
		return;
	}	
	uint32_t	StartPage;
	int32_t		BytesToRead;
	uint32_t	LocalOffset;	
	if((OffsetInByte+NumByteToRead_up_to_BlockSize) > w25qxx.BlockSize)
		BytesToRead = w25qxx.BlockSize-OffsetInByte;
	else
		BytesToRead = NumByteToRead_up_to_BlockSize;	
	StartPage = W25qxx_BlockToPage(Block_Address)+(OffsetInByte/w25qxx.PageSize);
	LocalOffset = OffsetInByte%w25qxx.PageSize;	
	do
	{		
		W25qxx_ReadPage(pBuffer,StartPage,LocalOffset,BytesToRead);
		StartPage++;
		BytesToRead-=w25qxx.PageSize-LocalOffset;
		pBuffer+=w25qxx.PageSize;	
		LocalOffset=0;
	}while(BytesToRead>0);		
	#if (_W25QXX_DEBUG==1)
	printf("---w25qxx ReadBlock Done\r\n");
	W25qxx_Delay(100);
	#endif	
}
//###################################################################################################################

