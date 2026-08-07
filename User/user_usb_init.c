#include "apm32f4xx.h"
#include "tx_api.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "apm32f4xx_misc.h"
#include "apm32f4xx_usb_device.h"
#include "usb_config.h"
#include "usb_dc.h"
#include "user_usb_init.h"


/**@} end of group OTGD_CDC_HS2_Variables*/

/** @defgroup OTGD_CDC_HS2_Functions Functions
  @{
  */

/*!
 * @brief       Init USB hardware
 *
 * @param       usbInfo:usb handler information
 *
 * @retval      None
 */
void USBD_HardwareInit(uint8_t busid)
{
    GPIO_Config_T gpioConfig;
    
//    #if defined(CONFIG_USB_FS)
//        RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SYSCFG);
//        /* Configure USB OTG */
//        RCM_EnableAHB2PeriphClock(RCM_AHB2_PERIPH_OTG_FS);
//    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_OTG_HS);
//        /* Configure USB OTG GPIO */
//        RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOA);

//        GPIO_ConfigPinAF(GPIOA, GPIO_PIN_SOURCE_11, GPIO_AF_OTG1_FS);
//        GPIO_ConfigPinAF(GPIOA, GPIO_PIN_SOURCE_12, GPIO_AF_OTG1_FS);

//        /* USB DM, DP pin configuration */
//        gpioConfig.mode = GPIO_MODE_AF;
//        gpioConfig.speed = GPIO_SPEED_100MHz;
//        gpioConfig.otype = GPIO_OTYPE_PP;
//        gpioConfig.pupd = GPIO_PUPD_NOPULL;
//        gpioConfig.pin = GPIO_PIN_11 | GPIO_PIN_12;
//        GPIO_Config(GPIOA, &gpioConfig);

//        /* NVIC */
//        NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_4);
//        NVIC_EnableIRQRequest(OTG_FS_IRQn, 1, 0);

//        /* Disable USB OTG all global interrupt */
////        USB_OTG_DisableAllGlobalInterrupt(USB_OTG_FS);

//    #elif defined(CONFIG_USB_HS)
         /* Configure USB OTG*/
        RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SYSCFG);
        
        RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_OTG_HS);
//        RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_OTG_HS_ULPI);
        
        /* Configure USB OTG GPIO */
        RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOB);

        GPIO_ConfigPinAF(GPIOB, GPIO_PIN_SOURCE_14, GPIO_AF_OTG_HS_FS);
        GPIO_ConfigPinAF(GPIOB, GPIO_PIN_SOURCE_15, GPIO_AF_OTG_HS_FS);

        /* USB DM, DP pin configuration */
        gpioConfig.mode = GPIO_MODE_AF;
        gpioConfig.speed = GPIO_SPEED_100MHz;
        gpioConfig.otype = GPIO_OTYPE_PP;
        gpioConfig.pupd = GPIO_PUPD_NOPULL;
        gpioConfig.pin = GPIO_PIN_14 | GPIO_PIN_15;
        GPIO_Config(GPIOB, &gpioConfig);
        
        // use hs2
        USB_OTG_HS2->USB_SWITCH_B.usb_switch = BIT_SET;
        USB_OTG_HS2->POWERON_CORE_B.poweron_core = BIT_SET;
        USB_OTG_HS2->OTG_SUSPENDM_B.otg_suspendm = BIT_SET;
        USB_OTG_HS2->SW_RREF_I2C_B.sw_rref_i2c = 0x05;

        /* NVIC */
        NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_4);
        NVIC_EnableIRQRequest(OTG_HS1_IRQn, 1, 0);

        /* Disable USB OTG all global interrupt */
//        USB_OTG_DisableAllGlobalInterrupt(USB_OTG_HS);

//    #else
//    /* code */
//    #endif
}

/*!
 * @brief       Reset USB hardware
 *
 * @param       usbInfo:usb handler information
 *
 * @retval      None
 */
void USBD_HardwareReset(uint8_t busid)
{
//    #if defined(CONFIG_USB_FS)
//        RCM_DisableAHB2PeriphClock(RCM_AHB2_PERIPH_OTG_FS);
//        
//        NVIC_DisableIRQRequest(OTG_FS_IRQn);

//    #elif defined(CONFIG_USB_HS)
        RCM_DisableAHB1PeriphClock(RCM_AHB1_PERIPH_OTG_HS);
        
        NVIC_DisableIRQRequest(OTG_HS1_IRQn);

//    #else
//    /* code */
//    #endif
}


void usb_dc_low_level_init(uint8_t busid)
{
    USBD_HardwareInit(busid);
}

void usb_dc_low_level_deinit(uint8_t busid)
{
    USBD_HardwareReset(busid);
}

uint32_t usbd_get_dwc2_gccfg_conf(uint32_t reg_base) 
{
    return ((1 << 16) | (1 << 21));
}

void usbd_dwc2_delay_ms(uint8_t ms)
{
    tx_thread_sleep(ms);
}

void OTG_FS_IRQHandler(void)
{
    USBD_IRQHandler(0);
}

void OTG_HS1_IRQHandler(void)
{
    USBD_IRQHandler(0);
}


void usb_event_handler(uint8_t busid, uint8_t event) {
    switch (event)
    {
        case USBD_USER_RESET:
            break;

        case USBD_USER_RESUME:
            break;

        case USBD_USER_SUSPEND:
            break;

        case USBD_USER_CONNECT:
            break;

        case USBD_USER_DISCONNECT:
            break;

        case USBD_USER_ERROR:
            break;

        default:
            break;
    }
}

