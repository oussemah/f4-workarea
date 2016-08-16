#include <stm32f4xx.h>
#include <stdio.h>
#include <stdlib.h>
#include "ff.h"
#include "diskio.h"

uint8_t SDHC = 0;

#define SEND_IF_COND    (0x40+ 8)
#define APP_CMD         (0x40+ 55)
#define READ_OCR        (0x40+ 58)
#define SD_SEND_OP_COND    (0x40 + 1)

void SPI_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  SPI_InitTypeDef SPI_InitStructure;

  /* Peripheral Clock Enable -------------------------------------------------*/
  /* Enable the SPI clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

  /* Enable GPIO clocks */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);


  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS - выбор устройства осуществлять будем ручками
  // если 1, то выклечено, а если 0, то передаем данные
  // Подключаем пины порта B к пинам как подписано на переходнике (сдесь активируем AF):
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2); // SCK
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2); // MISO
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2); // MOSI

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; // Все пины должны быть строго как AF
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  SPI_I2S_DeInit(SPI2);
  // Включаем SPI в нужный режим
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  /* Initializes the SPI communication */
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_Init(SPI2, &SPI_InitStructure);

  SPI_Cmd(SPI2, ENABLE); // SPI - включен!
}

uint8_t spi_send (uint8_t data){
   while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
   SPI_I2S_SendData(SPI2, data);
   while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
   return SPI_I2S_ReceiveData(SPI2);
}
uint8_t spi_read (void)
{
   return spi_send(0xff);        //читаем принятые данные
}

//********************************************************************************************
//function    посылка команды в SD                                                  //
//Arguments    команда и ее аргумент                                                      //
//return    0xff - нет ответа                                                //
//********************************************************************************************
uint8_t SD_sendCommand(uint8_t cmd, uint32_t arg)
{
  uint8_t response, wait=0, tmp;

  //для карт памяти SD выполнить корекцию адреса, т.к. для них адресация побайтная
  if(SDHC == 0)
  if(cmd == READ_SINGLE_BLOCK || cmd == WRITE_SINGLE_BLOCK )  {arg = arg << 9;}
  //для SDHC корекцию адреса блока выполнять не нужно(постраничная адресация)

  GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE GPIO_SetBits

  //передать код команды и ее аргумент
  spi_send(cmd | 0x40);
  spi_send(arg>>24);
  spi_send(arg>>16);
  spi_send(arg>>8);
  spi_send(arg);

  //передать CRC (учитываем только для двух команд)
  if(cmd == SEND_IF_COND) spi_send(0x87);
  else                    spi_send(0x95);

  //ожидаем ответ
  while((response = spi_read()) == 0xff)
   if(wait++ > 0xfe) break;                //таймаут, не получили ответ на команду

  //проверка ответа если посылалась команда READ_OCR
  if(response == 0x00 && cmd == 58)
  {
    tmp = spi_read();                      //прочитат один байт регистра OCR
    if(tmp & 0x40) SDHC = 1;               //обнаружена карта SDHC
    else           SDHC = 0;               //обнаружена карта SD
    //прочитать три оставшихся байта регистра OCR
    spi_read();
    spi_read();
    spi_read();
  }

  spi_read();

  GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS_DISABLE GPIO_ResetBits

  return response;
}

uint8_t SDHC;


//********************************************************************************************
//function    инициализация карты памяти                                      //
//return    0 - карта инициализирована                             //
//********************************************************************************************
uint8_t SD_init(void)
{
  uint8_t   i;
  uint8_t   response;
  uint8_t   SD_version = 2;             //по умолчанию версия SD = 2
  uint16_t  retry = 0 ;

  for(i=0;i<10;i++) spi_send(0xff);      //послать свыше 74 единиц

  //выполним программный сброс карты
 // CS_ENABLE;
  while(SD_sendCommand(GO_IDLE_STATE, 0)!=0x01)
    if(retry++>0x20)  return 1;
 // CS_DISABLE;
  spi_send (0xff);
  spi_send (0xff);

  retry = 0;
  while(SD_sendCommand(SEND_IF_COND,0x000001AA)!=0x01)
  {
    if(retry++>0xfe)
    {
      SD_version = 1;
      break;
    }
  }

 retry = 0;
 do
 {
   response = SD_sendCommand(APP_CMD,0);
   response = SD_sendCommand(SD_SEND_OP_COND,0x40000000);
   retry++;
   if(retry>0xffe) return 2;
 }while(response != 0x00);


 //читаем регистр OCR, чтобы определить тип карты
 retry = 0;
 SDHC = 0;
 if (SD_version == 2)
 {
   while(SD_sendCommand(READ_OCR,0)!=0x00)
    if(retry++>0xfe)  break;
 }

 return 0;
}

//********************************************************************************************
//function    чтение выбранного сектора SD                                      //
//аrguments    номер сектора,указатель на буфер размером 512 байт                         //
//return    0 - сектор прочитан успешно                              //
//********************************************************************************************
uint8_t SD_ReadSector(uint32_t BlockNumb,uint8_t *buff)
{
  uint16_t i=0;

  //послать команду "чтение одного блока" с указанием его номера
  if(SD_sendCommand(READ_SINGLE_BLOCK, BlockNumb)) return 1;
  GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE;
  //ожидание  маркера данных
  while(spi_read() != 0xfe)
  if(i++ > 0xfffe) {GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/ return 2;}

  //чтение 512 байт   выбранного сектора
  for(i=0; i<512; i++) *buff++ = spi_read();

  spi_read();
  spi_read();
  spi_read();

  GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS_DISABLE;

  return 0;
}

//********************************************************************************************
//function    запись выбранного сектора SD                                      //
//аrguments    номер сектора, указатель на данные для записи                              //
//return    0 - сектор записан успешно                              //
//********************************************************************************************
uint8_t SD_WriteSector(uint32_t BlockNumb,uint8_t *buff)
{
  uint8_t     response;
  uint16_t    i,wait=0;

  //послать команду "запись одного блока" с указанием его номера
  if( SD_sendCommand(WRITE_SINGLE_BLOCK, BlockNumb)) return 1;

  GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE;
  spi_send(0xfe);

  //записать буфер сектора в карту
  for(i=0; i<512; i++) spi_send(*buff++);

  spi_send(0xff);                //читаем 2 байта CRC без его проверки
  spi_send(0xff);

  response = spi_read();

  if( (response & 0x1f) != 0x05) //если ошибка при приеме данных картой
  { GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/  return 1; }

  //ожидаем окончание записи блока данных
  while(!spi_read())             //пока карта занята,она выдает ноль
  if(wait++ > 0xfffe){GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/ return 1;}

  GPIO_SetBits(GPIOB, GPIO_Pin_12); //CS_DISABLE;
  spi_send(0xff);
  GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE;

  while(!spi_read())               //пока карта занята,она выдает ноль
  if(wait++ > 0xfffe){GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/  return 1;}
  GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS_DISABLE;

  return 0;
}

/* This funcion initializes the USART1 peripheral
 * 
 * Arguments: baudrate --> the baudrate at which the USART is 
 * 						   supposed to operate
 */
void init_USART1(uint32_t baudrate){
	
	/* This is a concept that has to do with the libraries provided by ST
	 * to make development easier the have made up something similar to 
	 * classes, called TypeDefs, which actually just define the common
	 * parameters that every peripheral needs to work correctly
	 * 
	 * They make our life easier because we don't have to mess around with 
	 * the low level stuff of setting bits in the correct registers
	 */
	GPIO_InitTypeDef GPIO_InitStruct; // this is for the GPIO pins used as TX and RX
	USART_InitTypeDef USART_InitStruct; // this is for the USART1 initilization
	
	/* enable APB2 peripheral clock for USART1 
	 * note that only USART1 and USART6 are connected to APB2
	 * the other USARTs are connected to APB1
	 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	
	/* enable the peripheral clock for the pins used by 
	 * USART1, PB6 for TX and PB7 for RX
	 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	/* This sequence sets up the TX and RX pins 
	 * so they work correctly with the USART1 peripheral
	 */
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;				// Pins 6 (TX) 
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF; 			// the pins are configured as alternate function so the USART peripheral has access to them
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;		// this defines the IO speed and has nothing to do with the baudrate!
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;			// this defines the output type as push pull mode (as opposed to open drain)
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;			// this activates the pullup resistors on the IO pins
	GPIO_Init(GPIOB, &GPIO_InitStruct);					// now all the values are passed to the GPIO_Init() function which sets the GPIO registers
	
	/* The TX pin is now connected to their AF
	 * so that the USART1 can take over control of the 
	 * pin
	 */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1); //
	
	/* Now the USART_InitStruct is used to define the 
	 * properties of USART1 
	 */
	USART_InitStruct.USART_BaudRate = baudrate;				// the baudrate is set to the value we passed into this init function
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;// we want the data frame size to be 8 bits (standard)
	USART_InitStruct.USART_StopBits = USART_StopBits_1;		// we want 1 stop bit (standard)
	USART_InitStruct.USART_Parity = USART_Parity_No;		// we don't want a parity bit (standard)
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // we don't want flow control (standard)
	USART_InitStruct.USART_Mode = USART_Mode_Tx; 			// we want to enable the transmitter
	USART_Init(USART1, &USART_InitStruct);					// again all the properties are passed to the USART_Init function which takes care of all the bit setting

	// finally this enables the complete USART1 peripheral
	USART_Cmd(USART1, ENABLE);
}

/* This function is used to transmit a string of characters via 
 * the USART specified in USARTx.
 * 
 * It takes two arguments: USARTx --> can be any of the USARTs e.g. USART1, USART2 etc.
 * 						   (volatile) char *s is the string you want to send
 * 
 * Note: The string has to be passed to the function as a pointer because
 * 		 the compiler doesn't know the 'string' data type. In standard
 * 		 C a string is just an array of characters
 * 
 * Note 2: At the moment it takes a volatile char because the received_string variable
 * 		   declared as volatile char --> otherwise the compiler will spit out warnings
 * */
void USART_putc(USART_TypeDef* USARTx, unsigned char c){
	// wait until data register is empty
	while( !(USARTx->SR & 0x00000040) ); 
	USART_SendData(USARTx, c);
}


void USART_puts(USART_TypeDef* USARTx, volatile char *s){
	while(*s){
		USART_putc(USARTx, *s);
		s++;
	}
}

uint8_t Buff[512];

int main(void)
{
	init_USART1(9600);
	
	FATFS FatFs;
	FIL fil;
	FRESULT res;
	
	unsigned char buffer[6];

	USART_puts(USART1, "Mounting volume...");
#if 0	
    res = f_mount(&FatFs, "", 1); // mount the drive
	if (res)
	{
		USART_puts(USART1, "error occured!\n");
		while(1);
	}
	USART_puts(USART1, "success!\n");
	
	USART_puts(USART1, "Opening file: \"hello.txt\"...");
	res = f_open(&fil, "hello.txt", FA_OPEN_EXISTING | FA_WRITE | FA_READ); // open existing file in read and write mode
	if (res)
	{
		USART_puts(USART1, "error occured!\n");
		while(1);
	}
	USART_puts(USART1, "success!\n");
	
	f_gets(buffer, 6, &fil);
	
	USART_puts(USART1, "I read: \"");
	USART_puts(USART1, buffer);
	USART_puts(USART1, "\"\n");
	
	f_puts(buffer, &fil);
	
	f_close(&fil); // close the file
	f_mount(NULL, "", 1); // unmount the drive
	
#endif

#if 1

#define NEWL USART_puts(USART1,"\r\n");

    SPI_Config();
    uint8_t sd_st=SD_init();
    
     if(sd_st){
      USART_puts(USART1,"Err SD Init! code:");
      USART_putc(USART1, sd_st);
      NEWL
   }else{
      USART_puts(USART1, "SD inited! SDHC:");
      USART_putc(USART1, SDHC); // тут выводим тип карты: простая или HC
      NEWL
   }

   sd_st=SD_ReadSector(2, (uint8_t *)&Buff); // читаем 2-й сектор
   if(sd_st){
      USART_puts(USART1, "Err reead SD! code:");
      USART_putc(USART1, sd_st);
      NEWL
   }else{
      uint8_t i;
      for(i=0;i<100;i++){  // выводим первую сотню байт, где увидем, что у нас FAT32 на флешке
         USART_putc(USART1, Buff[i]); //см. мой вариант реализации общения по USART
      }
   }
#endif
	while (1){  

	}
}
