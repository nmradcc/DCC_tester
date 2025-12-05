#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void CommandStation_Init(void);
void CommandStation_Start(bool loop);
void CommandStation_Stop(void);
bool CommandStation_bidi_Threshold(uint16_t threshold);


#ifdef __cplusplus
}
#endif
