#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void RpcServer_Init(void);
void RpcServer_Start(bool test_mode);
void RpcServer_Stop(void);

#ifdef __cplusplus
}
#endif
