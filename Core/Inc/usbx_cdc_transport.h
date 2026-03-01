#ifndef USBX_CDC_TRANSPORT_H
#define USBX_CDC_TRANSPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t UsbCdcAcm_Write(const uint8_t* data, uint32_t length, uint32_t* actual_length);
void UsbCdcAcm_GetStatus(uint32_t* device_configured, uint32_t* cdc_active);

#ifdef __cplusplus
}
#endif

#endif