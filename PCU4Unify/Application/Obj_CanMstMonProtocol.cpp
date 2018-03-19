
#include "Publics.h"

void Class_CanMasterMonPrtcl::Dat_Init(void)
{
	//队列初始化
	m_CanRecvQueue.CanRecvQueueInit();
	m_CanXmitQueue.CanXmitQueueInit();

	m_u32_SeriNumRecvFromMon = 0xffffffff;
	m_u16_LogicIdBuff = 0xffff;
	m_u16_FrameType = 0xff;//初始化成0xff,获取到有效值为1或者0

	m_u16_CanXmitBusy = 0;

}

/***************************************************************************
函数名:SysCanXmitFcb
功能描述:协议层消息发送函数
输入参数:none
返回值:none
说明:将发送队列的消息写入CAN发送寄存器，由中断程序调用
作者:
日期:
****************************************************************************/
int16_t    Class_CanMasterMonPrtcl::CanXmitFcb(void *pUnused)
{
	CanTxMsg tempPacked;

	//队列不为空
	if (E_QUEUE_NULL != m_CanXmitQueue.FlagQueue())
	{
		tempPacked = m_CanXmitQueue.GetOldestNode();
		m_CanXmitQueue.DelQueue();
		
		m_u16_CanXmitBusy = TRUE;
		//发送数据
		mcp2515_tx0_extend(&tempPacked);

	}
	else
	{

		//clear sending flag
		m_u16_CanXmitBusy = FALSE;
	}
	return 0;
}


/***************************************************************************
函数名:SysCanRecvFcb
功能描述:协议层消息发送函数
输入参数:none
返回值:none
说明:将发送队列的消息写入CAN发送寄存器，由中断程序调用
作者:
日期:
****************************************************************************/
int16_t    Class_CanMasterMonPrtcl::CanRecvFcb(IN CanRxMsg *pMsg)
{
	m_CurRecivedFrame.Frame = *pMsg;
	if((m_CurRecivedFrame.PackedMsg.ExtId.bPF== PROTNO_UPGRADE
		||m_CurRecivedFrame.PackedMsg.ExtId.bPF == PROTNO_DATATSFR
		||m_CurRecivedFrame.PackedMsg.ExtId.bPF == PROTNO_CNCTMNGT
		||m_CurRecivedFrame.PackedMsg.ExtId.bPF == PROTNO_CODE_OVER
		|| m_CurRecivedFrame.PackedMsg.ExtId.bPF == PROTNO_CODEOVER)
		&&(m_CurRecivedFrame.PackedMsg.ExtId.bPS == objPcuModel.m_i16_SelfAddr))
	{
		objUpgrade.m_CanRecvQueue.InsQueue(m_CurRecivedFrame.Frame);
		objUpgrade.u16UpgdOffCnt = 0;
	}
	else if ((m_CurRecivedFrame.PackedMsg.ExtId.bPS == objPcuModel.m_i16_SelfAddr)
				&&( m_CurRecivedFrame.PackedMsg.ExtId.bPF == PROTNO_TPCM))
	{
		//其他长针包
		objCanMstMonLongFrame.m_CanRecvQueue.InsQueue(m_CurRecivedFrame.Frame);
	}
/*				
	else if((m_CurRecivedFrame.PackedMsg.ExtId.bPS >= MAC_ID_PV1 && m_CurRecivedFrame.PackedMsg.ExtId.bPS <= MAC_ID_PV7
				&& objPDU.m_i16_SelfAddr >= MAC_ID_PV1 && objPDU.m_i16_SelfAddr <= MAC_ID_PV7)
			&& (m_CurRecivedFrame.PackedMsg.ExtId.bPF == PROTNO_TPCM))
	{
		//其他长针包
		objCanMstMonLongFrame.m_CanRecvQueue.InsQueue(m_CurRecivedFrame.Frame);
	}
	*///comment by hexg 2017.11.15
	else if ((m_CurRecivedFrame.PackedMsg.ExtId.bPS == objPcuModel.m_i16_SelfAddr)
		||(m_CurRecivedFrame.PackedMsg.ExtId.bPS >= MAC_ID_PDU0&&m_CurRecivedFrame.PackedMsg.ExtId.bPS <= MAC_ID_PDU31)
	//	||(m_CurRecivedFrame.PackedMsg.ExtId.bPS >= MAC_ID_PV1&&m_CurRecivedFrame.PackedMsg.ExtId.bPS <= MAC_ID_PV7)
		|| (m_CurRecivedFrame.PackedMsg.ExtId.bPS == 0xff))

	{
		m_CanRecvQueue.InsQueue(m_CurRecivedFrame.Frame);
	}
	

	return 0;
}

/***************************************************************************
函数名:MsgRecvProc
功能描述:设置量请求消息重发标志
作者:
日期:
****************************************************************************/
int16_t    Class_CanMasterMonPrtcl::MsgRecvProc(OUT _CanMstMonAppMsgDef *pApdu)
{
	int16_t ret = 0;
	//CanRxMsg tempPacked;
	_CanMstMonRxProtocolDef tempPacked;

	//queue is not null
	while(E_QUEUE_NULL != m_CanRecvQueue.FlagQueue())
	{
		//接收队列里面已经有比较老的数据，接收后删除该数据
		tempPacked.Frame = m_CanRecvQueue.GetOldestNode();

		m_CanRecvQueue.DelQueue();

		//把接收协议层的数据转换成应用层的数据，处理后的应用层数据
		//赋值给指针m_NonFragRecvMsg.HeadInfo，最后返回给指针pApdu
		FrameParseToApdu(pApdu, &tempPacked);

		ret = sizeof(_CanMstMonAppMsgDef);
		return ret;
	}

	return ret;
}

/***************************************************************************
函数名:MsgXmitProc
功能描述:消息接收处理，应用层接口，读取队列信息，发送帧转换到协议层的解析
输入参数:协议层消息
作者:
日期:
****************************************************************************/
int16_t    Class_CanMasterMonPrtcl::MsgXmitProc(_CanMstMonAppMsgDef *pApdu)
{
	int16_t ret = 0;
	//CanTxMsg tempFrame;
	_CanMstMonTxProtocolDef tempFrame;

	FrameParseToPpdu(pApdu,&tempFrame);

	
	ret = XmitFramePost(&(tempFrame.Frame));

	return ret;
}

/***************************************************************************
函数名:FrameParseToApdu
功能描述:驱动协议层到应用层消息映射
作者:
日期:
****************************************************************************/
void    Class_CanMasterMonPrtcl::FrameParseToApdu(INOUT _CanMstMonAppMsgDef *pApdu,IN _CanMstMonRxProtocolDef *pFrame)
{
 	int16_t i;

	//注意，如果说这块需要接收连续的多帧数据，则需要进行进一步解析组合数据

	pApdu->u16DestId = pFrame->PackedMsg.ExtId.bPS; //目的地址接收
	pApdu->u16MACId = pFrame->PackedMsg.ExtId.bSourceAddr; //源地址接收


	pApdu->u16PGN = ((uint16_t)(pFrame->PackedMsg.ExtId.bPF))<<8;

	pApdu->u16DataLen = pFrame->PackedMsg.DLC;
	for (i = 0; i < pApdu->u16DataLen; i++)
	{
		pApdu->pDataPtr[i] = pFrame->PackedMsg.MsgData[i];
	}
}

/***************************************************************************
函数名:FrameParseToPpdu
功能描述:发送消息从应用层到协议驱动层的转换
作者:
日期:
****************************************************************************/
void    Class_CanMasterMonPrtcl::FrameParseToPpdu(OUT _CanMstMonAppMsgDef *pApdu,IN _CanMstMonTxProtocolDef *pFrame)
{
 	int16_t i;

	pFrame->PackedMsg.ExtId.bDataPage = 0;
	pFrame->PackedMsg.ExtId.bPriority = pApdu->u16Priority;
	pFrame->PackedMsg.ExtId.bRev = 0;
	pFrame->PackedMsg.ExtId.bR = 0;
	pApdu->u16DestId = 0xfa;
  
	pFrame->PackedMsg.ExtId.bSourceAddr = pApdu->u16MACId;	//PCU 100

        
	pFrame->PackedMsg.ExtId.bPF = ((pApdu->u16PGN & 0xFF00) >> 8);
	pFrame->PackedMsg.ExtId.bPS = pApdu->u16DestId;

	pFrame->PackedMsg.DLC = pApdu->u16DataLen;
	pFrame->PackedMsg.IDE = CAN_ID_EXT;
	pFrame->PackedMsg.RTR = CAN_RTR_DATA;

	for (i = 0; i < pApdu->u16DataLen; i++)
	{

		pFrame->PackedMsg.MsgData[i] = pApdu->pDataPtr[i];


	}
}

/***************************************************************************
函数名:XmitFramePost
功能描述:消息映射，协议层数据消息帧发送，
说明:存入新的待发送消息到队列，并从队列中取出老的消息，放入CAN发送寄存器；
由应用层调用
作者:
日期:
****************************************************************************/
int16_t    Class_CanMasterMonPrtcl::XmitFramePost(IN CanTxMsg* pCanPacked)
{
 	CanTxMsg tempPacked = *pCanPacked;

	//BSP_CAN1_CloseInt();
	//m_u16_CanXmitBusy = FALSE;
	if (E_QUEUE_FULL == m_CanXmitQueue.FlagQueue())
	{
		//m_u16_CanXmitBusy = FALSE;
		return E_QUEUE_FULL;
	}
	else
	{
		m_CanXmitQueue.InsQueue(tempPacked);
	}

	if (FALSE == m_u16_CanXmitBusy)
	{
		m_u16_CanXmitBusy = TRUE;

		tempPacked = m_CanXmitQueue.GetOldestNode();

		m_CanXmitQueue.DelQueue();
		//SysCanSendData(&(tempPacked.Frame));
		//BSP_Can1_TranMsg(&tempPacked);
		mcp2515_tx0_extend(&tempPacked);
	}
	//BSP_CAN1_OpenInt();
	return 0;
}


