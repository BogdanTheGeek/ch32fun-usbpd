//------------------------------------------------------------------------------
//       Filename: main.c
//------------------------------------------------------------------------------
//       Bogdan Ionescu (c) 2024
//------------------------------------------------------------------------------
//       Purpose : Application entry point
//------------------------------------------------------------------------------
//       Notes :
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Module includes
//------------------------------------------------------------------------------
#include "ch32fun.h"
#include "log.h"
#include <inttypes.h>
#include <stdbool.h>

#define USBPD_IMPLEMENTATION
#include "usbpd.h"

//------------------------------------------------------------------------------
// Module constant defines
//------------------------------------------------------------------------------
#define TAG "main"

#define INTERNAL_VREF    1200
#define ADC_RESOLUTION   12
#define ADC_MAX          ((1 << ADC_RESOLUTION) - 1)
#define ADC_CHANNELS     15
#define ADC_VREF_CHANNEL 15

//------------------------------------------------------------------------------
// External variables
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// External functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Module type definitions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Module static variables
//------------------------------------------------------------------------------
static volatile uint32_t s_systickCount = 0;
static uint8_t newByte = 0;
static uint32_t count = 0;
static uint32_t countLast = 0;

//------------------------------------------------------------------------------
// Module static function prototypes
//------------------------------------------------------------------------------
static void SysTick_Init(void);
static void WDT_Init(uint16_t reload_val, uint8_t prescaler);
static void WDT_Pet(void);

//------------------------------------------------------------------------------
// Module externally exported functions
//------------------------------------------------------------------------------

/**
 * @brief  Debugger input handler
 * @param  numbytes - the number of bytes received
 * @param  data - the data (8 bytes)
 * @return None
 */
void handle_debug_input(int numbytes, uint8_t *data)
{
    newByte = data[0];
    count += numbytes;
}

/**
 * @brief  Get the next character from the debugger
 * @param  None
 * @return the next character or -1 if no character is available
 * @note   This function will block for up to 100ms waiting for a character
 */
int getchar(void)
{
    const uint32_t timeout = 100;
    const uint32_t end = s_systickCount + timeout;
    while (count == countLast && (s_systickCount < end))
    {
        poll_input();
        putchar(0);
    }

    if (count == countLast)
    {
        return -1;
    }

    countLast = count;
    return newByte;
}

/**
 * @brief  Application entry point
 * @param  None
 * @return None
 */
int main(void)
{
    SystemInit();

    const bool debuggerAttached = !WaitForDebuggerToAttach(1000);
    if (debuggerAttached)
    {
        LOG_Init(eLOG_LEVEL_DEBUG, (uint32_t *)&s_systickCount);
    }
    else
    {
        LOG_Init(eLOG_LEVEL_NONE, (uint32_t *)&s_systickCount);
    }

    SysTick_Init();
    LOGI(TAG, "System started");

    RCC->APB2PCENR |= (RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA);

    USBPD_VCC_e vcc = eUSBPD_VCC_5V0;

#if CHECK_VCC
    funAnalogInit();
    funPinMode(PA0, GPIO_CNF_IN_ANALOG); // VBUS

    const uint32_t vref = funAnalogRead(ADC_VREF_CHANNEL);
    const uint32_t vdd = (INTERNAL_VREF * ADC_MAX) / vref;
    LOGI(TAG, "VDD: %d mV", vdd);

    if (vdd < 4000)
    {
        vcc = eUSBPD_VCC_3V3;
    }
#endif

    USBPD_Result_e result = USBPD_Init(vcc);
    if (eUSBPD_OK != result)
    {
        LOGE(TAG, "USB PD init failed: %s", USBPD_ResultToStr(result));
        while (1)
            ;
    }

    LOGW(TAG, "You can press 'r' to reset USB PD negotiation, 'q' to reset the board");

loop_start:

    USBPD_Reset();

    LOGI(TAG, "USB PD init done");

    const uint32_t start = s_systickCount;
    const uint32_t timeout = 20000;
    uint32_t lastLog = 0;

    while (eUSBPD_BUSY == (result = USBPD_SinkNegotiate()))
    {
        if ((s_systickCount - start) > timeout)
        {
            LOGE(TAG, "USB PD negotiation timed out");
            break;
        }

        if (s_systickCount > (lastLog + 1000))
        {
            LOGD(TAG, "Negotiating USB PD: %s", USBPD_StateToStr(USBPD_GetState()));
            lastLog = s_systickCount;
        }

        if (!debuggerAttached) continue;

        switch (getchar())
        {
            case 'r':
                LOGI(TAG, "Resetting USB PD negotiation");
                goto loop_start;
            case 'q':
                NVIC_SystemReset();
            case -1:
                break;
            default:
                break;
        }
    }

    if (eUSBPD_OK != result)
    {
        LOGE(TAG, "USB PD negotiation failed: %s, state: %s", USBPD_ResultToStr(result), USBPD_StateToStr(USBPD_GetState()));
    }
    else
    {
        LOGI(TAG, "USB PD V%d.0 negotiation done", USBPD_GetVersion());

        USBPD_SPR_CapabilitiesMessage_t *capabilities;
        const size_t count = USBPD_GetCapabilities(&capabilities);

        LOGI(TAG, "USB PD capabilities:");
        for (size_t i = 0; i < count; i++)
        {
            const USBPD_SourcePDO_t *pdo = &capabilities->Source[i];
            switch (pdo->Header.PDOType)
            {
                case eUSBPD_PDO_FIXED:
                    LOGI(TAG, "%d: " FIXED_SUPPLY_FMT, i, FIXED_SUPPLY_FMT_ARGS(pdo));
                    break;
                case eUSBPD_PDO_BATTERY:
                    LOGI(TAG, "%d: " BATTERY_SUPPLY_FMT, i, BATTERY_SUPPLY_FMT_ARGS(pdo));
                    break;
                case eUSBPD_PDO_VARIABLE:
                    LOGI(TAG, "%d: " VARIABLE_SUPPLY_FMT, i, VARIABLE_SUPPLY_FMT_ARGS(pdo));
                    break;
                case eUSBPD_PDO_AUGMENTED:
                    switch (pdo->Header.AugmentedType)
                    {
                        case eUSBPD_APDO_SPR_PPS:
                            LOGI(TAG, "%d: " SPR_PPS_FMT, i, SPR_PPS_FMT_ARGS(pdo));
                            break;
                        case eUSBPD_APDO_SPR_AVS:
                            LOGI(TAG, "%d: " SPR_AVS_FMT, i, SPR_AVS_FMT_ARGS(pdo));
                            break;
                        case eUSBPD_APDO_EPR_AVS:
                            LOGI(TAG, "%d: " EPR_AVS_FMT, i, EPR_AVS_FMT_ARGS(pdo));
                            break;
                        default:
                            LOGE(TAG, "  Unknown Augmented PDO type: %d", pdo->Header.AugmentedType);
                            break;
                    }
                    break;
                default:
                    LOGE(TAG, "  Unknown PDO type: %d", pdo->Header.PDOType);
                    break;
            }
        }
        LOGW(TAG, "Cycling though PDO");
        for (size_t i = 0; i < count; i++)
        {
            const USBPD_SourcePDO_t *pdo = &capabilities->Source[i];
            LOGI(TAG, "Selecting PDO %d", i);
            if (USBPD_IsPPS(pdo))
            {
                for (uint32_t voltage = pdo->SPR_PPS.MinVoltageIn100mV;
                     voltage <= pdo->SPR_PPS.MaxVoltageIn100mV;
                     voltage += 10)
                {
                    LOGI(TAG, "Setting PPS voltage to %d mV", voltage * 10);
                    USBPD_SelectPDO(i, voltage);
                    Delay_Ms(1000);
                }
            }
            else
            {
                USBPD_SelectPDO(i, 0);
            }
            Delay_Ms(1000);
        }
    }

    Delay_Ms(3000);

    goto loop_start;
}

//------------------------------------------------------------------------------
// Module static functions
//------------------------------------------------------------------------------

/**
 * @brief  Enable the SysTick module
 * @param  None
 * @return None
 */
static void SysTick_Init(void)
{
    // Disable default SysTick behavior
    SysTick->CTLR = 0;

    // Enable the SysTick IRQ
    NVIC_EnableIRQ(SysTicK_IRQn);

    uint64_t CNT = SysTick->CNTL | ((uint64_t)SysTick->CNTH << 32);
    CNT += (FUNCONF_SYSTEM_CORE_CLOCK / 1000) - 1; // 1ms tick

    // Trigger an interrupt in 1ms
    SysTick->CMPL = (uint32_t)(CNT & 0xFFFFFFFF);
    SysTick->CMPH = (uint32_t)(CNT >> 32);

    // Start at zero
    s_systickCount = 0;

    // Enable SysTick counter, IRQ, HCLK/1
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
}

/**
 * @brief  Initialize the watchdog timer
 * @param reload_val - the value to reload the counter with
 * @param prescaler - the prescaler to use
 * @return None
 */
static void WDT_Init(uint16_t reload_val, uint8_t prescaler)
{
    IWDG->CTLR = 0x5555;
    IWDG->PSCR = prescaler;

    IWDG->CTLR = 0x5555;
    IWDG->RLDR = reload_val & 0xfff;

    IWDG->CTLR = 0xCCCC;
}

/**
 * @brief  Pet the watchdog timer
 * @param  None
 * @return None
 */
static void WDT_Pet(void)
{
    IWDG->CTLR = 0xAAAA;
}

/**
 * @brief  SysTick interrupt handler
 * @param  None
 * @return None
 * @note   __attribute__((interrupt)) syntax is crucial!
 */
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    uint64_t CNT = SysTick->CNTL | ((uint64_t)SysTick->CNTH << 32);
    CNT += (FUNCONF_SYSTEM_CORE_CLOCK / 1000) - 1; // 1ms tick

    // Trigger an interrupt in 1ms
    SysTick->CMPL = (uint32_t)(CNT & 0xFFFFFFFF);
    SysTick->CMPH = (uint32_t)(CNT >> 32);

    // Clear IRQ
    SysTick->SR = 0;

    // Update counter
    s_systickCount++;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
