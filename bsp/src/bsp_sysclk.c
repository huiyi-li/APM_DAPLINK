#include "bsp_sysclk.h"
#include "apm32f4xx_rcm.h"
#include "apm32f4xx_fmc.h"
#include "system_apm32f4xx.h"

/*!
 * @brief     Configures the System clock frequency, HCLK, PCLK2 and PCLK1
 *
 * @param     None
 *
 * @retval    None
 */
void bsp_sysclk_init(void)
{
    RCM_Reset();

    /* Open HSE 24MHz */
    RCM_ConfigHSE(RCM_HSE_OPEN);

    if(RCM_WaitHSEReady() == SUCCESS)
    {
        FMC_EnablePrefetchBuffer();
        FMC_ConfigLatency(FMC_LTNCY_4);

        RCM_ConfigAHB(RCM_AHB_DIV_1);
        RCM_ConfigAPB2(RCM_APB_DIV_2);
        RCM_ConfigAPB1(RCM_APB_DIV_4);

        RCM_ConfigPLL1(RCM_PLLSEL_HSE, 24,336,RCM_PLL_SYS_DIV_2,7);
        RCM_EnablePLL1();

        /* Wait for PLL1 Ready */
        while(RCM_ReadStatusFlag(RCM_FLAG_PLL1RDY) == RESET);

        /* Select PLL1 as System Clock */
        RCM_ConfigSYSCLK(RCM_SYSCLK_SEL_PLL);
        while(RCM_ReadSYSCLKSource() != RCM_SYSCLK_SEL_PLL)
        {
        }

        /* PLL1: HSE 24MHz / 24 * 336 / 2 = 168MHz system clock.
         * Set explicitly because the ARMCC scatter-load copy of RW data
         * (including SystemCoreClock's initializer) is not present in the
         * programmed flash image on this board, so the variable would
         * otherwise contain 0xFF. */
        SystemCoreClock = 168000000U;
    }
    else
    {
        while(1);
    }
}
