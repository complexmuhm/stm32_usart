#include "cmsis/stm32f1xx.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
using uint8_t = unsigned char;
using uint16_t = unsigned short;

volatile uint32_t ticks;
extern "C" void SysTick_Handler() { ++ticks; }

uint32_t SystemCoreClock;
extern "C" void SystemInit()
{
    // Conf clock : 72MHz using HSE 8MHz crystal w/ PLL X 9 (8MHz x 9 = 72MHz)
    FLASH->ACR      |= FLASH_ACR_LATENCY_2; // Two wait states, per datasheet
    RCC->CFGR       |= RCC_CFGR_PPRE1_2;    // prescale AHB1 = HCLK/2
    RCC->CR         |= RCC_CR_HSEON;        // enable HSE clock
    while( !(RCC->CR & RCC_CR_HSERDY) );    // wait for the HSEREADY flag
    
    RCC->CFGR       |= RCC_CFGR_PLLSRC;     // set PLL source to HSE
    RCC->CFGR       |= RCC_CFGR_PLLMULL9;   // multiply by 9 
    RCC->CR         |= RCC_CR_PLLON;        // enable the PLL 
    while( !(RCC->CR & RCC_CR_PLLRDY) );    // wait for the PLLRDY flag
    
    RCC->CFGR       |= RCC_CFGR_SW_PLL;     // set clock source to pll

    while( !(RCC->CFGR & RCC_CFGR_SWS_PLL) );    // wait for PLL to be CLK
    SystemCoreClock = 72000000;

    ticks = 0;
    SysTick->CTRL = 0b11; // Clocksource is AHB/8 and enable counter
    SysTick->LOAD = 8999; // SysTick interrupts now every millisecond
}

void delay(uint32_t ms)
{
    uint32_t now = ticks;
    while((ticks - now) < ms);
}


void init()
{
    RCC->APB2ENR = 
        RCC_APB2ENR_AFIOEN |
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_IOPBEN |
        RCC_APB2ENR_IOPCEN |
        RCC_APB2ENR_IOPDEN |
        RCC_APB2ENR_IOPEEN |
        RCC_APB2ENR_USART1EN;
    RCC->APB1ENR = 
        RCC_APB1ENR_TIM2EN |
        RCC_APB1ENR_I2C1EN;

    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
}

void init_gpio()
{
    // Setup PC13
    GPIOC->CRH = 0x00700000;
}

void init_usart()
{
    // PA9 TX, full duplex AF push pull
    // PA10 RX, full duplex Input floating / Input pull-up
    GPIOA->CRH = 0x000004B0;

    // USART
    // Page 792
    // Set the M bits to 8
    USART1->CR1 = 0;
    // Program the number of stop bits, 1 in our case
    USART1->CR2 = 0;
    // DMA enable in USART_CR3 if Multi buffer Communication
    // Select the baud rate using the USART_BRR register
    //USART1->BRR = 0x271; // Baud rate of 115.2kbps at 72 MHz
    USART1->BRR = 0x10; // Baud rate of 4500kbps at 72 MHz
}

void send_string(const char *str_)
{
    const char *str = str_;
    // Enable USART1
    SET_BIT(USART1->CR1, USART_CR1_UE);
    // Set the TE bit to send and idle frame
    SET_BIT(USART1->CR1, USART_CR1_TE);
    
    // Now we send out string
    // TXE bit ist reset when writing to the USART_DR register
    while(*str != 0)
    {
        USART1->DR = *str;
        // When TXE=1 then we can write to the USART_DR register again
        // Now wait until the contents have been transferred to the shift register
        while(!READ_BIT(USART1->SR, USART_SR_TXE)) {}
        ++str;
    }
    
    // After writing the last data into the USART_DR register, wait until TC=1
    while(!READ_BIT(USART1->SR, USART_SR_TC)) {}
    // Disable USART now
    CLEAR_BIT(USART1->CR1, USART_CR1_UE);
}

char* itoa(int val)
{
    const int base = 10;
    static char buf[32] = {0};
    int i = 30;

    for(; val && i ; --i, val /= base)
        buf[i] = "0123456789abcdef"[val % base];

    return &buf[i+1];
}

int main(void) 
{
    init();
    init_gpio();
    init_usart();
    
    while(1)
    {
        send_string("It works.");
        SET_BIT(GPIOC->BSRR, GPIO_BSRR_BR13);
        delay(500);
        SET_BIT(GPIOC->BSRR, GPIO_BSRR_BS13);
        delay(500);
    }
}

