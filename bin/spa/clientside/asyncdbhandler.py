
import threading
from spa.memqueue import CUQueue
from spa.udb import *
from spa.clientside import *
import collections
import math

class CAsyncDBHandler(CAsyncServiceHandler):
    ONE_MEGA_BYTES = 0x100000

    """
    Async database client/server just requires the following request identification numbers
    """
    idOpen = 0x7E7F
    idClose = idOpen + 1
    idBeginTrans = idClose + 1
    idEndTrans = idBeginTrans + 1
    idExecute = idEndTrans + 1
    idPrepare = idExecute + 1
    idExecuteParameters = idPrepare + 1

    """
    the request identification numbers used for message push from server to client
    """
    idDBUpdate = idExecuteParameters + 1 #server ==> client only
    idRowsetHeader = idDBUpdate + 1 #server ==> client only
    idOutputParameter = idRowsetHeader + 1 #server ==> client only

    """
    Internal request/response identification numbers used for data communication between client and server
    """
    idBeginRows = idOutputParameter + 1
    idTransferring = idBeginRows + 1
    idStartBLOB = idTransferring + 1
    idChunk = idStartBLOB + 1
    idEndBLOB = idChunk + 1
    idEndRows = idEndBLOB + 1

    """
    Whenever a data size in bytes is about twice larger than the defined value, the data will be treated in large object and transferred in chunks for reducing memory foot print
    """
    DEFAULT_BIG_FIELD_CHUNK_SIZE = 16 * 1024 #16k

    """
    A record data size in bytes is approximately equal to or slightly larger than the defined constant
    """
    DEFAULT_RECORD_BATCH_SIZE = 16 * 1024 #16k

    class Pair(object):
        def __init__(self, reqId, cb):
            self.first = reqId
            self.second = cb

    def __init__(self, serviceId):
        super(CAsyncDBHandler, self).__init__(serviceId)
        self._csDB = threading.Lock()
        self._vColInfo = CDBColumnInfoArray()
        self._strConnection = u''
        self._affected = -1
        self._dbError = 0
        self._dbErrorMsg = u''
        self._lastReqId = 0
        self._nCall = 0
        self._mapRowset = {}
        self._indexRowset = 0
        self._Blob = CUQueue()
        self._vData = []
        self._ms = tagManagementSystem.msUnknown
        self._flags = 0
        self.DBEvent = None
        self._deqResult = collections.deque()
        self._parameters = 0
        self._indexProc = 0
        self._output = 0
        self._mapParameterCall = {}

    def _GetResultHandler(self, reqId):
        if self.AttachedClientSocket.Random:
            with self._csDB:
                for one in self._deqResult:
                    if one.first == reqId:
                        self._deqResult.remove(one)
                        return one
        else:
            with self._csDB:
                if len(self._deqResult) > 0 and self._deqResult[0].first == reqId:
                    return self._deqResult.popleft()

    @property
    def Parameters(self):
        with self._csDB:
            return self._parameters

    @property
    def Outputs(self):
        with self._csDB:
            return self._output

    @property
    def LastDBErrorCode(self):
        with self._csDB:
            return self._dbError

    @property
    def DBManagementSystem(self):
        with self._csDB:
            return self._ms

    @property
    def Opened(self):
        with self._csDB:
            return self._strConnection != None and self._lastReqId > 0

    @property
    def ColumnInfo(self):
        with self._csDB:
            return self._vColInfo.list[:]

    @property
    def LastAffected(self):
        with self._csDB:
            return self._affected

    @property
    def LastDBErrorMessage(self):
        with self._csDB:
            return self._dbErrMsg

    def CleanCallbacks(self):
        with self._csDB:
            self._Clean()
        return super(CAsyncDBHandler, self).CleanCallbacks()

    def OnResultReturned(self, reqId, mc):
        if reqId == CAsyncDBHandler.idExecuteParameters or reqId == CAsyncDBHandler.idExecute:
            affected = mc.LoadLong()
            res = mc.LoadInt()
            errMsg = mc.LoadString()
            vtId = mc.LoadObject()
            fail_ok = mc.LoadULong()
            t = self._GetResultHandler(reqId)
            with self._csDB:
                self._lastReqId = reqId
                self._affected = affected
                self._dbError = res
                self._dbErrorMsg = errMsg
                if self._indexRowset in self._mapRowset:
                    self._mapRowset.pop(self._indexRowset)
                self._indexProc = 0
                if self._indexRowset in self._mapParameterCall:
                    self._mapParameterCall.pop(self._indexRowset)
                elif self.AttachedClientSocket.CountOfRequestsInQueue == 1:
                    self._mapParameterCall = {}
            if not t is None and not t.second is None:
                t.second(self, res, errMsg, affected, fail_ok, vtId)

        elif reqId == CAsyncDBHandler.idPrepare:
            res = mc.LoadInt()
            errMsg = mc.LoadString()
            parameters = mc.LoadInt()
            t = self._GetResultHandler(reqId)
            with self._csDB:
                self._lastReqId = reqId
                self._dbError = res
                self._dbErrorMsg = errMsg
                self._indexProc = 0
                self._parameters = (parameters & 0xffff)
                self._output = (parameters >> 16)
            if not t is None and not t.second is None:
                t.second(self, res, errMsg)

        elif reqId == CAsyncDBHandler.idOpen:
            res = mc.LoadInt()
            errMsg = mc.LoadString()
            ms = mc.LoadInt()
            t = self._GetResultHandler(reqId)
            with self._csDB:
                self._CleanRowset()
                self._dbError = res
                self._lastReqId = reqId
                if res == 0:
                    self._strConnection = errMsg
                    errMsg = u''
                else:
                    self._strConnection = u''
                self._dbErrorMsg = errMsg
                self._ms = ms
                self._indexProc = 0
                self._parameters = 0
                self._output = 0
                if self.AttachedClientSocket.CountOfRequestsInQueue == 1:
                    self._mapParameterCall = {}
            if not t is None and not t.second is None:
                t.second(self, res, errMsg)

        elif reqId == CAsyncDBHandler.idEndTrans:
            res = mc.LoadInt()
            errMsg = mc.LoadString()
            t = self._GetResultHandler(reqId)
            with self._csDB:
                self._lastReqId = reqId
                self._dbError = res
                self._dbErrorMsg = errMsg
                self._CleanRowset()
                if self.AttachedClientSocket.CountOfRequestsInQueue == 1:
                    self._mapParameterCall = {}
            if not t is None and not t.second is None:
                t.second(self, res, errMsg)

        elif reqId == CAsyncDBHandler.idBeginTrans:
            res = mc.LoadInt()
            errMsg = mc.LoadString()
            ms = mc.LoadInt()
            t = self._GetResultHandler(reqId)
            with self._csDB:
                self._CleanRowset()
                if res == 0:
                    self._strConnection = errMsg
                    errMsg = u''
                self._lastReqId = reqId
                self._dbError = res
                self._dbErrorMsg = errMsg
                self._ms = ms
            if not t is None and not t.second is None:
                t.second(self, res, errMsg)

        elif reqId == CAsyncDBHandler.idClose:
            res = mc.LoadInt()
            errMsg = mc.LoadString()
            t = self._GetResultHandler(reqId)
            with self._csDB:
                self._lastReqId = reqId
                self._dbError = res
                self._dbErrorMsg = errMsg
                self._CleanRowset()
                self._indexProc = 0
                self._parameters = 0
                self._output = 0
                if self.AttachedClientSocket.CountOfRequestsInQueue == 1:
                    self._mapParameterCall = {}
            if not t is None and not t.second is None:
                t.second(self, res, errMsg)

        elif reqId == CAsyncDBHandler.idRowsetHeader:
            self._Blob.SetSize(0)
            if self._Blob.MaxBufferSize > CAsyncDBHandler.ONE_MEGA_BYTES:
                self._Blob.Realloc(CAsyncDBHandler.ONE_MEGA_BYTES)
            self._vData = []
            header = None
            with self._csDB:
                self._vColInfo.LoadFrom(mc)
                self._indexRowset = mc.LoadULong()
                if mc.GetSize() > 0:
                    self._output = mc.LoadUInt()
                else:
                    self._output = 0
                if self._output == 0:
                    if self._indexRowset in self._mapRowset:
                        header = self._mapRowset.get(self._indexRowset).first
            if not header is None:
                header(self)

        elif reqId == CAsyncDBHandler.idBeginRows:
            self._Blob.SetSize(0)
            self._vData = []
            if mc.GetSize() > 0:
                with self._csDB:
                    self._indexRowset = mc.LoadULong()
        elif reqId == CAsyncDBHandler.idTransferring:
            while mc.GetSize() > 0:
                vt = mc.LoadObject()
                self._vData.append(vt)
        elif reqId == CAsyncDBHandler.idOutputParameter or reqId == CAsyncDBHandler.idEndRows:
            if mc.GetSize() > 0 or len(self._vData) > 0:
                while mc.GetSize() > 0:
                    vt = mc.LoadObject()
                    self._vData.append(vt)
                if reqId == CAsyncDBHandler.idOutputParameter:
                    with self._csDB:
                        self._output = len(self._vData)
                        if self._indexRowset in self._mapParameterCall:
                            vParam = self._mapParameterCall[self._indexRowset]
                            pos = self._parameters * self._indexProc + self._parameters - self._output
                            for obj in self._vData:
                                vParam[pos] = obj
                                pos += 1
                        self._indexProc += 1
                else:
                    row = None
                    with self._csDB:
                        if self._indexRowset in self._mapRowset:
                            row = self._mapRowset.get(self._indexRowset).second
                    if not row is None:
                        row(self, self._vData)
            self._vData = []
        elif reqId == CAsyncDBHandler.idStartBLOB:
            if mc.GetSize() > 0:
                self._Blob.SetSize(0)
                length = mc.LoadUInt()
                if length != -1 and length > self._Blob.MaxBufferSize:
                    self._Blob.Realloc(length)
                self._Blob.Push(mc.GetBuffer(), mc.GetSize())
                mc.SetSize(0)
        elif reqId == CAsyncDBHandler.idChunk:
            if mc.GetSize() > 0:
                self._Blob.Push(mc.GetBuffer(), mc.GetSize())
                mc.SetSize(0)
        elif reqId == CAsyncDBHandler.idEndBLOB:
            if mc.GetSize() > 0 or self._Blob.GetSize() > 0:
                self._Blob.Push(mc.GetBuffer(), mc.GetSize())
                mc.SetSize(0)
                vt = self._Blob.LoadObject()
                self._vData.append(vt)
        elif reqId == CAsyncDBHandler.idDBUpdate:
            if mc.GetSize() > 0:
                dbEventType = mc.LoadInt()
                dbInstance = mc.LoadString()
                dbPath = mc.LoadString()
                tablePath = mc.LoadString()
                idRow = mc.LoadObject()
                if not self.DBEvent is None:
                    self.DBEvent(self, dbEventType, dbInstance, dbPath, tablePath, idRow)
        else:
            super(CAsyncDBHandler, self).OnResultReturned(reqId, mc)

    def _Clean(self):
        self._strConnection = u''
        self._mapRowset = {}
        self._vColInfo = []
        self._lastReqId = 0
        self._Blob.SetSize(0)
        if self._Blob.MaxBufferSize > CAsyncDBHandler.DEFAULT_BIG_FIELD_CHUNK_SIZE:
            self._Blob.Realloc(CAsyncDBHandler.DEFAULT_BIG_FIELD_CHUNK_SIZE)
        self._vData = []

    def _CleanRowset(self, size = 0):
        if ((len(self._mapRowset) > 0 or len(self._vColInfo.list) > 0) and self.AttachedClientSocket.Sendable and self.AttachedClientSocket.CountOfRequestsInQueue <= size and self.AttachedClientSocket.ClientQueue.MessageCount <= size):
            self._mapRowset.Clear = {}
            self._vColInfo.Clear = []

    def Close(self, handler = None):
        """
        Notify connected remote server to close database connection string asynchronously
        :param handler: a callback for closing result, which should be OK always as long as there is network or queue available
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idClose, handler)
        buffer = CScopeUQueue.Lock()
        #don't make self._csDB locked across calling SendRequest, which may lead to client dead-lock in case a client asynchronously sends lots of requests without use of client side queue.
        with self._csDB:
            self._deqResult.append(cb)
        ok = self.SendRequest(CAsyncDBHandler.idClose, buffer, None)
        if not ok:
            with self._csDB:
                self._deqResult.remove(cb)
        CScopeUQueue.Unlock(buffer)
        return ok

    def BeginTrans(self, isolation = tagTransactionIsolation.tiReadCommited, handler = None):
        """
        Start a manual transaction with a given isolation asynchronously. Note the transaction will be associated with SocketPro client message queue if available to avoid possible transaction lose
        :param isolation: a value for transaction isolation. It defaults to tagTransactionIsolation.tiReadCommited
        :param handler: a callback for tracking its response result. It defaults to None
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idBeginTrans, handler)
        q = CScopeUQueue.Lock()
        #don't make self._csDB locked across calling SendRequest, which may lead to client dead-lock in case a client asynchronously sends lots of requests without use of client side queue.
        with self._csDB:
            q.SaveInt(isolation).SaveString(self._strConnection).SaveUInt(self._flags)
            self._deqResult.append(cb)
        self.AttachedClientSocket.ClientQueue.StartJob()
        ok = self.SendRequest(CAsyncDBHandler.idBeginTrans, q, None)
        if not ok:
            with self._csDB:
                self._deqResult.remove(cb)
        CScopeUQueue.Unlock(q)
        return ok

    def EndTrans(self, plan = tagRollbackPlan.rpDefault, handler = None):
        """
        End a manual transaction with a given rollback plan. Note the transaction will be associated with SocketPro client message queue if available to avoid possible transaction lose
        :param plan: a value for computing how included transactions should be rollback at server side. It defaults to tagRollbackPlan.rpDefault
        :param handler: a callback for tracking its response result
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        q = CScopeUQueue.Lock().SaveInt(plan)
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idEndTrans, handler)
        #don't make self._csDB locked across calling SendRequest, which may lead to client dead-lock in case a client asynchronously sends lots of requests without use of client side queue.
        with self._csDB:
            self._deqResult.append(cb)
        ok = self.SendRequest(CAsyncDBHandler.idEndTrans, q, None)
        if ok:
            self.AttachedClientSocket.ClientQueue.EndJob()
        else:
            with self._csDB:
                self._deqResult.remove(cb)
        CScopeUQueue.Unlock(q)
        return ok


    def Open(self, strConnection, handler = None, flags = 0):
        """
        Open a database connection at server side asynchronously
        :param strConnection: a database connection string. The database connection string can be an empty string if its server side supports global database connection string
        :param hander: a callback for database connecting result
        :param flags: a set of flags transferred to server to indicate how to build database connection at server side. It defaults to zero
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        s = ''
        q = CScopeUQueue.Lock().SaveString(strConnection).SaveUInt(flags)
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idOpen, handler)
        #don't make self._csDB locked across calling SendRequest, which may lead to client dead-lock in case a client asynchronously sends lots of requests without use of client side queue.
        with self._csDB:
            self._flags = flags
            self._deqResult.append(cb)
            if not strConnection is None:
                s = self._strConnection
                self._strConnection = strConnection
        ok = self.SendRequest(CAsyncDBHandler.idOpen, q, None)
        if not ok:
            with self._csDB:
                if not strConnection is None:
                    self._strConnection = s
                self._deqResult.remove(cb)
        CScopeUQueue.Unlock(q)
        return ok

    def Prepare(self, sql, handler = None, lstParameterInfo = []):
        """
        Send a parameterized SQL statement for preparing with a given array of parameter informations asynchronously
        :param sql: a parameterized SQL statement
        :param handler: a callback for SQL preparing result
        :param lstParameterInfo: a given array of parameter informations
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        q = CScopeUQueue.Lock().SaveString(sql)
        count = 0
        if not lstParameterInfo is None:
            count = len(lstParameterInfo)
        q.SaveUInt(count)
        if count > 0:
            for one in lstParameterInfo:
                one.SaveTo(q)
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idPrepare, handler)
        with self._csDB:
            self._deqResult.append(cb)
        ok = self.SendRequest(CAsyncDBHandler.idPrepare, q, None)
        if not ok:
            with self._csDB:
                self._deqResult.remove(cb)
        CScopeUQueue.Unlock(q)
        return ok

    def ExecuteSql(self, sql, handler = None, row = None, rh = None, meta = True, lastInsertId = True):
        """
        Process a complex SQL statement which may be combined with multiple basic SQL statements asynchronously
        :param sql: a complex SQL statement which may be combined with multiple basic SQL statements
        :param handler: a callback for tracking final result
        :param row: a callback for tracking record or output parameter returned data
        :param rh: a callback for tracking row set of header column informations. Note that there will be NO row set data or its column informations returned if NO such a callback is set
        :param meta: a boolean value for better or more detailed column meta details such as unique, not null, primary key, and so on. It defaults to true
        :param lastInsertId: a boolean value for last insert record identification number. It defaults to true
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        index = 0
        rowset = (not rh is None)
        if not rowset:
            meta = False
        q = CScopeUQueue.Lock().SaveString(sql).SaveBool(rowset).SaveBool(meta).SaveBool(lastInsertId)
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idExecute, handler)
        #don't make self._csDB locked across calling SendRequest, which may lead to client dead-lock in case a client asynchronously sends lots of requests without use of client side queue.
        with self._csDB:
            self._nCall += 1
            q.SaveULong(self._nCall)
            if rowset:
                self._mapRowset[self._nCall] = CAsyncDBHandler.Pair(rh,row)
            self._deqResult.append(cb)
            index = self._nCall
        ok = self.SendRequest(CAsyncDBHandler.idExecute, q, None)
        if not ok:
            with self._csDB:
                self._deqResult.remove(cb)
                if rowset:
                    self._mapRowset.pop(index)
        CScopeUQueue.Unlock(q)
        return ok

    def _SendParametersData(self, vParam):
        if vParam is None:
            return True
        size = len(vParam)
        self._firstRow = True
        def Send(sb):
            if sb.GetSize() > 0:
                if self._firstRow:
                    self._firstRow = False
                    if not self.SendRequest(CAsyncDBHandler.idBeginRows, sb, None):
                        return False
                else:
                    if not self.SendRequest(CAsyncDBHandler.idTransferring, sb, None):
                        return False
                sb.SetSize(0)
            elif self._firstRow:
                self._firstRow = False
                return self.SendRequest(CAsyncDBHandler.idBeginRows, None, None)
            return True
        def SendBlob(sb):
            start = True
            while sb.GetSize() > CAsyncDBHandler.DEFAULT_BIG_FIELD_CHUNK_SIZE:
                bytes = sb.PopBytes(CAsyncDBHandler.DEFAULT_BIG_FIELD_CHUNK_SIZE)
                if start:
                    if not self.SendRequest(CAsyncDBHandler.idStartBLOB, CUQueue(bytes), None):
                        return False
                    start = False
                else:
                    if not self.SendRequest(CAsyncDBHandler.idChunk, CUQueue(bytes), None):
                        return False
            if not self.SendRequest(CAsyncDBHandler.idEndBLOB, sb, None):
                return False
            sb.SetSize(0)
            return True
        sb = CScopeUQueue.Lock()
        for vt in vParam:
            if isinstance(vt, str):
                #send string as unicode string
                length = len(vt) * 2
                if length < 2 * CAsyncDBHandler.DEFAULT_BIG_FIELD_CHUNK_SIZE:
                    sb.SaveObject(vt)
                else:
                    if not Send(sb):
                        CScopeUQueue.Unlock(sb)
                        return False
                    length += 6
                    sb.SaveUInt(length).SaveObject(vt)
                    if not SendBlob(sb):
                        CScopeUQueue.Unlock(sb)
                        return False
            elif isinstance(vt, bytearray):
                #send array of bytes
                length = len(vt)
                if length < 2 * CAsyncDBHandler.DEFAULT_BIG_FIELD_CHUNK_SIZE:
                    sb.SaveObject(vt)
                else:
                    if not Send(sb):
                        CScopeUQueue.Unlock(sb)
                        return False
                    length += 6
                    sb.SaveUInt(length).SaveObject(vt)
                    if not SendBlob(sb):
                        CScopeUQueue.Unlock(sb)
                        return False
            else:
                sb.SaveObject(vt)
            if sb.GetSize() >= CAsyncDBHandler.DEFAULT_RECORD_BATCH_SIZE and not Send(sb):
                CScopeUQueue.Unlock(sb)
                return False
        ok = Send(sb)
        CScopeUQueue.Unlock(sb)
        return ok

    def ExecuteParameters(self, vParam, handler = None, row = None, rh = None, meta = True, lastInsertId = True):
        """
        Process a complex SQL statement which may be combined with multiple basic SQL statements asynchronously
        :param vParam: an array of parameter data which will be bounded to previously prepared parameters
        :param handler: a callback for tracking final result
        :param row: a callback for tracking record or output parameter returned data
        :param rh: a callback for tracking row set of header column informations. Note that there will be NO row set data or its column informations returned if NO such a callback is set
        :param meta: a boolean value for better or more detailed column meta details such as unique, not null, primary key, and so on. It defaults to true
        :param lastInsertId: a boolean value for last insert record identification number. It defaults to true
        :return: true if request is successfully sent or queued; and false if request is NOT successfully sent or queued
        """
        index = 0
        rowset = (not rh is None)
        cb = CAsyncDBHandler.Pair(CAsyncDBHandler.idExecuteParameters, handler)
        if not rowset:
            meta = False
        q = CScopeUQueue.Lock().SaveBool(rowset).SaveBool(meta).SaveBool(lastInsertId)
        #don't make self._csDB locked across calling SendRequest, which may lead to client dead-lock in case a client asynchronously sends lots of requests without use of client side queue.
        with self._csDB:
            if not self._SendParametersData(vParam):
                self._Clean()
                CScopeUQueue.Unlock(q)
                return False
            self._nCall += 1
            q.SaveULong(self._nCall)
            index = self._nCall
            self._deqResult.append(cb)
            self._mapParameterCall[self._nCall] = vParam
            if rowset:
                self._mapRowset[self._nCall] = CAsyncDBHandler.Pair(rh, row)
        ok = self.SendRequest(CAsyncDBHandler.idExecuteParameters, q, None)
        if not ok:
            with self._csDB:
                if rowset:
                    self._mapRowset.pop(index)
                self._mapParameterCall.pop(index)
                self._deqResult.remove(cb)
        CScopeUQueue.Unlock(q)
        return ok