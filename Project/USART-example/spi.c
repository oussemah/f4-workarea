#include <stm32f4xx.h>
#include <stm32f4xx_spi.h>

#include "stm32f4_discovery.h"

uint8_t SDHC = 0;

/* Definitions for MMC/SDC command */ 
#define GO_IDLE_STATE (0x40+0) /* GO_IDLE_STATE */ 
#define MMC_SEND_OP_COND (0x40+1) /* SEND_OP_COND (MMC) */ 
#define SD_SEND_OP_COND (0xC0+41) /* SEND_OP_COND (SDC) */ 
#define SEND_IF_COND (0x40+8) /* SEND_IF_COND */ 
#define SEND_CSD (0x40+9) /* SEND_CSD */ 
#define SEND_CID (0x40+10) /* SEND_CID */ 
#define STOP_TRANSMISSION (0x40+12) /* STOP_TRANSMISSION */ 
#define SD_STATUS (0xC0+13) /* SD_STATUS (SDC) */ 
#define SET_BLOCKLEN (0x40+16) /* SET_BLOCKLEN */ 
#define READ_SINGLE_BLOCK (0x40+17) /* READ_SINGLE_BLOCK */ 
#define READ_MULTIPLE_BLOCK (0x40+18) /* READ_MULTIPLE_BLOCK */ 
#define SET_BLOCK_COUNT (0x40+23) /* SET_BLOCK_COUNT (MMC) */ 
#define SET_WR_BLK_ERASE_COUNT (0xC0+23) /* SET_WR_BLK_ERASE_COUNT (SDC) */ 
#define WRITE_SINGLE_BLOCK (0x40+24) /* WRITE_BLOCK */ 
#define WRITE_MULTIPLE_BLOCK (0x40+25) /* WRITE_MULTIPLE_BLOCK */ 
#define APP_CMD (0x40+55) /* APP_CMD */ 
#define READ_OCR (0x40+58) /* READ_OCR */ 

#define DEBUG

#define LED_RED     LED5
#define LED_GREEN   LED4

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
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;

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

#ifdef DEBUG
    STM_EVAL_LEDInit(LED_GREEN);
    STM_EVAL_LEDInit(LED_RED);
#endif
}

uint8_t spi_send (uint8_t data) {

#ifdef DEBUG
    STM_EVAL_LEDOff(LED_GREEN);
    STM_EVAL_LEDOff(LED_RED);
#endif
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, data);
#ifdef DEBUG
    STM_EVAL_LEDOn(LED_GREEN);
#endif
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI2);
#ifdef DEBUG
    STM_EVAL_LEDOn(LED_RED);
#endif
}

#undef DEBUG
uint8_t spi_read (void)
{
    return spi_send(0xff); //читаем принятые данные
}

//********************************************************************************************
//function посылка команды в SD //
//Arguments команда и ее аргумент //
//return 0xff - нет ответа //
//********************************************************************************************
uint8_t SD_sendCommand(uint8_t cmd, uint32_t arg)
{
    uint8_t response, wait=0, tmp;

//для карт памяти SD выполнить корекцию адреса, т.к. для них адресация побайтная
    if(SDHC == 0)
        if(cmd == READ_SINGLE_BLOCK || cmd == WRITE_SINGLE_BLOCK ) {
            arg = arg << 9;
        }
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
    else spi_send(0x95);

//ожидаем ответ
    while((response = spi_read()) == 0xff)
        if(wait++ > 0xfe) break; //таймаут, не получили ответ на команду

//проверка ответа если посылалась команда READ_OCR
    if(response == 0x00 && cmd == 58)
    {
        tmp = spi_read(); //прочитат один байт регистра OCR
        if(tmp & 0x40) SDHC = 1; //обнаружена карта SDHC
        else SDHC = 0; //обнаружена карта SD
//прочитать три оставшихся байта регистра OCR
        spi_read();
        spi_read();
        spi_read();
    }

    spi_read();

    GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS_DISABLE GPIO_ResetBits

    return response;
}

//********************************************************************************************
//function инициализация карты памяти //
//return 0 - карта инициализирована //
//********************************************************************************************
uint8_t SD_init(void)
{
    uint8_t i;
    uint8_t response;
    uint8_t SD_version = 2; //по умолчанию версия SD = 2
    uint16_t retry = 0 ;

    for(i=0; i<10; i++) spi_send(0xff); //послать свыше 74 единиц

//выполним программный сброс карты
// CS_ENABLE;
    while(SD_sendCommand(GO_IDLE_STATE, 0)!=0x01)
        if(retry++>0x20) return 1;
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
    } while(response != 0x00);


//читаем регистр OCR, чтобы определить тип карты
    retry = 0;
    SDHC = 0;
    if (SD_version == 2)
    {
        while(SD_sendCommand(READ_OCR,0)!=0x00)
            if(retry++>0xfe) break;
    }

    return 0;
}

//********************************************************************************************
//function чтение выбранного сектора SD //
//аrguments номер сектора,указатель на буфер размером 512 байт //
//return 0 - сектор прочитан успешно //
//********************************************************************************************
uint8_t SD_ReadSector(uint32_t BlockNumb,uint8_t *buff)
{
    uint16_t i=0;

//послать команду "чтение одного блока" с указанием его номера
    if(SD_sendCommand(READ_SINGLE_BLOCK, BlockNumb)) return 1;
    GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE;
//ожидание маркера данных
    while(spi_read() != 0xfe)
        if(i++ > 0xfffe) {
            GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/ return 2;
        }

//чтение 512 байт выбранного сектора
    for(i=0; i<512; i++) *buff++ = spi_read();

    spi_read();
    spi_read();
    spi_read();

    GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS_DISABLE;

    return 0;
}

//********************************************************************************************
//function запись выбранного сектора SD //
//аrguments номер сектора, указатель на данные для записи //
//return 0 - сектор записан успешно //
//********************************************************************************************
uint8_t SD_WriteSector(uint32_t BlockNumb,uint8_t *buff)
{
    uint8_t response;
    uint16_t i,wait=0;

//послать команду "запись одного блока" с указанием его номера
    if( SD_sendCommand(WRITE_SINGLE_BLOCK, BlockNumb)) return 1;

    GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE;
    spi_send(0xfe);

//записать буфер сектора в карту
    for(i=0; i<512; i++) spi_send(*buff++);

    spi_send(0xff); //читаем 2 байта CRC без его проверки
    spi_send(0xff);

    response = spi_read();

    if( (response & 0x1f) != 0x05) //если ошибка при приеме данных картой
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/ return 1;
    }

//ожидаем окончание записи блока данных
    while(!spi_read()) //пока карта занята,она выдает ноль
        if(wait++ > 0xfffe) {
            GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/ return 1;
        }

    GPIO_SetBits(GPIOB, GPIO_Pin_12); //CS_DISABLE;
    spi_send(0xff);
    GPIO_ResetBits(GPIOB, GPIO_Pin_12); // CS_ENABLE;

    while(!spi_read()) //пока карта занята,она выдает ноль
        if(wait++ > 0xfffe) {
            GPIO_SetBits(GPIOB, GPIO_Pin_12); /*CS_DISABLE;*/ return 1;
        }
    GPIO_SetBits(GPIOB, GPIO_Pin_12); // CS_DISABLE;

    return 0;
}

/*
int main(void)
{

    SystemInit();
    initAll();
    SPI_Config();

    printf("Started USART!\n");
    uint8_t sd_st=SD_init();
    if(sd_st) {
        printf("Err SD Init! code:%d\n",sd_st);
    } else {
        printf("SD inited! SDHC:%d\n",SDHC); // тут выводим тип карты: простая или HC
    }

    sd_st=SD_ReadSector(2, (uint8_t *)&Buff); // читаем 2-й сектор
    if(sd_st) {
        printf("Err reead SD! code:%d\n",sd_st);
    } else {
        uint8_t i;
        for(i=0; i<100; i++) { // выводим первую сотню байт, где увидем, что у нас FAT32 на флешке
            UsartPutData(Buff[i]); //см. мой вариант реализации общения по USART
        }
    }
    UsartSend();

    while(1) {}
}

*/
