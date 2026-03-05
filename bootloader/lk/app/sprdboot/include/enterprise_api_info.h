/*
* Created by changmei.chen for EnterpriseService
*/

#ifndef _ENTERPRISE_API_INFO_H_
#define _ENTERPRISE_API_INFO_H_

#define ENTERPRISEAPIINFO_PARTITION_NAME "oemowninfo" // This is the partition name

//[HMDEnterpriseService] add block factory reset api begin 2024-09-04 
#define BLOCK_FACTORY_RESET_OFFSET 1221 //(805 * 1024 + 20) // This is the offset of the partition where to save the flag of factory reset
//[HMDEnterpriseService] add block factory reset api end 2024-09-04 

//[HMDEnterpriseService] add block download mode api begin 2024-09-04 
#define BLOCK_DOWNLOAD_MODE_OFFSET 1220 //(805 * 1024 + 23) // This is the offset of the partition where to save the flag of download mode
//[HMDEnterpriseService] add block download mode api end 2024-09-04 
//[HMDEnterpriseService] for error code showing begin 2025-03-04
#define ENTERPRISE_ERROR_CODE_OFFSET 1494 // This is the offset of the partition where to save the error code

#define ERROR_CODE_POS 39 // the position of the fastboot screen where to show the error code, need to update per project


#define ERROR_CODE_LEN 7 // Don't touch this item
int is_hmd_error_code_set(char* ptrErr); // Don't touch this item
//[HMDEnterpriseService] for error code showing end 2025-03-04
#endif
