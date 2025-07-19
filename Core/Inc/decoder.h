
#ifdef __cplusplus
extern "C" {
#endif

void DC_HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

void Decoder_Init(void);
void Decoder_Start(void);
void Decoder_Stop(void);

#ifdef __cplusplus
}
#endif
