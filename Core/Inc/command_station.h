
#ifdef __cplusplus
extern "C" {
#endif

void CS_HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

void CommandStation_Init(void);
void CommandStation_Start(void);
void CommandStation_Stop(void);


#ifdef __cplusplus
}
#endif
