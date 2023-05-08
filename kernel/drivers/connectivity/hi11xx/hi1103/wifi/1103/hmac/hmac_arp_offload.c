


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "frw_ext_if.h"
#include "hmac_arp_offload.h"
#include "hmac_vap.h"
#include "securec.h"
#include "securectype.h"


#undef  THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_HMAC_ARP_OFFLOAD_C


#ifdef _PRE_WLAN_FEATURE_ARP_OFFLOAD
/*****************************************************************************
  2 全局变量定义
*****************************************************************************/


/*****************************************************************************
  3 函数实现
*****************************************************************************/

oal_uint32 hmac_arp_offload_set_ip_addr_etc(mac_vap_stru *pst_mac_vap,
                                        dmac_ip_type_enum_uint8 en_type,
                                        dmac_ip_oper_enum_uint8 en_oper,
                                        oal_void *pst_ip_addr,
                                        oal_void *pst_mask_addr)
{
    dmac_ip_addr_config_stru  st_ip_addr_config;
    oal_uint32                ul_ret;
    oal_int32                 l_ret = EOK;

    if (OAL_UNLIKELY(OAL_ANY_NULL_PTR3(pst_mac_vap,pst_ip_addr,pst_mask_addr)))
    {
        OAM_ERROR_LOG3(0, OAM_SF_PWR,
                        "{hmac_arp_offload_set_ip_addr_etc::The pst_mac_vap or ip_addr or mask_addr is NULL. %p, %p, %p}",
                        (uintptr_t)pst_mac_vap,
                        (uintptr_t)pst_ip_addr,
                        (uintptr_t)pst_mask_addr);
        return OAL_ERR_CODE_PTR_NULL;
    }

    memset_s(&st_ip_addr_config, OAL_SIZEOF(st_ip_addr_config), 0, OAL_SIZEOF(st_ip_addr_config));

    if (DMAC_CONFIG_IPV4 == en_type)
    {
        l_ret += memcpy_s(st_ip_addr_config.auc_ip_addr, OAL_IP_ADDR_MAX_SIZE, pst_ip_addr, OAL_IPV4_ADDR_SIZE);
        l_ret += memcpy_s(st_ip_addr_config.auc_mask_addr, OAL_IP_ADDR_MAX_SIZE, pst_mask_addr, OAL_IPV4_ADDR_SIZE);
    }
    else if (DMAC_CONFIG_IPV6 == en_type)
    {
        l_ret += memcpy_s(st_ip_addr_config.auc_ip_addr, OAL_IP_ADDR_MAX_SIZE, pst_ip_addr, OAL_IPV6_ADDR_SIZE);
    }
    else
    {
        OAM_ERROR_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_PWR, "{hmac_arp_offload_set_ip_addr_etc::The type[%d] is error.}", en_type);
        return OAL_ERR_CODE_MAGIC_NUM_FAIL;
    }
    if (l_ret != EOK) {
        OAM_ERROR_LOG0(0, OAM_SF_PWR, "hmac_arp_offload_set_ip_addr_etc::memcpy fail!");
        return OAL_FAIL;
    }

    st_ip_addr_config.en_type = en_type;

    if (OAL_UNLIKELY(DMAC_IP_OPER_BUTT <= en_oper))
    {
        OAM_ERROR_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_PWR, "{hmac_arp_offload_set_ip_addr_etc::The operation[%d] is error.}", en_type);
        return OAL_ERR_CODE_INVALID_CONFIG;
    }
    st_ip_addr_config.en_oper = en_oper;

    ul_ret = hmac_config_set_ip_addr_etc(pst_mac_vap, OAL_SIZEOF(dmac_ip_addr_config_stru), (oal_uint8 *)&st_ip_addr_config);
    return ul_ret;
}


oal_uint32 hmac_arp_offload_enable(mac_vap_stru *pst_mac_vap, oal_switch_enum_uint8 en_switch)
{
    oal_uint32                    ul_ret;

    if (OAL_UNLIKELY(NULL == pst_mac_vap))
    {
        OAM_ERROR_LOG0(0, OAM_SF_PWR, "{hmac_arp_offload_enable::The pst_mac_vap point is NULL.}");
        return OAL_ERR_CODE_PTR_NULL;
    }

    if (OAL_UNLIKELY(OAL_SWITCH_BUTT <= en_switch))
    {
        OAM_ERROR_LOG1(pst_mac_vap->uc_vap_id, OAM_SF_PWR, "{hmac_arp_offload_enable::The en_switch[%d] is error.}", en_switch);
        return OAL_ERR_CODE_INVALID_CONFIG;
    }

    ul_ret = hmac_config_enable_arp_offload(pst_mac_vap, OAL_SIZEOF(oal_switch_enum_uint8), (oal_switch_enum_uint8 *)&en_switch);
    return ul_ret;
}
#endif

#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

