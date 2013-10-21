#include <stm32f4xx.h>
#include <stdio.h>
#include <stdlib.h>
#include "ff.h"
#include "diskio.h"

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

void USART_putn(USART_TypeDef* USARTx, unsigned char* data, uint16_t len){
	while(len--){
		USART_putc(USARTx, *data);
		data++;
	}
}

/* This funcion shows how to initialize
* the GPIO pins on GPIOD and how to configure
* them as inputs and outputs
*/
void init_GPIO(void){

	/* This TypeDef is a structure defined in the
	* ST's library and it contains all the properties
	* the corresponding peripheral has, such as output mode,
	* pullup / pulldown resistors etc.
	*
	* These structures are defined for every peripheral so
	* every peripheral has it's own TypeDef. The good news is
	* they always work the same so once you've got a hang
	* of it you can initialize any peripheral.
	*
	* The properties of the periperals can be found in the corresponding
	* header file e.g. stm32f4xx_gpio.h and the source file stm32f4xx_gpio.c
	*/
	GPIO_InitTypeDef GPIO_InitStruct;

	/* This enables the peripheral clock to the GPIOD IO module
	* Every peripheral's clock has to be enabled
	*
	* The STM32F4 Discovery's User Manual and the STM32F407VGT6's
	* datasheet contain the information which peripheral clock has to be used.
	*
	* It is also mentioned at the beginning of the peripheral library's
	* source file, e.g. stm32f4xx_gpio.c
	*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	/* In this block of instructions all the properties
	* of the peripheral, the GPIO port in this case,
	* are filled with actual information and then
	* given to the Init function which takes care of
	* the low level stuff (setting the correct bits in the
	* peripheral's control register)
	*
	*
	* The LEDs on the STM324F Discovery are connected to the
	* pins PD12 thru PD15
	*/
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15 | GPIO_Pin_14 | GPIO_Pin_13 | GPIO_Pin_12; // we want to configure all LED GPIO pins
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT; // we want the pins to be an output
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz; // this sets the GPIO modules clock speed
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; // this sets the pin type to push / pull (as opposed to open drain)
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL; // this sets the pullup / pulldown resistors to be inactive
	GPIO_Init(GPIOD, &GPIO_InitStruct); // this finally passes all the values to the GPIO_Init function which takes care of setting the corresponding bits.

	/* This enables the peripheral clock to
	* the GPIOA IO module
	*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	/* Here the GPIOA module is initialized.
	* We want to use PA0 as an input because
	* the USER button on the board is connected
	* between this pin and VCC.
	*/
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;	// we want to configure PA0
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN; // we want it to be an input
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;//this sets the GPIO modules clock speed
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP; // this sets the pin type to push / pull (as opposed to open drain)
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN; // this enables the pulldown resistor --> we want to detect a high level
	GPIO_Init(GPIOA, &GPIO_InitStruct);	// this passes the configuration to the Init function which takes care of the low level stuff
}

int main(void)
{
	init_GPIO();
	init_USART1(9600);
	
	FATFS FatFs;
	FIL fil;
	FRESULT res;
	
	unsigned char buffer[6];


	res = f_mount(&FatFs, "", 1); // mount the drive
	res = f_open(&fil, "hello.txt", FA_OPEN_EXISTING | FA_WRITE | FA_READ); // open existing file in read and write mode
	
	//f_lseek(&fil, 0); // set rw pointer to top of file
	f_gets(buffer, 6, &fil);
	
	//f_lseek(&fil, 6); // set rw pointer to pos 6
	f_puts(buffer, &fil);
	
	f_close(&fil); // close the file
	f_mount(NULL, "", 1); // unmount the drive
	
	
	uint8_t i = 0; 

	while (1){  

	}
}
