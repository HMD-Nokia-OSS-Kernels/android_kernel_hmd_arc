/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include "lk_sec_drv.h"

/*
  image 校验、及相关扩展的示例调用流程：
*/
void uboot_sec_test(void){

  uint32_t ret;
  uint32_t funcid;
  uint32_t start_adr;
  uint32_t lenth;

  //1、准备传入参数
  funcid = TEESMC64_SIPCALL_WITH_ARG;
  start_adr = 0x58000000; //image 起始地址；
  lenth = 0x200;          //image 长度；

  struct smc_param64 param;
  param.a0 = funcid;
  param.a1 = start_adr;
  param.a2 = lenth;

  //2、调用SMC
  tee_smc_call64(&param);
/*

  3、op-tee中处理逻辑：

  void tee_entry(struct thread_smc_args *args)
  {
    switch (args->a0) {
    case TEESMC32_CALLS_COUNT:
      tee_entry_get_api_call_count(args);
      break;
      ........
    case TEESMC64_SIPCALL_WITH_ARG:
      tee_verify_img_with_arg(args);
      break;
    default:
      args->a0 = TEESMC_RETURN_UNKNOWN_FUNCTION;
      break;
    }
  }
*/
  //4、获取返回值
  ret = param.a0;

}


