/**
 *
 * Bit-Banging of 32*32 RGB Matrix Panels using a STM32F401
 *
 * Author: Paul Rogalinski, paul@paul.vc
 * License: Public Domain, see LICENSE.txt and http://unlicense.org/
 *
 * This is for educational purposes only, driving such panels with
 * bitbanging is inefficient and pretty much a bad idea. One might
 * this code useful for prototyping purposes related to
 * the 32*32 or 32*16 RGB Panels before moving on to a more suited
 * hardware such as FPGA or at least some DMA Kung-Fu.
 *
 *
 * Pin Assignment
 * ==============================================
 *
 * PORT A: Color Data
 * ----------------------------------------------
 *
 * r1   PORT A: GPIO_PIN_5
 * g1   PORT A: GPIO_PIN_6
 * b1   PORT A: GPIO_PIN_7
 *
 * r2   PORT A: GPIO_PIN_8
 * g2   PORT A: GPIO_PIN_9
 * b2   PORT A: GPIO_PIN_10
 *
 *
 * Address Select, Output Enable
 * ----------------------------------------------
 *
 * A    PORT B: GPIO_PIN_5
 * B    PORT B: GPIO_PIN_6
 * C    PORT B: GPIO_PIN_8
 * D    PORT B: GPIO_PIN_9
 * OE   PORT B: GPIO_PIN_10
 *
 * USART
 * ----------------------------------------------
 * TX		PORT A: GPIO_PIN_2
 * RX		PORT A: GPIO_PIN_3
 *
 * Clock & Latch
 * ----------------------------------------------
 * CLK  PORT C: GPIO_PIN_7
 * STB  PORT C: GPIO_PIN_0
 */

#include <stdlib.h>

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "setup_hw.h"
#include <math.h>

#define DISP_ON         GPIOB->BSRRH = GPIO_PIN_10;
#define DISP_OFF        GPIOB->BSRRL = GPIO_PIN_10;
#define CLK_TOGGLE      GPIOC->ODR ^= (1 << 7); GPIOC->ODR ^= (1 << 7);
#define STROBE          GPIOC->BSRRL = GPIO_PIN_0; GPIOC->BSRRH = GPIO_PIN_0;

#define MATRIX_WIDTH    32
#define MATRIX_HEIGHT   32
#define MATRIX_SIZE     MATRIX_WIDTH*MATRIX_HEIGHT

#define RXBUFFERSIZE	256

void setRow(int row);
void showLine(int amount);
void setRGB(uint32_t rgb1, uint32_t rgb2, uint8_t plane );
void displayBuffer(uint32_t buffer[]);
void randomizeFramebuffer(uint32_t buffer[]);

const int waits[] = {10,20,40,80,160,320,640,1280};
const int scan =  MATRIX_HEIGHT/2;
uint8_t gammaTable[256];
uint32_t framebuffer[MATRIX_SIZE];

UART_HandleTypeDef uartHandle;
__IO ITStatus UartReady = RESET;
uint8_t aRxBuffer[RXBUFFERSIZE];


char* uartAliveMsg = "200 frames passed";


int	main() {

	initGPIO();
	initUART(&uartHandle);

	if(HAL_UART_Transmit_IT(&uartHandle, (uint8_t*) "RGB-matrix started", 18)  != HAL_OK) { }
	if(HAL_UART_Receive_IT (&uartHandle, (uint8_t*) aRxBuffer, RXBUFFERSIZE)   != HAL_OK) { }

	DISP_OFF;

	// precalculate the gamma lookup table
	for (int i=0; i<256; i++) gammaTable[i] = 255*pow((i/256.0),1.6);

	// clear framebuffer
	memset (framebuffer, 0, sizeof(framebuffer));

	// testpattern,
	framebuffer[0] =    0x00000050;
	framebuffer[31] =   0x00005000;
	framebuffer[992] =  0x00500000;
	framebuffer[1023] = 0x00505000;

	// display test pattern for 500 frames
	for (int i=0; i<500; i++){
		displayBuffer(framebuffer);
	}

	int frame = 0;
	while(1){
		displayBuffer(framebuffer);
		if (++frame % 5 == 0) randomizeFramebuffer(framebuffer);
		if (frame % 200 == 0) {
			while (UartReady != SET){}
			UartReady = RESET;
			if(HAL_UART_Transmit_IT(&uartHandle, uartAliveMsg, 17)!= HAL_OK) { }
		}
	}
}

/**
 * Displays the buffer on the display using binary encoding (PWM equivalent).
 */
void displayBuffer(uint32_t buffer[]) {
	for (int s=0; s<scan; s++){
		setRow(s);
		int offset1 = MATRIX_WIDTH * s;
		int offset2 = MATRIX_WIDTH * (s+scan);
		for (int plane=0; plane < 8; plane ++) {
			for (int x=0; x<MATRIX_WIDTH; x++) {
				setRGB(buffer[offset1+x], buffer[offset2+x], plane);
				CLK_TOGGLE;
			}
			showLine(waits[plane]);
		}
	}
}

/**
 * generates some random junk for testing on the framebuffer.
 */
uint8_t counter;
void randomizeFramebuffer(uint32_t buffer[]) {
	counter ++; // yes, it will overflow

	int max = gammaTable[counter];
	if (max == 0) max = 1;

	for (int i = 0; i < MATRIX_SIZE; i++) {
		buffer[i] = 0x00
				| ((gammaTable[rand() % max]) << 0)
				| ((gammaTable[rand() % max]) << 8)
				| ((gammaTable[rand() % max]) << 16);
	}
}


/**
 * sets the row on the row gpio ports
 */
void setRow(int row) {

	// todo: perhaps a lookup table could give us a tiny boost here.

	if (row & 0b0001) GPIOB->BSRRL = GPIO_PIN_5;
	else GPIOB->BSRRH = GPIO_PIN_5;

	if (row & 0b0010) GPIOB->BSRRL = GPIO_PIN_6;
	else GPIOB->BSRRH = GPIO_PIN_6;

	if (row & 0b0100) GPIOB->BSRRL = GPIO_PIN_8;
	else GPIOB->BSRRH = GPIO_PIN_8;

	if (row & 0b1000) GPIOB->BSRRL = GPIO_PIN_9;
	else GPIOB->BSRRH = GPIO_PIN_9;
}


/**
 * loads rgb1 and rgb2 gpio ports with the given bitplane
 */
void setRGB(uint32_t rgb1, uint32_t rgb2, uint8_t plane ){
	// TODO: this is quite a good candidate for performance
	// TODO: optimizations. if you want to drive more chained
	// TODO: panels, start tweaking this code.

	if (rgb1 & (1 << plane))        GPIOA->BSRRL = GPIO_PIN_5;
	else                            GPIOA->BSRRH = GPIO_PIN_5;

	if (rgb1 & (1 << (plane + 8))) 	GPIOA->BSRRL = GPIO_PIN_6;
	else                            GPIOA->BSRRH = GPIO_PIN_6;

	if (rgb1 & (1 << (plane + 16))) GPIOA->BSRRL = GPIO_PIN_7;
	else                            GPIOA->BSRRH = GPIO_PIN_7;

	if (rgb2 & (1 << plane))        GPIOA->BSRRL = GPIO_PIN_8;
	else                            GPIOA->BSRRH = GPIO_PIN_8;

	if (rgb2 & (1 << (plane + 8))) 	GPIOA->BSRRL = GPIO_PIN_9;
	else                            GPIOA->BSRRH = GPIO_PIN_9;

	if (rgb2 & (1 << (plane + 16))) GPIOA->BSRRL = GPIO_PIN_10;
	else                            GPIOA->BSRRH = GPIO_PIN_10;
}

/**
 * strobes / shows a line for a n*nop amount of time.
 */
void showLine(int amount) {
	STROBE;
	DISP_ON;
	for (int c=0; c<amount; c++) asm("nop");
	DISP_OFF;
}

/**
 * IRQ Handlers.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uartHandle) { UartReady = SET; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uartHandle) { UartReady = SET; }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uartHandle) {}
void USART6_IRQHandler(void) { HAL_UART_IRQHandler(&uartHandle); }
