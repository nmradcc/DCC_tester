#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CommandStation_Init(void);
void CommandStation_Start(uint8_t loop);  // loop: 0=no loop, 1=loop1, 2=loop2, 3=loop3
void CommandStation_Stop(void);
bool CommandStation_bidi_Threshold(uint16_t threshold);


#ifdef __cplusplus
}
#endif
