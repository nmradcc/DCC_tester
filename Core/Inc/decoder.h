
#ifdef __cplusplus
extern "C" {
#endif

void DC_HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

void DecoderThread_Init(void);
void DecoderThread_Start(void);

#ifdef __cplusplus
}
#endif
