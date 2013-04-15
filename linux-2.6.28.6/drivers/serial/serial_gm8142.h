#ifndef _SERIAL_GM8142_H_
#define _SERIAL_GM8142_H_

#define CLK_FREQ 			1000000		//SPI_SPEED
#define G_PCLK				66000000	//PCLK

#define S3C6410_PCLK    	0x7E00F034
#define S3C6410_SCLK    	0x7E00F038
#define S3C6410_SPI     	0x7F00B000

//SPI configuration register----CH_CFG 	p1057
#define HIGH_SPEED_EN  		(0x00<<6)	//0:disable 1:enable
#define SW_RST    			(0x00<<5)	//0:inactive, 1:active, SW reset, RX/TX fifo SPI status will clear
#define MASTER   			(0x00<<4)	//0:Master, 1:Slave
#define SLAVE				(0x01<<4)	//0:Master, 1:Slave
#define CPOL    			(0x00<<3)	//0:active high 1:active low
#define CPHA   	 			(0x00<<2)	//0:Format A 1:Format B

//Clock configuration register----CLK_CFG	p1058
#define SPI_CLKSEL   		(0x00<<9)	//00:PCLK, 01:USBCLK, 10:EPLL clk
#define ENCLK    			(0x01<<8)	//1 enable clk 0:disable
#define SPI_SCALER   		(0x20<<0)	//(u8)(((float)G_PCLK)/(float)2/(float)CLK_FREQ - (float)1);

//SPI FIFO control register----MODE_CFG		P1059
#define CH_WIDTH   			(0x00<<29)	//00byte, 01 half word 10 word, 11 reserves
#define TRAILING_CNT  		(0x00<<19)	//Count value from writing the last data in RX FIFO to flush trailing bytes in FIFO
#define BUS_WIDTH   		(0x00<<17)	//00byte, 01 half word 10 word, 11 reserves
#define RX_RDY_LVL   		(0x01<<11)	//RX FIFO trigger mode. 0-63bytes
#define TX_RDY_LVL   		(0x01<<5)	//TX FIFO trigger mode, 0-63 bytes
#define RX_DMA_SW   		(0x00<<2)	//RX dma disable
#define TX_DMA_SW   		(0x00<<1)	//TX dma disable
#define DMA_TYPE   			(0x00<<0)	//0:single, 1:4 burst

//Slave selection signal control register-----CS_REG p1060
#define	SLAVE_CS_LOW		(0X00<<0)
#define	SLAVE_CS_HIGH		(0x01<<0)
#define	SLAVE_AUTO_OFF		(0x00<<1)
#define	SLAVE_AUTO_ON		(0x01<<1)

//SPI Interrupt Enable register --------SPI_INT_EN	P1061
#define	SPI_INT_ENABLE		(0x00<<0)

//SWAP config register----- SWAP_CFG p1065
#define RX_SWAP_EN   		(0x00<<4)	//1:Enable 0:disable
#define RX_HALF_WORD_SWAP 	(0x01<<7)
#define RX_BYTE_SWAP  		(0x01<<6)
#define RX_BIT_SWAP   		(0x01<<5)
#define RX_SWAP_MODE  		 RX_BIT_SWAP
#define TX_SWAP_EN   		(0x00<<0)
#define TX_BIT_SWAP   		(0x01<<1)
#define TX_BYTE_SWAP  		(0x01<<2)
#define TX_HALF_WORD_SWAP 	(0x01<<3)
#define TX_SWAP_MODE  		 TX_BIT_SWAP

//GM8142 BAUD SET   --7.3728MHZ
#define GM_8142_B115200			(0x0a)
#define GM_8142_B57600			(0x0b)
#define GM_8142_B9600			(0x04)
#define GM_8142_B4800			(0x05)
#define GM_8142_B1200			(0x07)
 
#endif 
