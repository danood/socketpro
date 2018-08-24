#include "stdafx.h"
#include "njobjects.h"
#include "njhandler.h"


namespace NJA {
	using v8::Context;

	Persistent<Function> NJSocketPool::constructor;

	NJSocketPool::NJSocketPool(const wchar_t* defaultDb, unsigned int id, bool autoConn, unsigned int recvTimeout, unsigned int connTimeout)
	: SvsId(id), m_errSSL(0), m_defaultDb(defaultDb ? defaultDb : L"") {
		switch (id) {
		case SPA::Sqlite::sidSqlite:
			if (m_defaultDb.size())
				Sqlite = new CSQLMasterPool<false, CSqlite>(m_defaultDb.c_str(), recvTimeout);
			else
				Sqlite = new CSocketPool<CSqlite>(autoConn, recvTimeout, connTimeout);
			break;
		case SPA::Mysql::sidMysql:
			if (m_defaultDb.size())
				Mysql = new CSQLMasterPool<false, CMysql>(m_defaultDb.c_str(), recvTimeout);
			else
				Mysql = new CSocketPool<CMysql>(autoConn, recvTimeout, connTimeout);
			break;
		case SPA::Odbc::sidOdbc:
			if (m_defaultDb.size())
				Odbc = new CSQLMasterPool<false, COdbc>(m_defaultDb.c_str(), recvTimeout);
			else
				Odbc = new CSocketPool<COdbc>(autoConn, recvTimeout, connTimeout);
			break;
		case SPA::Queue::sidQueue:
			Queue = new CSocketPool<CAsyncQueue>(autoConn, recvTimeout, connTimeout);
			break;
		case SPA::SFile::sidFile:
			File = new CSocketPool<CStreamingFile>(autoConn, recvTimeout, connTimeout);
			break;
		default:
			if (m_defaultDb.size())
				Handler = new CMasterPool<false, CAsyncHandler>(m_defaultDb.c_str(), recvTimeout, id);
			else
				Handler = new CSocketPool<CAsyncHandler>(autoConn, recvTimeout, connTimeout);
			break;
		}
		::memset(&m_asyncType, 0, sizeof(m_asyncType));
		m_asyncType.data = this;
		::memset(&m_csType, 0, sizeof(m_csType));
		m_csType.data = this;
	}

	NJSocketPool::~NJSocketPool() {
		Release();
	}

	bool NJSocketPool::IsValid(Isolate* isolate) {
		if (!Handler) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "SocketPool object already disposed")));
			return false;
		}
		return true;
	}

	void NJSocketPool::Release() {
		if (Handler) {
			switch (SvsId) {
			case SPA::Sqlite::sidSqlite:
				delete Sqlite;
				break;
			case SPA::Mysql::sidMysql:
				delete Mysql;
				break;
			case SPA::Odbc::sidOdbc:
				delete Odbc;
				break;
			case SPA::Queue::sidQueue:
				delete Queue;
				break;
			case SPA::SFile::sidFile:
				delete File;
				break;
			default:
				delete Handler;
				break;
			}
			Handler = nullptr;
		}
	}

	void NJSocketPool::Init(Local<Object> exports) {
		Isolate* isolate = exports->GetIsolate();

		// Prepare constructor template
		Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
		tpl->SetClassName(String::NewFromUtf8(isolate, "CSocketPool"));
		tpl->InstanceTemplate()->SetInternalFieldCount(15);

		//Prototype
		NODE_SET_PROTOTYPE_METHOD(tpl, "Dispose", Dispose);
		NODE_SET_PROTOTYPE_METHOD(tpl, "DisconnectAll", DisconnectAll);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getAsyncHandlers", getAsyncHandlers);
		//NODE_SET_PROTOTYPE_METHOD(tpl, "getAvg", getAvg);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getConnectedSockets", getConnectedSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getDisconnectedSockets", getDisconnectedSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getIdleSockets", getIdleSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getLockedSockets", getLockedSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getPoolId", getPoolId);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getSvsId", getSvsId);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getErrCode", getErrCode);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getAutoMerge", getQueueAutoMerge);
		NODE_SET_PROTOTYPE_METHOD(tpl, "setAutoMerge", setQueueAutoMerge);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getQueueName", getQueueName);
		NODE_SET_PROTOTYPE_METHOD(tpl, "setQueueName", setQueueName);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getErrMsg", getErrMsg);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getQueues", getQueues);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getSockets", getSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getTotalSockets", getSocketsPerThread);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getStarted", getStarted);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getThreads", getThreadsCreated);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Lock", Lock);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Seek", Seek);
		NODE_SET_PROTOTYPE_METHOD(tpl, "SeekByQueue", SeekByQueue);
		NODE_SET_PROTOTYPE_METHOD(tpl, "ShutdownPool", ShutdownPool);
		NODE_SET_PROTOTYPE_METHOD(tpl, "StartPool", StartSocketPool);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Unlock", Unlock);
		NODE_SET_PROTOTYPE_METHOD(tpl, "setPoolEvent", setPoolEvent);

		constructor.Reset(isolate, tpl->GetFunction());
		exports->Set(String::NewFromUtf8(isolate, "CSocketPool"), tpl->GetFunction());
	}

	void NJSocketPool::New(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		if (args.IsConstructCall()) {
			unsigned int svsId = 0;
			if (args[0]->IsUint32()) {
				svsId = args[0]->Uint32Value();
			}
			if (svsId < SPA::sidChat || (svsId > SPA::sidODBC && svsId <= SPA::sidReserved)) {
				isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "A valid unsigned int required for service id")));
				return;
			}
			if (svsId == sidHTTP) {
				isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "No support to HTTP/websocket at client side")));
				return;
			}
			std::wstring db;
			if (args[1]->IsString()) {
				String::Value str(args[1]);
				unsigned int len = (unsigned int)str.length();
				if (len)
					db.assign(*str, *str + len);
			}
			if (db.size()) {
				switch (svsId) {
				case sidFile:
					isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "File streaming doesn't support master-slave pool")));
					return;
				case sidChat:
					isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Persistent queue doesn't support master-slave pool")));
					return;
				default:
					break;
				}
			}
			// Invoked as constructor: `new NJSocketPool(...)`
			NJSocketPool* obj = new NJSocketPool(db.c_str(), svsId);
			obj->Wrap(args.This());
			obj->Handler->DoSslServerAuthentication = [obj](CSocketPool<CAsyncHandler> *pool, SPA::ClientSide::CClientSocket *cs)->bool {
				IUcert *cert = cs->GetUCert();
				obj->m_errMsg = cert->Verify(&obj->m_errSSL);
				return (obj->m_errSSL == 0); //true -- user id and password will be sent to server
			};
			obj->Handler->SocketPoolEvent = [obj](CSocketPool<CAsyncHandler> *pool, tagSocketPoolEvent spe, CAsyncHandler *handler) {
				switch (spe) {
				case SPA::ClientSide::speUSocketCreated:
					handler->GetAttachedClientSocket()->m_asyncType = &obj->m_csType;
					break;
				default:
					break;
				}
				SPA::CAutoLock al(obj->m_cs);
				if (obj->m_evPool.IsEmpty())
					return;
				PoolEvent pe;
				pe.Spe = spe;
				pe.Handler = handler;
				obj->m_deqPoolEvent.push_back(pe);
				int fail = uv_async_send(&obj->m_asyncType);
				assert(!fail);
			};
			int fail = uv_async_init(uv_default_loop(), &obj->m_asyncType, async_cb);
			assert(!fail);
			fail = uv_async_init(uv_default_loop(), &obj->m_csType, async_cs_cb);
			assert(!fail);
			args.GetReturnValue().Set(args.This());
		}
		else {
			// Invoked as plain function `NJSocketPool(...)`, turn into construct call.
			Local<Value> argv[] = {args[0],args[1]};
			Local<Context> context = isolate->GetCurrentContext();
			Local<Function> cons = Local<Function>::New(isolate, constructor);
			Local<Object> result = cons->NewInstance(context, 2, argv).ToLocalChecked();
			args.GetReturnValue().Set(result);
		}
	}

	void NJSocketPool::async_cb(uv_async_t* handle) {
		Isolate* isolate = Isolate::GetCurrent();
		v8::HandleScope handleScope(isolate); //required for Node 4.x
		NJSocketPool* obj = (NJSocketPool*)handle->data;
		SPA::CAutoLock al(obj->m_cs);
		while (obj->m_deqPoolEvent.size()) {
			const PoolEvent &pe = obj->m_deqPoolEvent.front();
			if (!obj->m_evPool.IsEmpty()) {
				Local<Value> argv[] = {Int32::New(isolate, pe.Spe)};
				Local<Function> cb = Local<Function>::New(isolate, obj->m_evPool);
				cb->Call(Null(isolate), 1, argv);
			}
			obj->m_deqPoolEvent.pop_front();
		}
		if (!obj->Handler) {
			uv_close((uv_handle_t*)handle, nullptr);
		}
	}

	void NJSocketPool::async_cs_cb(uv_async_t* handle) {
		Isolate* isolate = Isolate::GetCurrent();
		v8::HandleScope handleScope(isolate); //required for Node 4.x
		NJSocketPool* obj = (NJSocketPool*)handle->data;
		SPA::CAutoLock al(obj->m_cs);
		while (obj->m_deqSocketEvent.size()) {
			const SocketEvent &se = obj->m_deqSocketEvent.front();
			switch (se.Se)
			{
			case seAllProcessed:
				break;
			case seBaseRequestProcessed:
				break;
			case seResultReturned:
				break;
			case seServerException:
				break;
			default:
				break;
			}
			SPA::CScopeUQueue::Unlock(se.QData);
			obj->m_deqSocketEvent.pop_front();
		}
	}

	void NJSocketPool::Dispose(const FunctionCallbackInfo<Value>& args) {
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		obj->Release();
	}

	void NJSocketPool::DisconnectAll(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			bool ok = obj->Handler->DisconnectAll();
			args.GetReturnValue().Set(Boolean::New(isolate, ok));
		}
	}

	void NJSocketPool::getAsyncHandlers(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {

		}
	}

	void NJSocketPool::getAvg(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			bool ok = obj->Handler->IsAvg();
			args.GetReturnValue().Set(Boolean::New(isolate, ok));
		}
	}

	void NJSocketPool::getConnectedSockets(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetConnectedSockets();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getDisconnectedSockets(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetDisconnectedSockets();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getIdleSockets(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetIdleSockets();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getLockedSockets(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetLockedSockets();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getSvsId(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		unsigned int data = obj->SvsId;
		args.GetReturnValue().Set(Uint32::New(isolate, data));
	}

	void NJSocketPool::getPoolId(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetPoolId();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getErrCode(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		int data = obj->m_errSSL;
		args.GetReturnValue().Set(Int32::New(isolate, data));
	}

	void NJSocketPool::getErrMsg(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		args.GetReturnValue().Set(String::NewFromUtf8(isolate, obj->m_errMsg.c_str()));
	}

	void NJSocketPool::getQueueAutoMerge(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			bool ok = obj->Handler->GetQueueAutoMerge();
			args.GetReturnValue().Set(Boolean::New(isolate, ok));
		}
	}

	void NJSocketPool::setQueueAutoMerge(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsBoolean()) {
				bool b = p->BooleanValue();
				obj->Handler->SetQueueAutoMerge(b);
			}
			else {
				isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "A boolean value expected")));
			}
		}
	}

	void NJSocketPool::getQueueName(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			std::string queueName = obj->Handler->GetQueueName();
			args.GetReturnValue().Set(String::NewFromUtf8(isolate, queueName.c_str()));
		}
	}

	void NJSocketPool::setQueueName(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsString()) {
				String::Utf8Value str(p);
				obj->Handler->SetQueueName(*str);
			}
			else {
				isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "A boolean value expected")));
			}
		}
	}

	void NJSocketPool::getQueues(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetQueues();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getSockets(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			
		}
	}

	void NJSocketPool::getSocketsPerThread(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetSocketsPerThread();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::getStarted(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			bool ok = obj->Handler->IsStarted();
			args.GetReturnValue().Set(Boolean::New(isolate, ok));
		}
	}

	void NJSocketPool::getThreadsCreated(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			unsigned int data = obj->Handler->GetThreadsCreated();
			args.GetReturnValue().Set(Uint32::New(isolate, data));
		}
	}

	void NJSocketPool::Lock(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {

		}
	}
	void NJSocketPool::Seek(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {

		}
	}
	void NJSocketPool::SeekByQueue(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {

		}
	}
	void NJSocketPool::ShutdownPool(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			obj->Handler->ShutdownPool();
		}
	}

	bool NJSocketPool::To(Isolate* isolate, const Local<Object> &obj, SPA::ClientSide::CConnectionContext &cc) 		{
		Local<Array> props = obj->GetPropertyNames();
		if (props->Length() != 8) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid connection context")));
			return false;
		}

		auto v = obj->Get(props->Get(0));
		if (!v->IsString()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid host string")));
			return false;
		}
		String::Value host(v);
		cc.Host.assign(*host, *host + host.length());

		v = obj->Get(props->Get(1));
		if (!v->IsUint32()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid port number")));
			return false;
		}
		cc.Port = v->Uint32Value();

		v = obj->Get(props->Get(2));
		if (!v->IsString()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid user id string")));
			return false;
		}
		String::Value uid(v);
		cc.UserId.assign(*uid, *uid + uid.length());

		v = obj->Get(props->Get(3));
		if (!v->IsString()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid password string")));
			return false;
		}
		String::Value pwd(v);
		cc.Password.assign(*pwd, *pwd + pwd.length());

		v = obj->Get(props->Get(4));
		if (!v->IsUint32()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Encryption method expected")));
			return false;
		}
		unsigned int em = v->Uint32Value();
		if (em > 1) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid encryption method")));
			return false;
		}
		cc.EncrytionMethod = (SPA::tagEncryptionMethod)em;

		v = obj->Get(props->Get(5));
		if (!v->IsBoolean()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Boolean value expected for Zip")));
			return false;
		}
		cc.Zip = v->BooleanValue();

		v = obj->Get(props->Get(6));
		if (!v->IsBoolean()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Boolean value expected for V6")));
			return false;
		}
		cc.V6 = v->BooleanValue();
		
		//v = obj->Get(props->Get(7)); //ignored at now

		return true;
	}

	void NJSocketPool::StartSocketPool(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (!obj->IsValid(isolate))
			return;
		auto p1 = args[1];
		if (!p1->IsUint32()) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "An unsigned int number expected for client sockets")));
			return;
		}
		unsigned int sessions = p1->Uint32Value();
		if (!sessions) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "The number of client sockets cannot be zero")));
			return;
		}
		std::vector<SPA::ClientSide::CConnectionContext> vCC;
		auto p0 = args[0];
		if (p0->IsArray()) {
			Local<Array> jsArr = Local<Array>::Cast(p0);
			unsigned int count = jsArr->Length();
			for (unsigned int n = 0; n < count; ++n) {
				auto v = jsArr->Get(n);
				if (!v->IsObject()) {
					isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid connection context found")));
					return;
				}
				Local<Object> obj = jsArr->Get(n)->ToObject();
				SPA::ClientSide::CConnectionContext cc;
				if (!To(isolate, obj, cc)) {
					return;
				}
				vCC.push_back(cc);
			}
		}
		else if (p0->IsObject()) {
			Local<Object> obj = p0->ToObject();
			SPA::ClientSide::CConnectionContext cc;
			if (!To(isolate, obj, cc)) {
				return;
			}
			vCC.push_back(cc);
		}
		else {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "One or an array of connection contexts expected")));
			return;
		}
		unsigned int remain = sessions % vCC.size();
		sessions = (unsigned int)((sessions / vCC.size()) * vCC.size() + (remain ? vCC.size() : 0));
		unsigned int repeats = (unsigned int)(sessions / vCC.size() - 1);
		std::vector<SPA::ClientSide::CConnectionContext> vCCs = vCC;
		for (unsigned int n = 0; n < repeats; ++n) {
			for (auto it = vCC.begin(), end = vCC.end(); it != end; ++it) {
				vCCs.push_back(*it);
			}
		}
		typedef CConnectionContext* PCConnectionContext;
		PCConnectionContext ppCCs[] = { vCCs.data() };
		bool ok = obj->Handler->StartSocketPool(ppCCs, 1, sessions, true, SPA::tagThreadApartment::taNone);
		if (!ok) {
			auto cs = obj->Handler->GetSockets()[0];
			obj->m_errSSL = cs->GetErrorCode();
			obj->m_errMsg = cs->GetErrorMsg();
		}
		else {
			obj->m_errSSL = 0;
			obj->m_errMsg.clear();
		}
		args.GetReturnValue().Set(Boolean::New(isolate, ok));
	}

	void NJSocketPool::Unlock(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {

		}
	}

	void NJSocketPool::setPoolEvent(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsFunction()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_evPool.Reset(isolate, Local<Function>::Cast(p));
			}
			else if (p->IsNullOrUndefined()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_evPool.Empty();
			}
			else {
				isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "A callback expected for tracking pool event")));
			}
		}
	}
}
