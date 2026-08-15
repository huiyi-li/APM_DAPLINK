#ifndef BOARD_H
#define BOARD_H

#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"

/* Status LEDs, active high. */
#define BOARD_LED_USB_PORT              GPIOE
#define BOARD_LED_USB_PIN               GPIO_PIN_0
#define BOARD_LED_USB_PIN_SOURCE        GPIO_PIN_SOURCE_0
#define BOARD_LED_SWD_PORT              GPIOE
#define BOARD_LED_SWD_PIN               GPIO_PIN_1
#define BOARD_LED_SWD_PIN_SOURCE        GPIO_PIN_SOURCE_1
#define BOARD_LED_CDC_PORT              GPIOE
#define BOARD_LED_CDC_PIN               GPIO_PIN_2
#define BOARD_LED_CDC_PIN_SOURCE        GPIO_PIN_SOURCE_2
#define BOARD_LED_GPIO_CLOCK            RCM_AHB1_PERIPH_GPIOE

/* User buttons, active low with internal pull-ups. */
#define BOARD_BUTTON_0_PORT             GPIOE
#define BOARD_BUTTON_0_PIN              GPIO_PIN_3
#define BOARD_BUTTON_1_PORT             GPIOE
#define BOARD_BUTTON_1_PIN              GPIO_PIN_4
#define BOARD_BUTTON_2_PORT             GPIOE
#define BOARD_BUTTON_2_PIN              GPIO_PIN_5
#define BOARD_BUTTON_GPIO_CLOCK         RCM_AHB1_PERIPH_GPIOE

/* W25Q64 on SPI1. */
#define BOARD_FLASH_CS_PORT             GPIOA
#define BOARD_FLASH_CS_PIN              GPIO_PIN_4
#define BOARD_FLASH_SCK_PORT            GPIOA
#define BOARD_FLASH_SCK_PIN             GPIO_PIN_5
#define BOARD_FLASH_SCK_PIN_SOURCE      GPIO_PIN_SOURCE_5
#define BOARD_FLASH_MISO_PORT           GPIOA
#define BOARD_FLASH_MISO_PIN            GPIO_PIN_6
#define BOARD_FLASH_MISO_PIN_SOURCE     GPIO_PIN_SOURCE_6
#define BOARD_FLASH_MOSI_PORT           GPIOA
#define BOARD_FLASH_MOSI_PIN            GPIO_PIN_7
#define BOARD_FLASH_MOSI_PIN_SOURCE     GPIO_PIN_SOURCE_7
#define BOARD_FLASH_GPIO_CLOCK          RCM_AHB1_PERIPH_GPIOA

/* Debug console on USART1. */
#define BOARD_DEBUG_TX_PORT             GPIOA
#define BOARD_DEBUG_TX_PIN              GPIO_PIN_9
#define BOARD_DEBUG_TX_PIN_SOURCE       GPIO_PIN_SOURCE_9
#define BOARD_DEBUG_RX_PORT             GPIOA
#define BOARD_DEBUG_RX_PIN              GPIO_PIN_10
#define BOARD_DEBUG_RX_PIN_SOURCE       GPIO_PIN_SOURCE_10
#define BOARD_DEBUG_GPIO_CLOCK          RCM_AHB1_PERIPH_GPIOA

/* USB CDC bridge on USART3. */
#define BOARD_CDC_UART_TX_PORT          GPIOB
#define BOARD_CDC_UART_TX_PIN           GPIO_PIN_10
#define BOARD_CDC_UART_TX_PIN_SOURCE    GPIO_PIN_SOURCE_10
#define BOARD_CDC_UART_RX_PORT          GPIOB
#define BOARD_CDC_UART_RX_PIN           GPIO_PIN_11
#define BOARD_CDC_UART_RX_PIN_SOURCE    GPIO_PIN_SOURCE_11
#define BOARD_CDC_UART_GPIO_CLOCK       RCM_AHB1_PERIPH_GPIOB

/* Target SWD interface. */
#define BOARD_TARGET_RESET_PORT         GPIOB
#define BOARD_TARGET_RESET_PIN          GPIO_PIN_0
#define BOARD_TARGET_RESET_PIN_SOURCE   GPIO_PIN_SOURCE_0
#define BOARD_TARGET_SWCLK_PORT         GPIOB
#define BOARD_TARGET_SWCLK_PIN          GPIO_PIN_13
#define BOARD_TARGET_SWCLK_PIN_SOURCE   GPIO_PIN_SOURCE_13
#define BOARD_TARGET_SWDIO_PORT         GPIOC
#define BOARD_TARGET_SWDIO_PIN          GPIO_PIN_3
#define BOARD_TARGET_SWDIO_PIN_SOURCE   GPIO_PIN_SOURCE_3

/* LCD control signals: software SPI bit-bang bus. */
#define BOARD_LCD_DC_PORT               GPIOC
#define BOARD_LCD_DC_PIN                GPIO_PIN_9
#define BOARD_LCD_DC_PIN_SOURCE         GPIO_PIN_SOURCE_9
#define BOARD_LCD_SCK_PORT              GPIOC
#define BOARD_LCD_SCK_PIN               GPIO_PIN_10
#define BOARD_LCD_SCK_PIN_SOURCE        GPIO_PIN_SOURCE_10
#define BOARD_LCD_MOSI_PORT             GPIOC
#define BOARD_LCD_MOSI_PIN              GPIO_PIN_12
#define BOARD_LCD_MOSI_PIN_SOURCE       GPIO_PIN_SOURCE_12
#define BOARD_LCD_RESET_PORT            GPIOC
#define BOARD_LCD_RESET_PIN             GPIO_PIN_13
#define BOARD_LCD_RESET_PIN_SOURCE      GPIO_PIN_SOURCE_13
#define BOARD_LCD_CS_PORT               GPIOA
#define BOARD_LCD_CS_PIN                GPIO_PIN_15
#define BOARD_LCD_CS_PIN_SOURCE         GPIO_PIN_SOURCE_15
#define BOARD_LCD_GPIO_CLOCK            RCM_AHB1_PERIPH_GPIOC
#define BOARD_LCD_CS_GPIO_CLOCK         RCM_AHB1_PERIPH_GPIOA

/* USB OTG HS controller with internal FS PHY. */
#define BOARD_USB_DM_PORT               GPIOB
#define BOARD_USB_DM_PIN                GPIO_PIN_14
#define BOARD_USB_DP_PORT               GPIOB
#define BOARD_USB_DP_PIN                GPIO_PIN_15
#define BOARD_USB_GPIO_CLOCK            RCM_AHB1_PERIPH_GPIOB

#endif
