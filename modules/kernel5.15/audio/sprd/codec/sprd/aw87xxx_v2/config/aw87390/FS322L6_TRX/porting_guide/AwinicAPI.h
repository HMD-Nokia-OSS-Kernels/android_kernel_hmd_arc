#ifndef __AWINIC_ALGO_H__
#define __AWINIC_ALGO_H__

#include <tinyalsa/asoundlib.h>


/* The operation completed with no errors. */
/* Success.*/
#define ADSP_EOK          ( 0  )
/** General failure. */
#define ADSP_EFAILED      ( -1  )
/** Bad operation parameter. */
#define ADSP_EBADPARAM    ( -2  )
/** Unsupported routine or operation. */
#define ADSP_EUNSUPPORTED ( -3  )
/** Unsupported version. */
#define ADSP_EVERSION     ( -4  )
/** Unexpected problem encountered. */
#define ADSP_EUNEXPECTED  ( -5  )

typedef struct media_info{
    unsigned int num_channels;
    unsigned int bits_per_sample;
    unsigned int bit_qactor_sample;
    unsigned int sampling_rate;
    unsigned int data_is_signed;
}media_info_t;

int AwinicSetMediaInfo(void *env_ptr,void *info);
typedef int (*AwGetAlgoInfo)(void);
typedef int (*AwGetSize_t)(unsigned int*);
typedef int (*AwInit_t)(void *,const char*);
typedef int (*AwEnd_t)(void *);
typedef int (*AwReset_t)(void *);
typedef int (*AwHandle_t)(void *,void *,unsigned long);
typedef int (*AwSetMediaInfo_t)(void*,void*);
typedef int (*AwSetVmax_t)(void*,int32_t,int);
typedef int (*AwGetVmax_t)(void*,int32_t*,int);
typedef int (*AwSetMix_t)(void*,uint32_t);
typedef int (*AwSetSpin_t)(void*,uint32_t);
typedef int (*AwGetMix_t)(void *);
typedef int (*AwSwitchScene_t)(void*, uint32_t);

typedef struct AWINIC_SKT_ALGO{
	AwGetAlgoInfo getalgoinfo;
	AwGetSize_t getSize;
	AwInit_t init;
	AwEnd_t end;
	AwReset_t reset;
	AwHandle_t process;
	AwSetMediaInfo_t setMediaInfo;
	AwSetVmax_t setVmax;
	AwGetVmax_t getVmax;
	AwSetMix_t  setMix;
	AwGetMix_t getMix;
	AwSwitchScene_t switchScene;
	AwSetSpin_t setSpin;
	media_info_t info;
	bool  is_module_ready;
	bool  is_module_enable;
	char*  module_context_buffer;
	char*  audio_data_buffer;
	void *awinic_lib;
	enum pcm_format format;
}aw_skt_t;

#endif

