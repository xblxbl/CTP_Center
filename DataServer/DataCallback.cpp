#include "DataServer.h"

//----------------CallBack--------------------

//深度行情回报
void __stdcall DataServer::OnRtnDepthMarketData(void* pMdUserApi, CThostFtdcDepthMarketDataField *pDepthMarketData)
{
    static const double INF = 1e+100;
    if (hasData.count(pDepthMarketData->InstrumentID) == 0)
    {
        hasData.insert(pDepthMarketData->InstrumentID);
        return;
    }
    BSONObjBuilder b;
    b.appendDate("UpdateTime", Date_t(GetEpochTime(pDepthMarketData->TradingDay, pDepthMarketData->UpdateTime, pDepthMarketData->UpdateMillisec)));
    b.append("InstrumentID", pDepthMarketData->InstrumentID);
    b.append("OpenPrice", pDepthMarketData->OpenPrice > INF ? -1 : pDepthMarketData->OpenPrice);
    b.append("HighestPrice", pDepthMarketData->HighestPrice > INF ? -1 : pDepthMarketData->HighestPrice);
    b.append("LowestPrice", pDepthMarketData->LowestPrice > INF ? -1 : pDepthMarketData->LowestPrice);
    b.append("ClosePrice", pDepthMarketData->ClosePrice > INF ? -1 : pDepthMarketData->ClosePrice);
    b.append("LastPrice", pDepthMarketData->LastPrice > INF ? -1 : pDepthMarketData->LastPrice);
    b.append("Volume", pDepthMarketData->Volume);
    b.append("AskPrice1", pDepthMarketData->AskPrice1 > INF ? -1 : pDepthMarketData->AskPrice1);
    b.append("BidPrice1", pDepthMarketData->BidPrice1 > INF ? -1 : pDepthMarketData->BidPrice1);
    b.append("AskVolume1", pDepthMarketData->AskVolume1);
    b.append("BidVolume1", pDepthMarketData->BidVolume1);
    b.append("Turnover", pDepthMarketData->Turnover > INF ? -1 : pDepthMarketData->Turnover);
    b.append("UpperLimitPrice", pDepthMarketData->UpperLimitPrice > INF ? -1 : pDepthMarketData->UpperLimitPrice);
    b.append("LowerLimitPrice", pDepthMarketData->LowerLimitPrice > INF ? -1 : pDepthMarketData->LowerLimitPrice);
    b.append("AveragePrice", pDepthMarketData->AveragePrice > INF ? -1 : pDepthMarketData->AveragePrice);
    b.append("PreSettlementPrice", pDepthMarketData->PreSettlementPrice > INF ? -1 : pDepthMarketData->PreSettlementPrice);
    b.append("PreClosePrice", pDepthMarketData->PreClosePrice > INF ? -1 : pDepthMarketData->PreClosePrice);
    b.append("PreOpenInterest", pDepthMarketData->PreOpenInterest > INF ? -1 : pDepthMarketData->PreOpenInterest);
    b.append("OpenInterest", pDepthMarketData->OpenInterest > INF ? -1 : pDepthMarketData->OpenInterest);
    b.append("SettlementPrice", pDepthMarketData->SettlementPrice > INF ? -1 : pDepthMarketData->SettlementPrice);
    pCon->insert(database + ".tick", b.done());

    UpdateBar(pDepthMarketData);
}

void DataServer::UpdateBar(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
    BSONObjBuilder b;
    Date_t time = Date_t(GetBarTime(pDepthMarketData->TradingDay, pDepthMarketData->UpdateTime));
    b.append("instrument", pDepthMarketData->InstrumentID);
    b.append("type", 1);
    b.append("time", time);
    BSONObj qry = b.done();

    auto_ptr<DBClientCursor> cursor;
    cursor = pCon->query(database + ".bar", qry);
    BSONObjBuilder bsonbar;
    bsonbar.append("instrument", pDepthMarketData->InstrumentID);
    bsonbar.append("time", time);
    bsonbar.append("type", 1);
    bsonbar.append("v", pDepthMarketData->Volume);
    bsonbar.append("i", pDepthMarketData->OpenInterest);
        
    if (cursor->more())
    {
        BSONObj p = cursor->next();
        bsonbar.append("o", p["o"].Double());
        bsonbar.append("h", p["h"].Double() > pDepthMarketData->LastPrice ? p["h"].Double() : pDepthMarketData->LastPrice);
        bsonbar.append("l", p["l"].Double() < pDepthMarketData->LastPrice ? p["l"].Double() : pDepthMarketData->LastPrice);
        bsonbar.append("c", pDepthMarketData->LastPrice);
        
        pCon->update(database + ".bar", qry, bsonbar.done());
    }
    else
    {
        bsonbar.append("o", pDepthMarketData->LastPrice);
        bsonbar.append("h", pDepthMarketData->LastPrice);
        bsonbar.append("l",  pDepthMarketData->LastPrice);
        bsonbar.append("c", pDepthMarketData->LastPrice);
        pCon->insert(database + ".bar", bsonbar.done());
    }
}
void __stdcall DataServer::OnRspQryInstrument(void* pTraderApi, CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID)
        dblog->PrintLog(GetPortMsg(pTraderApi) + ": " + pRspInfo->ErrorMsg, "error");
    lock_guard<std::mutex> cl(cs_instrument);
    insts.insert(pInstrument->InstrumentID);

    if (bIsLast)
        SetEvent(h_instrumentGeted);

    auto_ptr<DBClientCursor> cursor =
        pCon->query(database+".instrument", QUERY("InstrumentID" << pInstrument->InstrumentID));

    BSONObjBuilder b;
    b.append("InstrumentID", pInstrument->InstrumentID);
    b.append("ExchangeID", pInstrument->ExchangeID);
    b.append("InstrumentName", GBKToUTF8(pInstrument->InstrumentName));
    b.append("ExchangeInstID", pInstrument->ExchangeInstID);
    b.append("ProductID", pInstrument->ProductID);
    b.append("ProductClass", pInstrument->ProductClass);
    b.append("DeliveryYear", pInstrument->DeliveryYear);
    b.append("DeliveryMonth", pInstrument->DeliveryMonth);
    b.append("MaxMarketOrderVolume", pInstrument->MaxMarketOrderVolume);
    b.append("MinMarketOrderVolume", pInstrument->MinMarketOrderVolume);
    b.append("MaxLimitOrderVolume", pInstrument->MaxLimitOrderVolume);
    b.append("MinLimitOrderVolume", pInstrument->MinLimitOrderVolume);
    b.append("VolumeMultiple", pInstrument->VolumeMultiple);
    b.append("PriceTick", pInstrument->PriceTick);
    b.append("CreateDate", pInstrument->CreateDate);
    b.append("OpenDate", pInstrument->OpenDate);
    b.append("ExpireDate", pInstrument->ExpireDate);
    b.append("StartDelivDate", pInstrument->StartDelivDate);
    b.append("EndDelivDate", pInstrument->EndDelivDate);
    b.append("InstLifePhase", pInstrument->InstLifePhase);
    b.append("IsTrading", pInstrument->IsTrading);
    b.append("PositionType", pInstrument->PositionType);
    b.append("PositionDateType", pInstrument->PositionDateType);
    b.append("MaxMarginSideAlgorithm", pInstrument->MaxMarginSideAlgorithm);
    b.append("UnderlyingInstrID", pInstrument->UnderlyingInstrID);
    b.append("StrikePrice", pInstrument->StrikePrice);
    b.append("OptionsType", pInstrument->OptionsType);
    b.append("UnderlyingMultiple", pInstrument->UnderlyingMultiple);
    b.append("CombinationType", pInstrument->CombinationType);

    if (!cursor->more())
    {
        b.append("LongMarginRatio", pInstrument->LongMarginRatio);
        b.append("ShortMarginRatio", pInstrument->ShortMarginRatio);
        pCon->insert(database + ".instrument", b.done());
    }
    else
    {
        BSONObjBuilder bb;
        bb.append("$set", b.done());
        pCon->update(database + ".instrument", QUERY("InstrumentID" << pInstrument->InstrumentID), bb.done());
    }

}

//连接回报信息
void __stdcall DataServer::OnConnect(void* pApi, CThostFtdcRspUserLoginField *pRspUserLogin, ConnectionStatus result)
{
     dblog->PrintLog(GetPortMsg(pApi) +string("-->") + ConnectionStatusMsg(result));
}

//断开连接回报信息
void __stdcall DataServer::OnDisconnect(void* pApi, CThostFtdcRspInfoField *pRspInfo, ConnectionStatus step)
{
    dblog->PrintLog(GetPortMsg(pApi) + string("-->") + ConnectionStatusMsg(step));
}


//错误信息
void __stdcall DataServer::OnRspError(void* pApi, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID)
        dblog->PrintLog(GetPortMsg(pApi) + ": " + pRspInfo->ErrorMsg, "error");
}

//合约状态回报
void __stdcall DataServer::OnRtnInstrumentStatus(void* pTraderApi, CThostFtdcInstrumentStatusField *pInstrumentStatus)
{

}

void __stdcall DataServer::OnRspQryInstrumentMarginRate(void* pTraderApi, CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID)
        dblog->PrintLog(GetPortMsg(pTraderApi) + ": " + pRspInfo->ErrorMsg, "error");
    BSONObjBuilder b, bb;
    b.append("LongMarginRatio", pInstrumentMarginRate->LongMarginRatioByMoney);
    b.append("ShortMarginRatio", pInstrumentMarginRate->ShortMarginRatioByMoney);
    bb.append("$set", b.done());
    pCon->update(database + ".instrument", QUERY("InstrumentID" << pInstrumentMarginRate->InstrumentID), bb.done());
}

