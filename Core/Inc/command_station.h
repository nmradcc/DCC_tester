#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void CS_HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

void CommandStation_Init(void);
void CommandStation_Start(bool bidi);
void CommandStation_Stop(void);
bool CommandStation_bidi_Threshold(uint16_t threshold);


#ifdef __cplusplus
}
#endif
