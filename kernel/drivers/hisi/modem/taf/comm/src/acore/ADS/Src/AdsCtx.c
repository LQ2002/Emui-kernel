/*
* Copyright (C) Huawei Technologies Co., Ltd. 2012-2015. All rights reserved.
* foss@huawei.com
*
* If distributed as part of the Linux kernel, the following license terms
* apply:
*
* * This program is free software; you can redistribute it and/or modify
* * it under the terms of the GNU General Public License version 2 and
* * only version 2 as published by the Free Software Foundation.
* *
* * This program is distributed in the hope that it will be useful,
* * but WITHOUT ANY WARRANTY; without even the implied warranty of
* * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* * GNU General Public License for more details.
* *
* * You should have received a copy of the GNU General Public License
* * along with this program; if not, write to the Free Software
* * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
*
* Otherwise, the following license terms apply:
*
* * Redistribution and use in source and binary forms, with or without
* * modification, are permitted provided that the following conditions
* * are met:
* * 1) Redistributions of source code must retain the above copyright
* *    notice, this list of conditions and the following disclaimer.
* * 2) Redistributions in binary form must reproduce the above copyright
* *    notice, this list of conditions and the following disclaimer in the
* *    documentation and/or other materials provided with the distribution.
* * 3) Neither the name of Huawei nor the names of its contributors may
* *    be used to endorse or promote products derived from this software
* *    without specific prior written permission.
*
* * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*/

/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "AdsCtx.h"
#include "AdsUpLink.h"
#include "AdsDownLink.h"
#include "AdsFilter.h"
#include "AdsDebug.h"
#include "mdrv.h"



/*****************************************************************************
    协议栈打印打点方式下的.C文件宏定义
*****************************************************************************/
#define    THIS_FILE_ID                 PS_FILE_ID_ADS_CTX_C

/*****************************************************************************
  2 全局变量定义
*****************************************************************************/

VOS_UINT32                              g_ulAdsULTaskId        = 0;  /* ADS上行任务ID */
VOS_UINT32                              g_ulAdsDLTaskId        = 0;  /* ADS下行任务ID */
VOS_UINT32                              g_ulAdsULTaskReadyFlag = 0;  /* ADS上行任务运行状态 */
VOS_UINT32                              g_ulAdsDLTaskReadyFlag = 0;  /* ADS下行任务运行状态 */

/* ADS模块的上下文 */
ADS_CTX_STRU                            g_stAdsCtx;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
#ifdef CONFIG_ARM64
VOS_UINT64                              g_ullAdsDmaMask = 0xffffffffffffffffULL;
#else
VOS_UINT64                              g_ullAdsDmaMask = 0xffffffffULL;
#endif
#else
struct device                          *g_pstDmaDev;
#endif
/*****************************************************************************
  3 函数实现
*****************************************************************************/


VOS_UINT32 ADS_UL_CheckAllQueueEmpty(VOS_UINT32 ulInstanceIndex)
{
    VOS_UINT32                          i;
    ADS_UL_CTX_STRU                    *pstAdsUlCtx = VOS_NULL_PTR;

    pstAdsUlCtx = ADS_GetUlCtx(ulInstanceIndex);

    for (i = ADS_RAB_ID_MIN; i < ADS_RAB_ID_MAX + 1; i++)
    {
        if (VOS_FALSE != pstAdsUlCtx->astAdsUlQueue[i].ucIsQueueValid)
        {
            break;
        }
    }

    /* 所有的PDP都去激活　*/
    if ((ADS_RAB_ID_MAX + 1) != i)
    {
        return VOS_FALSE;
    }

    return VOS_TRUE;
}


VOS_VOID ADS_UL_SetProtectTmrLen(VOS_UINT32 ulTimerLen)
{
    g_stAdsCtx.stAdsIpfCtx.ulProtectTmrLen = ulTimerLen;
    return;
}


VOS_UINT32 ADS_UL_IsQueueExistent(
    VOS_UINT32                           ulInstanceIndex,
    VOS_UINT32                           ulRabId
)
{
    /* 队列为空 */
    if (VOS_NULL_PTR == ADS_UL_GET_QUEUE_LINK_PTR(ulInstanceIndex, ulRabId))
    {
        return VOS_ERR;
    }
    else
    {
        return VOS_OK;
    }
}


VOS_UINT32 ADS_UL_IsAnyQueueExist(VOS_VOID)
{
    VOS_UINT32                           ulInstance;
    VOS_UINT32                           ulRabId;

    for (ulInstance = 0; ulInstance < ADS_INSTANCE_MAX_NUM; ulInstance++)
    {
        for (ulRabId = ADS_RAB_ID_MIN; ulRabId <= ADS_RAB_ID_MAX; ulRabId++)
        {
            if (VOS_OK == ADS_UL_IsQueueExistent(ulInstance, ulRabId))
            {
                return VOS_TRUE;
            }
        }
    }

    return VOS_FALSE;
}


VOS_UINT32 ADS_UL_InsertQueue(
    VOS_UINT32                           ulInstance,
    IMM_ZC_STRU                         *pstImmZc,
    VOS_UINT32                           ulRabId
)
{
    VOS_UINT32                          ulNonEmptyEvent;
    VOS_UINT32                          ulAllUlQueueDataNum;
    VOS_UINT32                          ulSlice;
    VOS_UINT                            ulQueueLen;
    VOS_ULONG                           ulLockLevel;
    ADS_CDS_IPF_PKT_TYPE_ENUM_UINT8     enPktType;

    ulNonEmptyEvent = VOS_FALSE;

    /* 此接口不释放pstData，由上层模块根据函数返回值判断是否需要释放内存 */

    /* 队列加锁 */
    /*lint -e571*/
    VOS_SpinLockIntLock(ADS_UL_GET_QUEUE_LINK_SPINLOCK(ulInstance, ulRabId), ulLockLevel);
    /*lint +e571*/

    /* 结点存在，但队列不存在 */
    if (VOS_OK != ADS_UL_IsQueueExistent(ulInstance, ulRabId))
    {
        /* 队列操作完成解锁 */
        VOS_SpinUnlockIntUnlock(ADS_UL_GET_QUEUE_LINK_SPINLOCK(ulInstance, ulRabId), ulLockLevel);
        ADS_WARNING_LOG(ACPU_PID_ADS_UL, "ADS_UL_InsertQueue:the queue is not ext!");
        ADS_DBG_UL_PKT_ENQUE_FAIL_NUM(1);
        return VOS_ERR;
    }

    ulQueueLen = IMM_ZcQueueLen(ADS_UL_GET_QUEUE_LINK_PTR(ulInstance, ulRabId));
    if (ulQueueLen >= ADS_UL_GET_MAX_QUEUE_LENGTH(ulInstance))
    {
        /* 队列操作完成解锁 */
        VOS_SpinUnlockIntUnlock(ADS_UL_GET_QUEUE_LINK_SPINLOCK(ulInstance, ulRabId), ulLockLevel);
        ADS_DBG_UL_PKT_ENQUE_FAIL_NUM(1);
        return VOS_ERR;
    }

    /* 保存ModemId/PktType/RabId到IMM */
    enPktType = ADS_UL_GET_QUEUE_PKT_TYPE(ulInstance, ulRabId);
    ADS_UL_SAVE_MODEMID_PKTTYEP_RABID_TO_IMM(pstImmZc, ulInstance, enPktType, ulRabId);

    /* 插入队列前将数据打上时间戳 */
    ulSlice = VOS_GetSlice();
    ADS_UL_SAVE_SLICE_TO_IMM(pstImmZc, ulSlice);

    /* 插入队列 */
    IMM_ZcQueueTail(ADS_UL_GET_QUEUE_LINK_PTR(ulInstance, ulRabId), pstImmZc);
    ADS_DBG_UL_PKT_ENQUE_SUCC_NUM(1);

    /* 队列由空变为非空 */
    if (1 == IMM_ZcQueueLen(ADS_UL_GET_QUEUE_LINK_PTR(ulInstance, ulRabId)))
    {
        ulNonEmptyEvent = VOS_TRUE;
    }

    /* 队列操作完成解锁 */
    VOS_SpinUnlockIntUnlock(ADS_UL_GET_QUEUE_LINK_SPINLOCK(ulInstance, ulRabId), ulLockLevel);

    ulAllUlQueueDataNum = ADS_UL_GetAllQueueDataNum();

    if (VOS_TRUE == ADS_UL_GET_THRESHOLD_ACTIVE_FLAG())
    {
        /* (1).jiffies超时,则直接触发发数,并停止发数定时器
           (2).队列中数据已到攒包门限且当前没有在处理数据,触发上行缓存缓存处理事件
           (3).队列由空变为非空时启动数据统计定时器以及保护定时器
         */
        ADS_UL_ADD_STAT_PKT_NUM(1);

        /* 超时长度为非零则需要启动触发发数 */
        if (0 != ADS_UL_GET_JIFFIES_EXP_TMR_LEN())
        {
            if (ADS_TIME_AFTER_EQ(ADS_CURRENT_TICK,
                                  (ADS_UL_GET_JIFFIES_TMR_CNT() + ADS_UL_GET_JIFFIES_EXP_TMR_LEN())))
            {
                ADS_UL_SET_JIFFIES_TMR_CNT(ADS_CURRENT_TICK);
                ADS_UL_SndEvent(ADS_UL_EVENT_DATA_PROC);
                ADS_DBG_UL_TMR_HIT_THRES_TRIG_EVENT(1);
                return VOS_OK;
            }
        }

        if (ADS_UL_IS_REACH_THRESHOLD(ulAllUlQueueDataNum, ADS_UL_GET_SENDING_FLAG()))
        {
            ADS_UL_SndEvent(ADS_UL_EVENT_DATA_PROC);
            ADS_DBG_UL_QUE_HIT_THRES_TRIG_EVENT(1);
        }

        /* 队列由空变为非空 */
        if (VOS_TRUE == ulNonEmptyEvent)
        {
            ADS_StartTimer(TI_ADS_UL_DATA_STAT, ADS_UL_GET_STAT_TIMER_LEN());
            ADS_StartTimer(TI_ADS_UL_SEND, ADS_UL_GET_PROTECT_TIMER_LEN());
        }
    }
    else
    {
        /* (1).队列由空变为非空时触发上行缓存处理事件
           (2).队列中数据已到攒包门限的整数倍且当前没有在处理数据
               触发上行缓存缓存处理事件
         */
        if (VOS_TRUE == ulNonEmptyEvent)
        {
            ADS_UL_SndEvent(ADS_UL_EVENT_DATA_PROC);
            ADS_DBG_UL_QUE_NON_EMPTY_TRIG_EVENT(1);
        }
        else
        {
            if (ADS_UL_IS_REACH_THRESHOLD(ulAllUlQueueDataNum, ADS_UL_GET_SENDING_FLAG()))
            {
                ADS_UL_SndEvent(ADS_UL_EVENT_DATA_PROC);
                ADS_DBG_UL_QUE_HIT_THRES_TRIG_EVENT(1);
            }
        }
    }

    return VOS_OK;
}


VOS_UINT32 ADS_UL_GetInstanceAllQueueDataNum(VOS_UINT32 ulInstanceIndex)
{
    VOS_UINT32                          i;
    VOS_UINT32                          ulTotalNum;
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx = VOS_NULL_PTR;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex]);

    ulTotalNum = 0;

    for (i = ADS_RAB_ID_MIN; i < ADS_RAB_ID_MAX + 1; i++)
    {
        if (VOS_NULL_PTR != pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].pstAdsUlLink)
        {
            ulTotalNum += pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].pstAdsUlLink->qlen;
        }
    }

    return ulTotalNum;
}


VOS_UINT32 ADS_UL_GetAllQueueDataNum(VOS_VOID)
{
    VOS_UINT32                          ulTotalNum;
    VOS_UINT32                          i;

    ulTotalNum = 0;

    for (i = 0; i < ADS_INSTANCE_MAX_NUM; i++)
    {
        ulTotalNum = ulTotalNum + ADS_UL_GetInstanceAllQueueDataNum(i);
    }

    return ulTotalNum;
}


/*lint -sem(ADS_UL_SetQueue,custodial(4))*/
VOS_UINT32 ADS_UL_CreateQueue(
    VOS_UINT32                          ulInstanceIndex,
    VOS_UINT32                          ulRabId,
    ADS_QCI_TYPE_ENUM_UINT8             enPrio,
    ADS_CDS_IPF_PKT_TYPE_ENUM_UINT8     enPktType,
    VOS_UINT8                           uc1XorHrpdUlIpfFlag
)
{
    IMM_ZC_HEAD_STRU                   *pstUlQueue;
    ADS_UL_CTX_STRU                    *pstAdsUlCtx;

    pstAdsUlCtx = ADS_GetUlCtx(ulInstanceIndex);

    /* RabId的队列已经存在 */
    if (VOS_OK == ADS_UL_IsQueueExistent(ulInstanceIndex, ulRabId))
    {
        /* 对应的调度优先级也一样或者是比之前的要低，不更新QCI直接返回OK */
        if (enPrio >= pstAdsUlCtx->astAdsUlQueue[ulRabId].enPrio)
        {
            return VOS_OK;
        }
        /* 如果对应的调度优先级比之前的要高，需要更新该PDP的队列优先级，并对队列管理进行排序 */
        else
        {
            ADS_UL_UpdateQueueInPdpModified(ulInstanceIndex, enPrio, ulRabId);
            return VOS_OK;
        }
    }

    /* ucRabID的队列不存在, 需要创建队列头结点 */
    pstUlQueue = (IMM_ZC_HEAD_STRU *)PS_MEM_ALLOC(ACPU_PID_ADS_UL, sizeof(IMM_ZC_HEAD_STRU));

    if (VOS_NULL_PTR == pstUlQueue)
    {
        ADS_ERROR_LOG(ACPU_PID_ADS_UL, "ADS_UL_CreateQueue: pstUlQueue is null");
        return VOS_ERR;
    }

    /* 队列初始化 */
    IMM_ZcQueueHeadInit(pstUlQueue);

    /* 将队列信息更新到上行上下文 */
    ADS_UL_SetQueue(ulInstanceIndex,
                    ulRabId,
                    VOS_TRUE,
                    pstUlQueue,
                    enPrio,
                    enPktType,
                    uc1XorHrpdUlIpfFlag);

    /* 队列不可能被用尽，一个RABID对应一个队列，而无效的已经在消息处理处屏蔽，
       故不需要判断是否满，可以直接重新排序 */
    ADS_UL_OrderQueueIndex(ulInstanceIndex, ulRabId);

    return VOS_OK;
}


VOS_VOID ADS_UL_ClearQueue(
    IMM_ZC_HEAD_STRU                   *pstQueue
)
{
    VOS_UINT32                          i;
    VOS_UINT32                          ulQueueCnt;
    IMM_ZC_STRU                        *pstNode;

    ulQueueCnt = IMM_ZcQueueLen(pstQueue);

    for (i = 0; i < ulQueueCnt; i++)
    {
        pstNode = IMM_ZcDequeueHead(pstQueue);

        /* 释放结点内容 */
        if (VOS_NULL_PTR != pstNode)
        {
            IMM_ZcFreeAny(pstNode);
        }
    }
}


VOS_VOID ADS_UL_DestroyQueue(
    VOS_UINT32                           ulInstanceIndex,
    VOS_UINT32                           ulRabId
)
{
    VOS_ULONG                           ulLockLevel;

    /* 结点存在但队列为空 */
    if (VOS_ERR == ADS_UL_IsQueueExistent(ulInstanceIndex, ulRabId))
    {
        /* Rab Id以及优先级置为无效值 */
        ADS_UL_SetQueue(ulInstanceIndex,
                        ulRabId,
                        VOS_FALSE,
                        VOS_NULL_PTR,
                        ADS_QCI_TYPE_BUTT,
                        ADS_PDP_TYPE_BUTT,
                        VOS_FALSE);

        /* 根据最新的队列管理进行排序 */
        ADS_UL_UpdateQueueInPdpDeactived(ulInstanceIndex, ulRabId);

        return;
    }

    /* 队列加锁 */
    /*lint -e571*/
    VOS_SpinLockIntLock(ADS_UL_GET_QUEUE_LINK_SPINLOCK(ulInstanceIndex, ulRabId), ulLockLevel);
    /*lint +e571*/

    /* 销毁队列中的数据 */
    ADS_UL_ClearQueue(ADS_UL_GET_QUEUE_LINK_PTR(ulInstanceIndex, ulRabId));

    /* 销毁队列头结点*/
    PS_MEM_FREE(ACPU_PID_ADS_DL, ADS_UL_GET_QUEUE_LINK_PTR(ulInstanceIndex, ulRabId));

    /* 将队列信息更新到上行上下文 */
    ADS_UL_SetQueue(ulInstanceIndex,
                    ulRabId,
                    VOS_FALSE,
                    VOS_NULL_PTR,
                    ADS_QCI_TYPE_BUTT,
                    ADS_PDP_TYPE_BUTT,
                    VOS_FALSE);

    /* 队列操作完成解锁 */
    VOS_SpinUnlockIntUnlock(ADS_UL_GET_QUEUE_LINK_SPINLOCK(ulInstanceIndex, ulRabId), ulLockLevel);

    /* 根据最新的队列管理进行排序 */
    ADS_UL_UpdateQueueInPdpDeactived(ulInstanceIndex, ulRabId);

}


VOS_UINT32 ADS_UL_GetInsertIndex(
    VOS_UINT32                           ulInstanceIndex,
    VOS_UINT32                           ulRabId
)
{
    VOS_UINT32                          i;
    ADS_UL_CTX_STRU                    *pstAdsUlCtx;

    pstAdsUlCtx = ADS_GetUlCtx(ulInstanceIndex);

    /* 根据优先级来获取上行上下文中队列的Index值 */
    for (i = 0; i < ADS_RAB_NUM_MAX; i++)
    {
        if (pstAdsUlCtx->aulPrioIndex[i] == ulRabId)
        {
            break;
        }
    }

    return i;

}


VOS_VOID ADS_UL_OrderQueueIndex(
    VOS_UINT32                           ulInstanceIndex,
    VOS_UINT32                           ulIndex
)
{
    VOS_UINT32                          i;
    VOS_UINT32                          j;
    ADS_UL_CTX_STRU                    *pstAdsUlCtx;

    pstAdsUlCtx = ADS_GetUlCtx(ulInstanceIndex);

    /* 如果该PDP的优先级比较高，需要插入到其他PDP的前面，比其优先级低的需要向后移一位 */
    for (i = 0; i < ADS_RAB_NUM_MAX; i++)
    {
        if (pstAdsUlCtx->astAdsUlQueue[ulIndex].enPrio < pstAdsUlCtx->astAdsUlQueue[pstAdsUlCtx->aulPrioIndex[i]].enPrio)
        {
            for (j = ADS_RAB_NUM_MAX - 1; j > i; j--)
            {
                pstAdsUlCtx->aulPrioIndex[j] = pstAdsUlCtx->aulPrioIndex[j - 1];
            }
            pstAdsUlCtx->aulPrioIndex[i] = ulIndex;

            break;
        }
    }
}


VOS_VOID ADS_UL_UpdateQueueInPdpModified(
    VOS_UINT32                           ulInstanceIndex,
    ADS_QCI_TYPE_ENUM_UINT8              enPrio,
    VOS_UINT32                           ulRabId
)
{
    VOS_UINT32                          i;
    VOS_UINT32                          ulIndex;
    ADS_UL_CTX_STRU                    *pstAdsUlCtx;

    pstAdsUlCtx = ADS_GetUlCtx(ulInstanceIndex);

    /* 将修改的优先级更新到对于的队列管理中 */
    pstAdsUlCtx->astAdsUlQueue[ulRabId].enPrio = enPrio;

    /* 获取修改的RABID在aucPrioIndex数组中位置 */
    ulIndex = ADS_UL_GetInsertIndex(ulInstanceIndex, ulRabId);

    /* 没有找到，则不做处理 */
    if (ulIndex >= ADS_RAB_NUM_MAX)
    {
        return;
    }

    /* 先将修改对应位后面的向前移动一位 */
    for (i = ulIndex; i <  ADS_RAB_NUM_MAX - 1; i++)
    {
        pstAdsUlCtx->aulPrioIndex[i] = pstAdsUlCtx->aulPrioIndex[i + 1UL];
    }

    pstAdsUlCtx->aulPrioIndex[ADS_RAB_NUM_MAX - 1] = 0;

    /* 移动后相当于是重新插入到队列管理中 */
    ADS_UL_OrderQueueIndex(ulInstanceIndex, ulRabId);

}


VOS_VOID ADS_UL_UpdateQueueInPdpDeactived(
    VOS_UINT32                           ulInstanceIndex,
    VOS_UINT32                           ulRabId
)
{
    VOS_UINT32                          i;
    VOS_UINT32                          ulIndex;
    ADS_UL_CTX_STRU                    *pstAdsUlCtx;

    pstAdsUlCtx = ADS_GetUlCtx(ulInstanceIndex);

    /* 根据删除的PDP索引，将其后面的元素向前移动一位 */
    ulIndex = ADS_UL_GetInsertIndex(ulInstanceIndex, ulRabId);

    if (ulIndex >= ADS_RAB_NUM_MAX)
    {
        return;
    }

    for (i = ulIndex; i < ADS_RAB_NUM_MAX - 1; i++)
    {
        pstAdsUlCtx->aulPrioIndex[i] = pstAdsUlCtx->aulPrioIndex[i + 1UL];
    }

    pstAdsUlCtx->aulPrioIndex[ADS_RAB_NUM_MAX - 1] = 0;

}


VOS_VOID ADS_UL_SetQueue(
    VOS_UINT32                          ulInstanceIndex,
    VOS_UINT32                          ulRabId,
    VOS_UINT8                           ucIsQueueValid,
    IMM_ZC_HEAD_STRU                   *pstUlQueue,
    ADS_QCI_TYPE_ENUM_UINT8             enPrio,
    ADS_CDS_IPF_PKT_TYPE_ENUM_UINT8     enPktType,
    VOS_UINT8                           uc1XorHrpdUlIpfFlag
)
{
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].pstAdsUlLink   = pstUlQueue;
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].ucIsQueueValid = ucIsQueueValid;
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].enPrio         = enPrio;
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].usRecordNum    = 0;
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].enPktType      = enPktType;
#if (FEATURE_ON == FEATURE_UE_MODE_CDMA)
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].uc1XorHrpdUlIpfFlag = uc1XorHrpdUlIpfFlag;
#else
    g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex].stAdsUlCtx.astAdsUlQueue[ulRabId].uc1XorHrpdUlIpfFlag = VOS_FALSE;
#endif
}


VOS_VOID ADS_UL_SndEvent(VOS_UINT32 ulEvent)
{
    if (1 == g_ulAdsULTaskReadyFlag)
    {
        (VOS_VOID)VOS_EventWrite(g_ulAdsULTaskId, ulEvent);
    }

    return;
}


VOS_VOID ADS_UL_ProcEvent(VOS_UINT32 ulEvent)
{
    if (ulEvent & ADS_UL_EVENT_DATA_PROC)
    {
        ADS_UL_WakeLock();
        ADS_UL_ProcLinkData();
        ADS_UL_WakeUnLock();
        ADS_DBG_UL_PROC_EVENT_NUM(1);
    }

    return;
}


VOS_VOID ADS_DL_SndEvent(VOS_UINT32 ulEvent)
{
    if (1 == g_ulAdsDLTaskReadyFlag)
    {
        (VOS_VOID)VOS_EventWrite(g_ulAdsDLTaskId, ulEvent);
    }

    return;
}


VOS_VOID ADS_DL_ProcEvent(VOS_UINT32 ulEvent)
{
#if (FEATURE_ON == FEATURE_RNIC_NAPI_GRO)
    VOS_ULONG                           ulLockLevel;
#endif

    if (ulEvent & ADS_DL_EVENT_IPF_RD_INT)
    {
        ADS_DL_WakeLock();
        ADS_DL_ProcIpfResult();
        ADS_DL_WakeUnLock();
        ADS_DBG_DL_PROC_IPF_RD_EVENT_NUM(1);
    }

    if (ulEvent & ADS_DL_EVENT_IPF_ADQ_EMPTY_INT)
    {
#if (FEATURE_ON == FEATURE_RNIC_NAPI_GRO)
        /*lint -e571*/
        VOS_SpinLockIntLock(&(g_stAdsCtx.stAdsIpfCtx.stAdqSpinLock), ulLockLevel);
        /*lint +e571*/
        ADS_DL_AllocMemForAdq();
        VOS_SpinUnlockIntUnlock(&(g_stAdsCtx.stAdsIpfCtx.stAdqSpinLock), ulLockLevel);
#else
        ADS_DL_AllocMemForAdq();
#endif
        ADS_DBG_DL_PROC_IPF_AD_EVENT_NUM(1);
    }

    if (ulEvent & ADS_DL_EVENT_IPF_FILTER_DATA_PROC)
    {
        ADS_DL_ProcIpfFilterQue();
    }

    return;
}


VOS_VOID ADS_DL_InitFcAssemParamInfo(VOS_VOID)
{
    ADS_DL_FC_ASSEM_STRU               *pstFcAssemInfo;

    pstFcAssemInfo = ADS_DL_GET_FC_ASSEM_INFO_PTR(ADS_INSTANCE_INDEX_0);

    pstFcAssemInfo->ulEnableMask     = VOS_FALSE;
    pstFcAssemInfo->ulFcActiveFlg    = VOS_FALSE;
    pstFcAssemInfo->ulTmrCnt         = ADS_CURRENT_TICK;
    pstFcAssemInfo->ulRdCnt          = 0;
    pstFcAssemInfo->ulRateUpLev      = 0;
    pstFcAssemInfo->ulRateDownLev    = 0;
    pstFcAssemInfo->ulExpireTmrLen   = 0;
    pstFcAssemInfo->pFcAssemTuneFunc = VOS_NULL_PTR;

    return;
}


VOS_VOID ADS_DL_ResetFcAssemParamInfo(VOS_VOID)
{
    ADS_DL_FC_ASSEM_STRU               *pstFcAssemInfo;

    pstFcAssemInfo = ADS_DL_GET_FC_ASSEM_INFO_PTR(ADS_INSTANCE_INDEX_0);

    pstFcAssemInfo->ulFcActiveFlg    = VOS_FALSE;
    pstFcAssemInfo->ulRdCnt          = 0;

    return;
}


VOS_UINT32 ADS_UL_EnableRxWakeLockTimeout(VOS_UINT32 ulValue)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    if (ulValue > pstIpfCntxt->ulRxWakeLockTimeout)
    {
        pstIpfCntxt->ulRxWakeLockTimeout = ulValue;
    }

    return 0;
}


VOS_UINT32 ADS_UL_WakeLockTimeout(VOS_VOID)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;
    VOS_UINT32                          ulRet;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    ulRet       = pstIpfCntxt->ulRxWakeLockTimeout;

    if (0 != pstIpfCntxt->ulRxWakeLockTimeout)
    {
        __pm_wakeup_event(&(pstIpfCntxt->stRxWakeLock), pstIpfCntxt->ulRxWakeLockTimeout);
    }

    pstIpfCntxt->ulRxWakeLockTimeout = 0;

    return ulRet;
}


VOS_UINT32 ADS_UL_WakeLock(VOS_VOID)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;
    VOS_UINT32                          ulRet;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    if (VOS_FALSE == pstIpfCntxt->ulWakeLockEnable)
    {
        return 0;
    }

    __pm_stay_awake(&(pstIpfCntxt->stUlBdWakeLock));
    pstIpfCntxt->ulUlBdWakeLockCnt++;

    ulRet = pstIpfCntxt->ulUlBdWakeLockCnt;
    return ulRet;
}


VOS_UINT32 ADS_UL_WakeUnLock(VOS_VOID)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;
    VOS_UINT32                          ulRet;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    if (VOS_FALSE == pstIpfCntxt->ulWakeLockEnable)
    {
        return 0;
    }

    ADS_UL_WakeLockTimeout();

    __pm_relax(&(pstIpfCntxt->stUlBdWakeLock));
    pstIpfCntxt->ulUlBdWakeLockCnt--;

    ulRet = pstIpfCntxt->ulUlBdWakeLockCnt;
    return ulRet;
}


VOS_UINT32 ADS_DL_EnableTxWakeLockTimeout(VOS_UINT32 ulValue)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    if (ulValue > pstIpfCntxt->ulTxWakeLockTimeout)
    {
        pstIpfCntxt->ulTxWakeLockTimeout = ulValue;
    }

    return 0;
}


VOS_UINT32 ADS_DL_WakeLockTimeout(VOS_VOID)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;
    VOS_UINT32                          ulRet;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    ulRet       = pstIpfCntxt->ulTxWakeLockTimeout;

    if (0 != pstIpfCntxt->ulTxWakeLockTimeout)
    {
        __pm_wakeup_event(&(pstIpfCntxt->stTxWakeLock), pstIpfCntxt->ulTxWakeLockTimeout);
    }

    pstIpfCntxt->ulTxWakeLockTimeout = 0;

    return ulRet;
}


VOS_UINT32 ADS_DL_WakeLock(VOS_VOID)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;
    VOS_UINT32                          ulRet;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    if (VOS_FALSE == pstIpfCntxt->ulWakeLockEnable)
    {
        return 0;
    }

    __pm_stay_awake(&(pstIpfCntxt->stDlRdWakeLock));
    pstIpfCntxt->ulDlRdWakeLockCnt++;

    ulRet = pstIpfCntxt->ulDlRdWakeLockCnt;
    return ulRet;
}


VOS_UINT32 ADS_DL_WakeUnLock(VOS_VOID)
{
    ADS_IPF_CTX_STRU                   *pstIpfCntxt = VOS_NULL_PTR;
    VOS_UINT32                          ulRet;

    pstIpfCntxt = ADS_GET_IPF_CTX_PTR();
    if (VOS_FALSE == pstIpfCntxt->ulWakeLockEnable)
    {
        return 0;
    }

    ADS_DL_WakeLockTimeout();

    __pm_relax(&(pstIpfCntxt->stDlRdWakeLock));
    pstIpfCntxt->ulDlRdWakeLockCnt--;

    ulRet = pstIpfCntxt->ulDlRdWakeLockCnt;
    return ulRet;
}


VOS_VOID ADS_IPF_MemMapRequset(
    IMM_ZC_STRU                        *pstImmZc,
    VOS_UINT32                          ulLen,
    VOS_UINT8                           ucIsIn
)
{
    VOS_UINT8                          *pucData = VOS_NULL_PTR;
    dma_addr_t                          ulDmaAddr;

    pucData   = IMM_ZcGetDataPtr(pstImmZc);
    ulDmaAddr = dma_map_single(ADS_GET_IPF_DEV(), pucData, ulLen,
                        (ucIsIn) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    ADS_IPF_SetMemDma(pstImmZc, ulDmaAddr);
    return;
}


VOS_VOID ADS_IPF_MemMapByDmaRequset(
    IMM_ZC_STRU                        *pstImmZc,
    VOS_UINT32                          ulLen,
    VOS_UINT8                           ucIsIn
)
{
    VOS_UINT8                          *pucData = VOS_NULL_PTR;
    dma_addr_t                          ulDmaAddr;

    ulDmaAddr = ADS_IPF_GetMemDma(pstImmZc);
    pucData   = phys_to_virt(ulDmaAddr);
    dma_map_single(ADS_GET_IPF_DEV(), pucData, ulLen,
                        (ucIsIn) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    return;
}


VOS_VOID ADS_IPF_MemUnmapRequset(
    IMM_ZC_STRU                        *pstImmZc,
    VOS_UINT32                          ulLen,
    VOS_UINT8                           ucIsIn
)
{
    dma_addr_t                          ulDmaAddr;

    ulDmaAddr = ADS_IPF_GetMemDma(pstImmZc);
    dma_unmap_single(ADS_GET_IPF_DEV(), ulDmaAddr, ulLen,
                        (ucIsIn) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    return;
}


VOS_UINT32 ADS_IPF_IsSpeMem(IMM_ZC_STRU *pstImmZc)
{
    return VOS_FALSE;
}


VOS_VOID ADS_IPF_SetMemDma(IMM_ZC_STRU *pstImmZc, dma_addr_t ulDmaAddr)
{
    ADS_IMM_MEM_CB(pstImmZc)->ulDmaAddr = ulDmaAddr;
    return;
}


dma_addr_t ADS_IPF_GetMemDma(IMM_ZC_STRU *pstImmZc)
{
    return ADS_IMM_MEM_CB(pstImmZc)->ulDmaAddr;
}



IMM_ZC_STRU* ADS_IPF_AllocMem(VOS_UINT32 ulPoolId, VOS_UINT32 ulLen, VOS_UINT32 ulReserveLen)
{
    IMM_ZC_STRU                        *pstImmZc = VOS_NULL_PTR;

    pstImmZc = (IMM_ZC_STRU *)IMM_ZcStaticAlloc(ulLen);
    if (VOS_NULL_PTR == pstImmZc)
    {
        ADS_DBG_DL_ADQ_ALLOC_SYS_MEM_FAIL_NUM(1);
        return VOS_NULL_PTR;
    }

    ADS_DBG_DL_ADQ_ALLOC_SYS_MEM_SUCC_NUM(1);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
    /* 操作顺序: 刷CACHE, 预留头部空间 */
    ADS_IPF_DL_MEM_MAP(pstImmZc, ulLen);
    IMM_ZcReserve(pstImmZc, ulReserveLen);
#else
    /* 操作顺序: 预留头部空间，刷CACHE  */
    IMM_ZcReserve(pstImmZc, ulReserveLen);
    ADS_IPF_DL_MEM_MAP(pstImmZc, ulLen);
#endif

    return pstImmZc;
}


VOS_SEM ADS_GetULResetSem(VOS_VOID)
{
    return g_stAdsCtx.hULResetSem;
}


VOS_SEM ADS_GetDLResetSem(VOS_VOID)
{
    return g_stAdsCtx.hDLResetSem;
}


VOS_UINT32 ADS_GetCurrentRate(
    VOS_UINT32                         *pulUlBpsRate,
    VOS_UINT32                         *pulDlBpsRate
)
{
    /* FC只有stick用，只返回实例0的 */
    *pulUlBpsRate = g_stAdsCtx.stDsFlowStatsCtx.stULDataStats.ulULCurDataRate << 3;
    *pulDlBpsRate = g_stAdsCtx.stDsFlowStatsCtx.stDLDataStats.ulDLCurDataRate << 3;

    return VOS_OK;
}


VOS_UINT8 ADS_GetUlResetFlag(VOS_VOID)
{
    return g_stAdsCtx.ucUlResetFlag;
}


VOS_VOID ADS_SetUlResetFlag(VOS_UINT8 ucFlag)
{
    g_stAdsCtx.ucUlResetFlag = ucFlag;

    return;
}

#if (FEATURE_ON == FEATURE_RNIC_NAPI_GRO)

VOS_VOID ADS_RNIC_AdjNapiWeight(VOS_VOID)
{
    VOS_UINT32                          ulInstanceIndex;
    VOS_UINT32                          ulRabIndex;
    VOS_UINT32                          ulExParam;
    ADJUST_NAPI_WEIGHT_FUNC             pAdjNapiWeightFunc = VOS_NULL_PTR;

    /* 查找注册了AdjNapiWeight回调函数的网卡，调用该回调函数 */
    for (ulInstanceIndex = 0; ulInstanceIndex < ADS_INSTANCE_MAX_NUM; ulInstanceIndex++)
    {
        for (ulRabIndex = ADS_RAB_ID_MIN; ulRabIndex < ADS_RAB_ID_MAX; ulRabIndex++)
        {
            pAdjNapiWeightFunc = ADS_DL_GET_ADJ_NAPI_WEIGHT_CALLBACK_FUNC(ulInstanceIndex, ulRabIndex);

            if (VOS_NULL_PTR != pAdjNapiWeightFunc)
            {
                ulExParam = ADS_DL_GET_DATA_EXPARAM(ulInstanceIndex, ulRabIndex);
                pAdjNapiWeightFunc(ulExParam);
            }
        }
    }

    return;
}
#endif



ADS_UL_CTX_STRU* ADS_GetUlCtx(VOS_UINT32 ulInstanceIndex)
{
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx = VOS_NULL_PTR;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex]);

    return &(pstAdsSpecCtx->stAdsUlCtx);
}


ADS_DL_CTX_STRU* ADS_GetDlCtx(VOS_UINT32 ulInstanceIndex)
{
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx = VOS_NULL_PTR;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex]);

    return &(pstAdsSpecCtx->stAdsDlCtx);
}


ADS_TIMER_CTX_STRU* ADS_GetTiCtx(VOS_VOID)
{
    return g_stAdsCtx.astAdsTiCtx;
}


ADS_CTX_STRU* ADS_GetAllCtx(VOS_VOID)
{
    return &g_stAdsCtx;
}


VOS_VOID ADS_InitUlCtx(VOS_UINT32 ulInstanceIndex)
{
    VOS_UINT32                          i;
    VOS_UINT32                          ulRst;
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx = VOS_NULL_PTR;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstanceIndex]);

    /* 默认从第一个队列开始调度 */
    pstAdsSpecCtx->stAdsUlCtx.ulAdsUlCurIndex      = 0;

    for (i = 0; i < ADS_RAB_ID_MAX + 1; i++)
    {
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].pstAdsUlLink    = VOS_NULL_PTR;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].ucIsQueueValid  = VOS_FALSE;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].enPrio          = ADS_QCI_TYPE_BUTT;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].usRecordNum     = 0;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].enPktType       = ADS_CDS_IPF_PKT_TYPE_IP;

        /* 锁初始化 */
        VOS_SpinLockInit(&(pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].stSpinLock));
    }

    TAF_MEM_SET_S(pstAdsSpecCtx->stAdsUlCtx.aulPrioIndex, sizeof(pstAdsSpecCtx->stAdsUlCtx.aulPrioIndex),
                  0x00, sizeof(pstAdsSpecCtx->stAdsUlCtx.aulPrioIndex));

    /* 读NV，将优先级加权数读写到ADS上下文中 */

    ulRst = TAF_ACORE_NV_READ(ulInstanceIndex,
                              en_NV_Item_ADS_Queue_Scheduler_Pri,
                              &(pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv),
                              sizeof(ADS_UL_QUEUE_SCHEDULER_PRI_NV_STRU));
    if(NV_OK != ulRst)
    {
        pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ulStatus = VOS_FALSE;

        for (i = 0; i < ADS_UL_QUEUE_SCHEDULER_PRI_MAX; i++)
        {
            pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ausPriWeightedNum[i] = ADS_UL_DEFAULT_PRI_WEIGHTED_NUM;
        }

        ADS_ERROR_LOG(ACPU_PID_ADS_UL, "ADS_InitUlCtx: NV read failed !");
    }

    if (VOS_FALSE == pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ulStatus)
    {
        for (i = 0; i < ADS_UL_QUEUE_SCHEDULER_PRI_MAX; i++)
        {
            pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ausPriWeightedNum[i] = ADS_UL_DEFAULT_PRI_WEIGHTED_NUM;
        }
    }

    pstAdsSpecCtx->stAdsUlCtx.ulUlMaxQueueLength     = ADS_UL_MAX_QUEUE_LENGTH;

    return;
}


VOS_VOID ADS_InitDlCtx(VOS_UINT32 ulInstance)
{
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx = VOS_NULL_PTR;
    VOS_UINT32                          i;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstance]);

    /* 初始化下行的RAB信息 */
    for (i = 0; i < ADS_RAB_NUM_MAX; i++)
    {
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].ulRabId              = ADS_RAB_ID_INVALID;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].enPktType            = ADS_CDS_IPF_PKT_TYPE_IP;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].ulExParam            = 0;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pRcvDlDataFunc       = VOS_NULL_PTR;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pRcvDlFilterDataFunc = VOS_NULL_PTR;
#if (FEATURE_ON == FEATURE_RNIC_NAPI_GRO)
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pRcvRdLstDataFunc    = VOS_NULL_PTR;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pAdjNapiWeightFunc   = VOS_NULL_PTR;
#endif
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pstLstPkt            = VOS_NULL_PTR;
    }

    return;
}


VOS_VOID ADS_InitStatsInfoCtx(VOS_VOID)
{
    ADS_STATS_INFO_CTX_STRU            *pstDsFlowStatsCtx = VOS_NULL_PTR;

    pstDsFlowStatsCtx = ADS_GET_DSFLOW_STATS_CTX_PTR();

    pstDsFlowStatsCtx->stULDataStats.ulULPeriodSndBytes = 0;
    pstDsFlowStatsCtx->stULDataStats.ulULCurDataRate    = 0;
    pstDsFlowStatsCtx->stDLDataStats.ulDLPeriodRcvBytes = 0;
    pstDsFlowStatsCtx->stDLDataStats.ulDLCurDataRate    = 0;
}


VOS_VOID ADS_InitSpecCtx(VOS_VOID)
{
    VOS_UINT32                           i;

    for (i = 0; i < ADS_INSTANCE_MAX_NUM; i++)
    {
        /* 初始化上行上下文 */
        ADS_InitUlCtx(i);

        /* 初始化下行上下文 */
        ADS_InitDlCtx(i);
    }

    /* 初始化底软动态组包参数 */
    ADS_DL_InitFcAssemParamInfo();
}


VOS_VOID ADS_ResetSpecUlCtx(VOS_UINT32 ulInstance)
{
    VOS_UINT32                          i;
    VOS_UINT32                          ulRst;
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx = VOS_NULL_PTR;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstance]);

    /* 默认从第一个队列开始调度 */
    pstAdsSpecCtx->stAdsUlCtx.ulAdsUlCurIndex      = 0;

    for (i = 0; i < ADS_RAB_ID_MAX + 1; i++)
    {
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].pstAdsUlLink    = VOS_NULL_PTR;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].ucIsQueueValid  = VOS_FALSE;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].enPrio          = ADS_QCI_TYPE_BUTT;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].usRecordNum     = 0;
        pstAdsSpecCtx->stAdsUlCtx.astAdsUlQueue[i].enPktType       = ADS_CDS_IPF_PKT_TYPE_IP;
    }

    TAF_MEM_SET_S(pstAdsSpecCtx->stAdsUlCtx.aulPrioIndex, sizeof(pstAdsSpecCtx->stAdsUlCtx.aulPrioIndex),
                  0x00, sizeof(pstAdsSpecCtx->stAdsUlCtx.aulPrioIndex));

    /* 读NV，将优先级加权数读写到ADS上下文中 */
    ulRst = TAF_ACORE_NV_READ(ulInstance,
                              en_NV_Item_ADS_Queue_Scheduler_Pri,
                              &(pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv),
                              sizeof(ADS_UL_QUEUE_SCHEDULER_PRI_NV_STRU));
    if(NV_OK != ulRst)
    {
        pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ulStatus = VOS_FALSE;

        for (i = 0; i < ADS_UL_QUEUE_SCHEDULER_PRI_MAX; i++)
        {
            pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ausPriWeightedNum[i] = ADS_UL_DEFAULT_PRI_WEIGHTED_NUM;
        }

        ADS_ERROR_LOG(ACPU_PID_ADS_UL, "ADS_InitUlCtx: NV read failed !");
    }

    if (VOS_FALSE == pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ulStatus)
    {
        for (i = 0; i < ADS_UL_QUEUE_SCHEDULER_PRI_MAX; i++)
        {
            pstAdsSpecCtx->stAdsUlCtx.stQueuePriNv.ausPriWeightedNum[i] = ADS_UL_DEFAULT_PRI_WEIGHTED_NUM;
        }
    }

    pstAdsSpecCtx->stAdsUlCtx.ulUlMaxQueueLength     = ADS_UL_MAX_QUEUE_LENGTH;

    return;
}


VOS_VOID ADS_ResetSpecDlCtx(VOS_UINT32 ulInstance)
{
    ADS_SPEC_CTX_STRU                  *pstAdsSpecCtx  = VOS_NULL_PTR;
    VOS_UINT32                          i;

    pstAdsSpecCtx = &(g_stAdsCtx.astAdsSpecCtx[ulInstance]);

    /* 重置下行的RAB信息 */
    for (i = 0; i < ADS_RAB_NUM_MAX; i++)
    {
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].ulRabId              = ADS_RAB_ID_INVALID;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].enPktType            = ADS_CDS_IPF_PKT_TYPE_IP;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].ulExParam            = 0;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pRcvDlDataFunc       = VOS_NULL_PTR;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pRcvDlFilterDataFunc = VOS_NULL_PTR;
#if (FEATURE_ON == FEATURE_RNIC_NAPI_GRO)
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pRcvRdLstDataFunc    = VOS_NULL_PTR;
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pAdjNapiWeightFunc   = VOS_NULL_PTR;
#endif
        pstAdsSpecCtx->stAdsDlCtx.astAdsDlRabInfo[i].pstLstPkt            = VOS_NULL_PTR;
    }

    return;
}


VOS_VOID ADS_ResetUlCtx(VOS_VOID)
{
    VOS_UINT32                           i;

    for (i = 0; i < ADS_INSTANCE_MAX_NUM; i++)
    {
        /* 初始化上行上下文 */
        ADS_ResetSpecUlCtx(i);
    }

    /* 初始化数据统计的上下文 */
    ADS_InitStatsInfoCtx();

    return;
}


VOS_VOID ADS_ResetDlCtx(VOS_VOID)
{
    VOS_UINT32                           i;

    for (i = 0; i < ADS_INSTANCE_MAX_NUM; i++)
    {
        /* 初始化下行上下文 */
        ADS_ResetSpecDlCtx(i);
    }

    /* 重置底软动态组包参数 */
    ADS_DL_ResetFcAssemParamInfo();

    return;
}


VOS_VOID ADS_ResetIpfCtx(VOS_VOID)
{
    /* 默认上行数据发送保护定时器时长为10ms */
    g_stAdsCtx.stAdsIpfCtx.ulProtectTmrLen   = 10;

    /* 默认上行数据统计定时器时长为100ms */
    g_stAdsCtx.stAdsIpfCtx.stUlAssemParmInfo.stThresholdStatInfo.ulStatTmrLen = 100;
    g_stAdsCtx.stAdsIpfCtx.stUlAssemParmInfo.stThresholdStatInfo.ulStatPktNum = 0;

    /* 默认下行C核单独复位延时定时器时长为1s */
    g_stAdsCtx.stAdsIpfCtx.ulCCoreResetDelayTmrLen = 1000;

    /* 默认攒包的最大个数 */
    if (VOS_TRUE == g_stAdsCtx.stAdsIpfCtx.stUlAssemParmInfo.ulActiveFlag)
    {
        g_stAdsCtx.stAdsIpfCtx.ulThredHoldNum = ADS_UL_DATA_THRESHOLD_ONE;
    }
    else
    {
        g_stAdsCtx.stAdsIpfCtx.ulThredHoldNum = 32;
    }

    /* 默认数据不在发送 */
    g_stAdsCtx.stAdsIpfCtx.ucSendingFlg = VOS_FALSE;
}


VOS_VOID ADS_InitIpfCtx(VOS_VOID)
{
    ADS_UL_DYNAMIC_ASSEM_INFO_STRU     *pstUlAssemParmInfo = VOS_NULL_PTR;
    ADS_NV_DYNAMIC_THRESHOLD_STRU       stThreshold;
    TAF_NV_ADS_WAKE_LOCK_CFG_STRU       stWakeLockCfg;
    TAF_NV_ADS_IPF_MODE_CFG_STRU        stIpfMode;
    VOS_UINT32                          ulRet;
    VOS_UINT32                           i;

    for (i = 0; i < ADS_DL_ADQ_MAX_NUM; i++)
    {
        TAF_MEM_SET_S(g_stAdsCtx.stAdsIpfCtx.astIpfDlAdDesc[i], (VOS_SIZE_T)(IPF_DLAD0_DESC_SIZE * sizeof(IPF_AD_DESC_S)), 0x00, (VOS_SIZE_T)(IPF_DLAD0_DESC_SIZE * sizeof(IPF_AD_DESC_S)));
    }

    TAF_MEM_SET_S(ADS_DL_GET_IPF_RD_RECORD_PTR(),
                  sizeof(ADS_IPF_RD_RECORD_STRU),
                  0x00,
                  sizeof(ADS_IPF_RD_RECORD_STRU));

    /* 初始化上行源内存释放队列 */
    IMM_ZcQueueHeadInit(&g_stAdsCtx.stAdsIpfCtx.stUlSrcMemFreeQue);

    /* 初始化上行BD BUFF*/
    TAF_MEM_SET_S(g_stAdsCtx.stAdsIpfCtx.astIpfUlBdCfgParam, sizeof(g_stAdsCtx.stAdsIpfCtx.astIpfUlBdCfgParam), 0x00, (VOS_SIZE_T)(IPF_ULBD_DESC_SIZE * sizeof(IPF_CONFIG_ULPARAM_S)));

    /* 初始化下行RD BUFF*/
    TAF_MEM_SET_S(g_stAdsCtx.stAdsIpfCtx.astIpfDlRdDesc, sizeof(g_stAdsCtx.stAdsIpfCtx.astIpfDlRdDesc), 0x00, (VOS_SIZE_T)(IPF_DLRD_DESC_SIZE * sizeof(IPF_RD_DESC_S)));

    TAF_MEM_SET_S(&stWakeLockCfg, sizeof(stWakeLockCfg), 0x00, sizeof(TAF_NV_ADS_WAKE_LOCK_CFG_STRU));

    /* 默认上行数据发送保护定时器时长为10ms */
    g_stAdsCtx.stAdsIpfCtx.ulProtectTmrLen   = 10;

    /* 默认下行C核单独复位延时定时器时长为1s */
    g_stAdsCtx.stAdsIpfCtx.ulCCoreResetDelayTmrLen = 1000;

    pstUlAssemParmInfo = &g_stAdsCtx.stAdsIpfCtx.stUlAssemParmInfo;

    TAF_MEM_SET_S(&stThreshold, sizeof(stThreshold), 0x00, (VOS_SIZE_T)sizeof(ADS_NV_DYNAMIC_THRESHOLD_STRU));

    ulRet = TAF_ACORE_NV_READ(MODEM_ID_0,
                              en_NV_Item_ADS_DYNAMIC_THRESHOLD_CFG,
                              &stThreshold,
                              sizeof(ADS_NV_DYNAMIC_THRESHOLD_STRU));
    if(NV_OK != ulRet)
    {
        pstUlAssemParmInfo->ulActiveFlag                      = VOS_FALSE;
        pstUlAssemParmInfo->ulProtectTmrExpCnt                = 0;
        pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel1    = 80;
        pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel2    = 150;
        pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel3    = 500;
        pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel4    = 0xFFFFFFFF;

        pstUlAssemParmInfo->stThresholdLevel.ulThreshold1     = 1;
        pstUlAssemParmInfo->stThresholdLevel.ulThreshold2     = 13;
        pstUlAssemParmInfo->stThresholdLevel.ulThreshold3     = 60;
        pstUlAssemParmInfo->stThresholdLevel.ulThreshold4     = 64;
        ADS_ERROR_LOG(ACPU_PID_ADS_UL, "ADS_InitIpfCtx: NV read failed !");
    }

    pstUlAssemParmInfo->ulActiveFlag                      = stThreshold.ulActiveFlag;
    pstUlAssemParmInfo->ulProtectTmrExpCnt                = stThreshold.ulProtectTmrExpCnt;
    pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel1    = stThreshold.stWaterMarkLevel.ulWaterLevel1;
    pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel2    = stThreshold.stWaterMarkLevel.ulWaterLevel2;
    pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel3    = stThreshold.stWaterMarkLevel.ulWaterLevel3;
    pstUlAssemParmInfo->stWaterMarkLevel.ulWaterLevel4    = stThreshold.stWaterMarkLevel.ulWaterLevel4;

    pstUlAssemParmInfo->stThresholdLevel.ulThreshold1     = stThreshold.stThresholdLevel.ulThreshold1;
    pstUlAssemParmInfo->stThresholdLevel.ulThreshold2     = stThreshold.stThresholdLevel.ulThreshold2;
    pstUlAssemParmInfo->stThresholdLevel.ulThreshold3     = stThreshold.stThresholdLevel.ulThreshold3;
    pstUlAssemParmInfo->stThresholdLevel.ulThreshold4     = stThreshold.stThresholdLevel.ulThreshold4;

    /* 默认上行数据统计定时器时长为100ms */
    pstUlAssemParmInfo->stThresholdStatInfo.ulStatTmrLen = 100;
    pstUlAssemParmInfo->stThresholdStatInfo.ulStatPktNum = 0;

    /* 超时时长大于零才需要启动jiffies保护定时器 */
    if (0 != pstUlAssemParmInfo->ulProtectTmrExpCnt)
    {
        pstUlAssemParmInfo->ulProtectTmrCnt = ADS_CURRENT_TICK;
    }

    /* 默认攒包的最大个数 */
    if (VOS_TRUE == pstUlAssemParmInfo->ulActiveFlag)
    {
        g_stAdsCtx.stAdsIpfCtx.ulThredHoldNum = ADS_UL_DATA_THRESHOLD_ONE;
    }
    else
    {
        g_stAdsCtx.stAdsIpfCtx.ulThredHoldNum = 32;
    }

    /* 默认数据不在发送 */
    g_stAdsCtx.stAdsIpfCtx.ucSendingFlg = VOS_FALSE;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
    TAF_MEM_SET_S(&(g_stAdsCtx.stAdsIpfCtx.stDev), sizeof(g_stAdsCtx.stAdsIpfCtx.stDev), 0x00, (VOS_SIZE_T)sizeof(struct device));
    g_stAdsCtx.stAdsIpfCtx.stDev.dma_mask = &g_ullAdsDmaMask;
#endif

    wakeup_source_init(&g_stAdsCtx.stAdsIpfCtx.stUlBdWakeLock, "ipf_bd_wake");
    wakeup_source_init(&g_stAdsCtx.stAdsIpfCtx.stDlRdWakeLock, "ipf_rd_wake");

    wakeup_source_init(&g_stAdsCtx.stAdsIpfCtx.stRxWakeLock, "ads_rx_wake");
    wakeup_source_init(&g_stAdsCtx.stAdsIpfCtx.stTxWakeLock, "ads_tx_wake");

    g_stAdsCtx.stAdsIpfCtx.ulWakeLockEnable         = VOS_FALSE;

    g_stAdsCtx.stAdsIpfCtx.ulUlBdWakeLockCnt        = 0;
    g_stAdsCtx.stAdsIpfCtx.ulDlRdWakeLockCnt        = 0;

    g_stAdsCtx.stAdsIpfCtx.ulRxWakeLockTimeout      = 0;
    g_stAdsCtx.stAdsIpfCtx.ulTxWakeLockTimeout      = 0;

    g_stAdsCtx.stAdsIpfCtx.ulTxWakeLockTmrLen       = 500;
    g_stAdsCtx.stAdsIpfCtx.ulRxWakeLockTmrLen       = 500;

    ulRet = TAF_ACORE_NV_READ(MODEM_ID_0,
                              en_NV_Item_ADS_WAKE_LOCK_CFG,
                              &stWakeLockCfg,
                              sizeof(TAF_NV_ADS_WAKE_LOCK_CFG_STRU));
    if (NV_OK == ulRet)
    {
        g_stAdsCtx.stAdsIpfCtx.ulWakeLockEnable     = stWakeLockCfg.ulEnable;
        g_stAdsCtx.stAdsIpfCtx.ulTxWakeLockTmrLen   = stWakeLockCfg.ulTxWakeTimeout;
        g_stAdsCtx.stAdsIpfCtx.ulRxWakeLockTmrLen   = stWakeLockCfg.ulRxWakeTimeout;
    }

    ulRet = TAF_ACORE_NV_READ(MODEM_ID_0,
                              en_NV_Item_ADS_IPF_MODE_CFG,
                              &stIpfMode,
                              (VOS_UINT32)sizeof(TAF_NV_ADS_IPF_MODE_CFG_STRU));
    if (NV_OK == ulRet)
    {
        g_stAdsCtx.stAdsIpfCtx.ucIpfMode = stIpfMode.ucIpfMode;
    }

    IMM_ZcQueueHeadInit(ADS_GET_IPF_FILTER_QUE());

#if (FEATURE_ON == FEATURE_RNIC_NAPI_GRO)
    VOS_SpinLockInit(&(g_stAdsCtx.stAdsIpfCtx.stAdqSpinLock));
#endif

    return;
}


VOS_VOID ADS_InitTiCtx(VOS_VOID)
{
    VOS_UINT32                          i;

    for (i = 0; i < ADS_MAX_TIMER_NUM; i++)
    {
        g_stAdsCtx.astAdsTiCtx[i].hTimer        = VOS_NULL_PTR;
    }

    return;
}


VOS_VOID ADS_InitResetSem(VOS_VOID)
{
    g_stAdsCtx.hULResetSem  = VOS_NULL_PTR;
    g_stAdsCtx.hDLResetSem  = VOS_NULL_PTR;

    /* 分配二进制信号量 */
    if (VOS_OK != VOS_SmBCreate( "UL", 0, VOS_SEMA4_FIFO, &g_stAdsCtx.hULResetSem))
    {
        ADS_TRACE_HIGH("Create ADS acpu UL_CNF sem failed!\n");
        ADS_DBG_UL_RESET_CREATE_SEM_FAIL_NUM(1);
        return;
    }

    if (VOS_OK != VOS_SmBCreate( "DL", 0, VOS_SEMA4_FIFO, &g_stAdsCtx.hDLResetSem))
    {
        ADS_TRACE_HIGH("Create ADS acpu DL_CNF sem failed!\n");
        ADS_DBG_DL_RESET_CREATE_SEM_FAIL_NUM(1);
        return;
    }

    return;
}


VOS_VOID ADS_ReadPacketErrorFeedbackCongfigNV(VOS_VOID)
{
    ADS_PACKET_ERROR_FEEDBACK_CFG_STRU *pstConfig = VOS_NULL_PTR;
    TAF_NV_ADS_ERROR_FEEDBACK_CFG_STRU  stNvConfig;
    VOS_UINT32                          ulRet;

    TAF_MEM_SET_S(&stNvConfig, sizeof(stNvConfig),
                  0x00, sizeof(TAF_NV_ADS_ERROR_FEEDBACK_CFG_STRU));

    /* 默认配置 */
    pstConfig = ADS_DL_GET_PKT_ERR_FEEDBACK_CFG_PTR();
    pstConfig->ulEnabled = 0;

    pstConfig->ulPktErrRateThres   = ADS_PKT_ERR_RATE_DEFAULT_THRESHOLD;
    pstConfig->ulMinDetectDuration = msecs_to_jiffies(ADS_PKT_ERR_DETECT_DEFAULT_DURATION);
    pstConfig->ulMaxDetectDuration = msecs_to_jiffies(ADS_PKT_ERR_DETECT_DEFAULT_DURATION +
                                                ADS_PKT_ERR_DETECT_DEFAULT_DELTA);

    /* 读取NV配置 */
    ulRet = TAF_ACORE_NV_READ(MODEM_ID_0, en_NV_Item_ADS_PACKET_ERROR_FEEDBACK_CFG,
                      &stNvConfig, sizeof(TAF_NV_ADS_ERROR_FEEDBACK_CFG_STRU));
    if (NV_OK == ulRet)
    {
        pstConfig->ulEnabled = stNvConfig.ulEnabled;

        if (ADS_IS_PKT_ERR_RATE_THRESHOLD_VALID(stNvConfig.ulErrorRateThreshold))
        {
            pstConfig->ulPktErrRateThres   = stNvConfig.ulErrorRateThreshold;
        }

        if (ADS_IS_PKT_ERR_DETECT_DURATION_VALID(stNvConfig.ulDetectDuration))
        {
            pstConfig->ulMinDetectDuration = msecs_to_jiffies(stNvConfig.ulDetectDuration);
            pstConfig->ulMaxDetectDuration = msecs_to_jiffies(stNvConfig.ulDetectDuration +
                                                ADS_PKT_ERR_DETECT_DEFAULT_DELTA);
        }
    }

    return;
}


VOS_VOID ADS_InitCtx(VOS_VOID)
{
    TAF_MEM_SET_S(&g_stAdsStats, sizeof(g_stAdsStats), 0x00, sizeof(g_stAdsStats));

    /* 初始化每个实例的上下文 */
    ADS_InitSpecCtx();

    /* 初始化数据统计的上下文 */
    ADS_InitStatsInfoCtx();

    /* 初始化IPF相关的上下文 */
    ADS_InitIpfCtx();

    /* 初始化定时器上下文 */
    ADS_InitTiCtx();

    /* 初始化复位信号量 */
    ADS_InitResetSem();

    /* 初始化ADS过滤器上下文 */
    ADS_FILTER_InitCtx();

    /* 初始化当前实例索引值 */
    g_stAdsCtx.ulAdsCurInstanceIndex = ADS_INSTANCE_INDEX_0;

    /* 读取错包反馈配置 */
    ADS_ReadPacketErrorFeedbackCongfigNV();

    return;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))

VOS_INT32 ADS_PlatDevProbe(struct platform_device *pstDev)
{
    g_pstDmaDev = &(pstDev->dev);
    dma_set_mask_and_coherent(g_pstDmaDev, DMA_BIT_MASK(64));

    printk(KERN_ERR"ADS_PlatDevProbe: dma mask = 0x%llx, coherent_dma_mask = 0x%llx, archdata.dma_coherent = %d.\r\n",
           *(g_pstDmaDev->dma_mask), g_pstDmaDev->coherent_dma_mask, g_pstDmaDev->archdata.dma_coherent);

    return 0;
}


VOS_INT32 ADS_PlatDevRemove(struct platform_device *pstDev)
{
    return 0;
}

static const struct of_device_id g_stAdsPlatDevOfMatch[] = {
	{
		.compatible = "hisilicon,hisi-ads",
	},
	{ },
};

static struct platform_driver g_stAdsPlatDevDriver = {
	.probe	= ADS_PlatDevProbe,
	.remove	= ADS_PlatDevRemove,
	.driver	= {
		.name = "hisi-ads",
		.of_match_table = of_match_ptr(g_stAdsPlatDevOfMatch),
	},
};


VOS_INT32 __init ADS_PlatDevInit(void)
{
	return platform_driver_register(&g_stAdsPlatDevDriver);
}


VOS_VOID __exit ADS_PlatDevExit(void)
{
	platform_driver_unregister(&g_stAdsPlatDevDriver);
}

module_init(ADS_PlatDevInit);
module_exit(ADS_PlatDevExit);
#endif


