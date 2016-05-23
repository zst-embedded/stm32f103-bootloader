/* *****************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 LeafLabs LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/

/**
 *  @file hardware.c
 *
 *  @brief init routines to setup clocks, interrupts, also destructor functions.
 *  does not include USB stuff. EEPROM read/write functions.
 *
 */
#include "common.h"
#include "hardware.h"


static RCC_TypeDef *RCC = (RCC_TypeDef *)RCC_BASE;
static FLASH_TypeDef *FLASH = (FLASH_TypeDef *)FLASH_BASE;
static SCB_TypeDef *SCB = (SCB_TypeDef *)SCB_BASE;
static NVIC_TypeDef *NVIC = (NVIC_TypeDef *)NVIC_BASE;


/* system init shit */
void systemReset(void)
{
    /* TODO: say what this does */
    RCC->CR   |= 0x00000001;
    RCC->CFGR &= 0xF8FF0000;
    RCC->CR   &= 0xFEF6FFFF;
    RCC->CR   &= 0xFFFBFFFF;
    RCC->CFGR &= 0xFF80FFFF;
    
    /* disable all RCC interrupts */
    RCC->CIR   = 0x00000000;
}

void setupCLK(void)
{
	unsigned int StartUpCounter=0;
    
    // enable HSE
    RCC->CR |= 0x00010001;

    // and wait for it to come on
    while ((RCC->CR & 0x00020000) == 0);

    // enable flash prefetch buffer
    FLASH->ACR = 0x00000012;
	
    // Configure PLL
#ifdef XTAL12M
    RCC->CFGR |= 0x00110400; /* pll=72Mhz(x6),APB1=36Mhz,AHB=72Mhz */
#else
    RCC->CFGR |= 0x001D0400; /* pll=72Mhz(x9),APB1=36Mhz,AHB=72Mhz */
#endif

    // enable the pll
    RCC->CR |= 0x01000000;

#ifndef HSE_STARTUP_TIMEOUT
  #define HSE_STARTUP_TIMEOUT    ((unsigned int)0x0500) /*!< Time out for HSE start up */
#endif /* HSE_STARTUP_TIMEOUT */   

    StartUpCounter = HSE_STARTUP_TIMEOUT;
    while (((RCC->CR & 0x03000000) == 0) && --StartUpCounter);
	
	if (!StartUpCounter) {
		// HSE has not started. Try restarting the processor
		systemHardReset(); 
	}
	
    // Set SYSCLK as PLL
    RCC->CFGR |= 0x00000002;
    // and wait for it to come on
    while ((RCC->CFGR & 0x00000008) == 0); 

    // Enable All GPIO channels (A to E), AFIO clock; disable any other clocks
    RCC->APB2ENR = 0x0000007d;
    // disable JTAG pins
    REG_SET(AFIO_MAPR, AFIO_MAPR_SWJ_CFG_NO_JTAG_SW);
}


void setupLEDAndButton (void)
{ 
    // clear PA15, which is a now-disabled JTAG pin that is otherwise on because of JTAG hardware
    REG_SET(GPIO_CR(GPIOA, 15),
        (REG_GET(GPIO_CR(GPIOA, 15)) & crMask(15)) | CR_INPUT << CR_SHIFT(15));
    gpio_write_bit(GPIOA, 15, 0);

#if defined(BUTTON_BANK) && defined(BUTTON_PIN) && defined(BUTTON_ON_STATE)
    // configure activation button
    REG_SET(GPIO_CR(BUTTON_BANK, BUTTON_PIN),
        (GPIO_CR(BUTTON_BANK, BUTTON_PIN) & crMask(BUTTON_PIN)) | CR_INPUT_PU_PD << CR_SHIFT(BUTTON_PIN));
    gpio_write_bit(BUTTON_BANK, BUTTON_PIN, 1-BUTTON_ON_STATE);// set pulldown resistor in case there is no button.
#endif

#if defined(LED_PIN_K)
    // LED is connected to two pins, so set cathode side low
    REG_SET(GPIO_CR(LED_BANK, LED_PIN_K),
        (REG_GET(GPIO_CR(LED_BANK, LED_PIN_K)) & crMask(LED_PIN_K)) | CR_OUTPUT_PP << CR_SHIFT(LED_PIN_K));
    gpio_write_bit(LED_BANK, LED_PIN_K, 0);
#endif

    // configure LED
    REG_SET(GPIO_CR(LED_BANK, LED_PIN),
        (REG_GET(GPIO_CR(LED_BANK, LED_PIN)) & crMask(LED_PIN)) | CR_OUTPUT_PP << CR_SHIFT(LED_PIN));
}

void setupFLASH()
{
    // configure the HSI oscillator
    if ((RCC->CR & 0x01) == 0) {
        RCC->CR |= 0x01;
    }

    // wait for it to come on
    while ((RCC->CR & 0x02) == 0);
}

bool checkUserCode(u32 usrAddr)
{
    u32 sp = *(vu32 *) usrAddr;

    if ((sp & 0x2FFE0000) == 0x20000000) return (TRUE);
    
    return (FALSE);
}

void jumpToUser(u32 userAddr)
{
    // bootloaded reset vector
    u32 jumpAddr = *(vu32 *)(userAddr + 0x04); /* reset ptr in vector table */
    FuncPtr userMain = (FuncPtr)jumpAddr;

    // tear down all the dfu related setup
    // disable usb interrupts, clear them, turn off usb, set the disc pin
    // todo pick exactly what we want to do here, now its just a conservative
    flashLock();
    usbDsbISR();
    nvicDisableInterrupts();
	
#ifndef HAS_MAPLE_HARDWARE	
	usbDsbBus();
#endif

    // resets clocks and periphs, not core regs
    systemReset();

    // set user stack pointer
    __MSR_MSP(*(vu32 *)userAddr);

    // let's go
    userMain();                               
}

void nvicInit(NVIC_InitTypeDef *NVIC_InitStruct)
{
    u32 tmppriority = 0x00;
    u32 tmpreg      = 0x00;
    u32 tmpmask     = 0x00;
    u32 tmppre      = 0;
    u32 tmpsub      = 0x0F;

    /* Compute the Corresponding IRQ Priority --------------------------------*/
    tmppriority = (0x700 - (SCB->AIRCR & (u32)0x700)) >> 0x08;
    tmppre = (0x4 - tmppriority);
    tmpsub = tmpsub >> tmppriority;

    tmppriority = (u32)NVIC_InitStruct->NVIC_IRQChannelPreemptionPriority << tmppre;
    tmppriority |=  NVIC_InitStruct->NVIC_IRQChannelSubPriority & tmpsub;

    tmppriority = tmppriority << 0x04;
    tmppriority = ((u32)tmppriority) << ((NVIC_InitStruct->NVIC_IRQChannel & (u8)0x03) * 0x08);

    tmpreg = NVIC->IPR[(NVIC_InitStruct->NVIC_IRQChannel >> 0x02)];
    tmpmask = (u32)0xFF << ((NVIC_InitStruct->NVIC_IRQChannel & (u8)0x03) * 0x08);
    tmpreg &= ~tmpmask;
    tmppriority &= tmpmask;
    tmpreg |= tmppriority;

    NVIC->IPR[(NVIC_InitStruct->NVIC_IRQChannel >> 0x02)] = tmpreg;

    /* Enable the Selected IRQ Channels --------------------------------------*/
    NVIC->ISER[(NVIC_InitStruct->NVIC_IRQChannel >> 0x05)] =
        (u32)0x01 << (NVIC_InitStruct->NVIC_IRQChannel & (u8)0x1F);
}

void nvicDisableInterrupts()
{
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
    NVIC->ICPR[1] = 0xFFFFFFFF;

    REG_SET(STK_CTRL, 0x04); /* disable the systick, which operates separately from nvic */
}

void systemHardReset(void)
{
    

    /* Reset  */
    SCB->AIRCR = (u32)AIRCR_RESET_REQ;

    /*  should never get here */
    while (1) {
        asm volatile("nop");
    }
}

/* flash functions */
bool flashErasePage(u32 pageAddr)
{
    FLASH->CR = FLASH_CR_PER;

    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->AR = pageAddr;
    FLASH->CR = FLASH_CR_START | FLASH_CR_PER;
    while (FLASH->SR & FLASH_SR_BSY);

    /* todo: verify the page was erased */
    FLASH->CR = 0;

    return TRUE;
}

bool flashErasePages(u32 pageAddr, u16 n)
{
    while (n-- > 0) {
        if (!flashErasePage(pageAddr + wTransferSize * n)) {
            return FALSE;
        }
    }

    return TRUE;
}

bool flashWriteWord(u32 addr, u32 word)
{
    vu16 *flashAddr = (vu16 *)addr;
    vu32 lhWord = (vu32)word & 0x0000FFFF;
    vu32 hhWord = ((vu32)word & 0xFFFF0000) >> 16;

    FLASH->CR = FLASH_CR_PG;

    /* apparently we need not write to FLASH_AR and can
       simply do a native write of a half word */
    while (FLASH->SR & FLASH_SR_BSY);
    *(flashAddr + 0x01) = (vu16)hhWord;
    while (FLASH->SR & FLASH_SR_BSY);
    *(flashAddr) = (vu16)lhWord;
    while (FLASH->SR & FLASH_SR_BSY);

    FLASH->CR &= 0xFFFFFFFE;

    /* verify the write */
    if (*(vu32 *)addr != word) {
        return FALSE;
    }

    return TRUE;
}

void flashLock()
{
    /* take down the HSI oscillator? it may be in use elsewhere */

    /* ensure all FPEC functions disabled and lock the FPEC */
    FLASH->CR = 0x00000080;
}

void flashUnlock()
{
    /* unlock the flash */
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
}

#define FLASH_SIZE_REG 0x1FFFF7E0
int getFlashEnd(void)
{
    unsigned short *flashSize = (unsigned short *)FLASH_SIZE_REG;// Address register 
    return ((int)(*flashSize & 0xffff) * 1024) + 0x08000000;
}

int getFlashPageSize(void)
{

    unsigned short *flashSize = (unsigned short *)FLASH_SIZE_REG;// Address register 
    if ((*flashSize & 0xffff) > 128) {
        return 0x800;
    } else {
        return 0x400;
    }
}


/* gpio functions */
/**
 * Used to create the control register masking pattern, when setting control register mode.
 */
unsigned int crMask(int pin)
{
	unsigned int mask;
	if (pin>=8) {
		pin-=8;
	}

	mask = 0x0F << (pin << 2);
	return ~mask;
}	

void gpio_write_bit(u32 bank, u8 pin, u8 val) {
    val = !val;          // "set" bits are lower than "reset" bits  
    REG_SET(GPIO_BSRR(bank), (1U << pin) << (16 * val));
}

bool readPin(u32 bank, u8 pin) {
    // todo, implement read
    if (REG_GET(GPIO_IDR(bank)) & (0x01 << pin)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

bool readButtonState()
{
    bool state = FALSE;

#if defined(BUTTON_BANK) && defined (BUTTON_PIN) && defined (BUTTON_ON_STATE)   
    if (REG_GET(GPIO_IDR(BUTTON_BANK)) & (0x01 << BUTTON_PIN))  {
        state = TRUE;
    } 
    
    if (BUTTON_ON_STATE==0) {
        state = !state;
    }
#endif

    return state;
}

void strobePin(u32 bank, u8 pin, u8 count, u32 rate,u8 onState) 
{
    gpio_write_bit(bank, pin, 1-onState);

    u32 c;
    while (count-- > 0) {
        for (c = rate; c > 0; c--) {
            asm volatile("nop");
        }
        
        gpio_write_bit(bank, pin, onState);
        
        for (c = rate; c > 0; c--) {
            asm volatile("nop");
        }

        gpio_write_bit(bank, pin, 1 - onState);
    }
}