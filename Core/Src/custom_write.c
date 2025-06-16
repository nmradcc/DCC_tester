#include "stm32h5xx_hal.h"


// Function to write data to UART
int _write(int file, char *data, int len)
{
    (void)(file);
    // Transmit data using UART
    for (int i = 0; i < len; i++)
    {
        // Send the character
        USART3->TDR = (uint16_t)data[i];
        // Wait for the transmit buffer to be empty
        while (!(USART3->ISR & USART_ISR_TXE));
    }
    return len;
}

