
#ifdef __cplusplus
extern "C" {
#endif

void CS_HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

void CommandStationThread_Init(void);
void CommandStationThread_Start(void);

#ifdef __cplusplus
}
#endif
