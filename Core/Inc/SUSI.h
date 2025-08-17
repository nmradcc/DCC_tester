#ifdef __cplusplus
extern "C" {
#endif

void SUSI_Master_Init(SPI_HandleTypeDef *hspi);
void SUSI_Master_Start(void);
void SUSI_Master_Stop(void);

void SUSI_Slave_Init(SPI_HandleTypeDef *hspi);
void SUSI_Slave_Start(void);
void SUSI_Slave_Stop(void);

void SUSI_S_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);


/* SUSI commands */
//TODO: not complete
#define SUSI_NOOP     (0x00)

#define SUSI_FG1      (0x60)
#define SUSI_FG2      (0x61)
#define SUSI_FG3      (0x62)
#define SUSI_FG4      (0x63)
#define SUSI_FG5      (0x64)
#define SUSI_FG6      (0x65)
#define SUSI_FG7      (0x66)
#define SUSI_FG8      (0x67)
#define SUSI_FG9      (0x68)
#define SUSI_FBS      (0x6D)
#define SUSI_FBSL_LO  (0x6E)
#define SUSI_FBSL_HI  (0x6F)
#define SUSI_DC1      (0x40)
#define SUSI_DC2      (0x41)
#define SUSI_DC3      (0x42)
#define SUSI_DC4      (0x43)

#define SUSI_TRG      (0x21)

#define SUSI_CURRENT  (0x23)
#define SUSI_VLOCO    (0x24)
#define SUSI_VCPU     (0x25)
#define SUSI_LLOCO    (0x26)

#define SUSI_LOCOAV   (0x50)
#define SUSI_LOCOTV   (0x51)
#define SUSI_LOCODCC  (0x52)

// analog functions
#define SUSI_ANALOG1  (0x28)
#define SUSI_ANALOG2  (0x29)
#define SUSI_ANALOG3  (0x2A)
#define SUSI_ANALOG4  (0x2B)
#define SUSI_ANALOG5  (0x2C)
#define SUSI_ANALOG6  (0x2D)
#define SUSI_ANALOG7  (0x2E)
#define SUSI_ANALOG8  (0x2F)

/* SUSI packet timeout */
#define PACKET_TIMEOUT_MS  8




#ifdef __cplusplus
}
#endif
