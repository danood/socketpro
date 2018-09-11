#include "stdafx.h"
#include "njobjects.h"
#include "../../../include/sqlite/usqlite.h"
#include "../../../include/mysql/umysql.h"
#include "../../../include/odbc/uodbc.h"
#include "../../../include/rdbcache.h"
#include "../../../include/masterpool.h"

namespace NJA {
	using v8::Context;

	Persistent<Function> NJSocketPool::constructor;

	NJSocketPool::NJSocketPool(const wchar_t* defaultDb, unsigned int id, bool autoConn, unsigned int recvTimeout, unsigned int connTimeout)
		: SvsId(id), m_errSSL(0), m_defaultDb(defaultDb ? defaultDb : L"") {
		switch (id) {
		case SPA::Mysql::sidMysql:
		case SPA::Odbc::sidOdbc:
		case SPA::Sqlite::sidSqlite:
			if (m_defaultDb.size())
				Db = new CSQLMasterPool<false, CNjDb>(m_defaultDb.c_str(), recvTimeout, id);
			else
				Db = new CSocketPool<CNjDb>(autoConn, recvTimeout, connTimeout, id);
			break;
		case SPA::Queue::sidQueue:
			Queue = new CSocketPool<CAQueue>(autoConn, recvTimeout, connTimeout);
			break;
		case SPA::SFile::sidFile:
			File = new CSocketPool<CSFile>(autoConn, recvTimeout, connTimeout);
			break;
		default:
			if (m_defaultDb.size())
				Handler = new CMasterPool<false, CAsyncHandler>(m_defaultDb.c_str(), recvTimeout, id);
			else
				Handler = new CSocketPool<CAsyncHandler>(autoConn, recvTimeout, connTimeout, id);
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
			ThrowException(isolate, "SocketPool object already disposed");
			return false;
		}
		return true;
	}

	void NJSocketPool::Release() {
		SPA::CAutoLock al(m_cs);
		if (Handler) {
			switch (SvsId) {
			case SPA::Sqlite::sidSqlite:
			case SPA::Mysql::sidMysql:
			case SPA::Odbc::sidOdbc:
				delete Db;
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
			uv_close((uv_handle_t*)&m_asyncType, nullptr);
			uv_close((uv_handle_t*)&m_csType, nullptr);
		}
	}

	void NJSocketPool::Init(Local<Object> exports) {
		Isolate* isolate = exports->GetIsolate();

		// Prepare constructor template
		Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
		tpl->SetClassName(ToStr(isolate, "CSocketPool"));
		tpl->InstanceTemplate()->SetInternalFieldCount(15);

		//methods
		NODE_SET_PROTOTYPE_METHOD(tpl, "Dispose", Dispose);
		NODE_SET_PROTOTYPE_METHOD(tpl, "CloseAll", DisconnectAll);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Seek", Seek);
		NODE_SET_PROTOTYPE_METHOD(tpl, "SeekByQueue", SeekByQueue);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Shutdown", ShutdownPool);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Start", StartSocketPool);
		NODE_SET_PROTOTYPE_METHOD(tpl, "Unlock", Unlock);
		
		//properties
		NODE_SET_PROTOTYPE_METHOD(tpl, "getAsyncHandlers", getAsyncHandlers);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getConnectedSockets", getConnectedSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getClosedSockets", getDisconnectedSockets);
		NODE_SET_PROTOTYPE_METHOD(tpl, "getIdleSockets", getIdleSockets);
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
		NODE_SET_PROTOTYPE_METHOD(tpl, "setPoolEvent", setPoolEvent);
		NODE_SET_PROTOTYPE_METHOD(tpl, "setReturned", setResultReturned);
		NODE_SET_PROTOTYPE_METHOD(tpl, "setAllProcessed", setAllProcessed);
		NODE_SET_PROTOTYPE_METHOD(tpl, "setPush", setPush);

		//NODE_SET_PROTOTYPE_METHOD(tpl, "getAvg", getAvg);
		//NODE_SET_PROTOTYPE_METHOD(tpl, "getLockedSockets", getLockedSockets);
		//NODE_SET_PROTOTYPE_METHOD(tpl, "Lock", Lock);

		constructor.Reset(isolate, tpl->GetFunction());
		exports->Set(ToStr(isolate, "CSocketPool"), tpl->GetFunction());
	}

	void NJSocketPool::New(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		if (args.IsConstructCall()) {
			unsigned int svsId = 0;
			if (args[0]->IsUint32()) {
				svsId = args[0]->Uint32Value();
			}
			if (svsId < SPA::sidChat || (svsId > SPA::sidODBC && svsId <= SPA::sidReserved)) {
				ThrowException(isolate, "A valid unsigned int required for service id");
				return;
			}
			if (svsId == sidHTTP) {
				ThrowException(isolate, "No support to HTTP/websocket at client side");
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
					ThrowException(isolate, "File streaming doesn't support master-slave pool");
					return;
				case sidChat:
					ThrowException(isolate, "Persistent queue doesn't support master-slave pool");
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
			Local<Value> argv[] = { args[0],args[1] };
			Local<Context> context = isolate->GetCurrentContext();
			Local<Function> cons = Local<Function>::New(isolate, constructor);
			Local<Object> result = cons->NewInstance(context, 2, argv).ToLocalChecked();
			args.GetReturnValue().Set(result);
		}
	}

	void NJSocketPool::async_cb(uv_async_t* handle) {
		Isolate* isolate = Isolate::GetCurrent();
		HandleScope handleScope(isolate); //required for Node 4.x
		NJSocketPool* obj = (NJSocketPool*)handle->data;
		assert(obj);
		if (!obj)
			return;
		{
			SPA::CAutoLock al(obj->m_cs);
			while (obj->m_deqPoolEvent.size()) {
				const PoolEvent &pe = obj->m_deqPoolEvent.front();
				if (!obj->m_evPool.IsEmpty()) {
					Local<Value> argv[] = { Int32::New(isolate, pe.Spe) };
					Local<Function> cb = Local<Function>::New(isolate, obj->m_evPool);
					cb->Call(isolate->GetCurrentContext(), Null(isolate), 1, argv);
				}
				obj->m_deqPoolEvent.pop_front();
			}
		}
		isolate->RunMicrotasks();
	}

	void NJSocketPool::async_cs_cb(uv_async_t* handle) {
		unsigned short reqId;
		NJSocketPool* obj = (NJSocketPool*)handle->data;
		assert(obj);
		if (!obj)
			return;
		Isolate* isolate = Isolate::GetCurrent();
		HandleScope handleScope(isolate); //required for Node 4.x or later
		{
			SPA::CAutoLock al(obj->m_cs);
			while (obj->m_deqSocketEvent.size()) {
				SocketEvent se = obj->m_deqSocketEvent.front();
				SPA::ClientSide::PAsyncServiceHandler ash;
				*se.QData >> ash >> reqId;
				assert(ash);
				assert(reqId);
				Local<Object> njAsh;
				unsigned int sid = ash->GetSvsID();
				switch (sid) {
				case SPA::Odbc::sidOdbc:
				case SPA::Mysql::sidMysql:
				case SPA::Sqlite::sidSqlite:
					njAsh = NJSqlite::New(isolate, (CNjDb*)ash, true);
					break;
				case SPA::Queue::sidQueue:
					njAsh = NJAsyncQueue::New(isolate, (CAQueue*)ash, true);
					break;
				case SPA::SFile::sidFile:
					njAsh = NJFile::New(isolate, (CSFile*)ash, true);
					break;
				default:
					njAsh = NJHandler::New(isolate, (CAsyncHandler*)ash, true);
					break;
				}
				Local<Value> jsReqId = Uint32::New(isolate, reqId);
				if (ash) {
					sid = ash->GetSvsID();
					switch (se.Se) {
					case seChatEnter:
						if (!obj->m_push.IsEmpty()) {
							Local<String> jsName = ToStr(isolate, "Subscribe");
							auto sender = ToMessageSender(isolate, *se.QData);
							CComVariant vt;
							*se.QData >> vt;
							assert(!se.QData->GetSize());
							auto groups = From(isolate, vt);
							Local<Value> argv[] = { jsName, groups, sender, njAsh };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_push);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 4, argv);
						}
						break;
					case seChatExit:
						if (!obj->m_push.IsEmpty()) {
							Local<String> jsName = ToStr(isolate, "Unsubscribe");
							auto sender = ToMessageSender(isolate, *se.QData);
							CComVariant vt;
							*se.QData >> vt;
							assert(!se.QData->GetSize());
							auto groups = From(isolate, vt);
							Local<Value> argv[] = { jsName, groups, sender, njAsh };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_push);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 4, argv);
						}
						break;
					case sePublish:
						if (!obj->m_push.IsEmpty()) {
							Local<String> jsName = ToStr(isolate, "Publish");
							auto sender = ToMessageSender(isolate, *se.QData);
							CComVariant vt;
							*se.QData >> vt;
							auto groups = From(isolate, vt);
							vt.Clear();
							*se.QData >> vt;
							assert(!se.QData->GetSize());
							auto msg = From(isolate, vt);
							Local<Value> argv[] = { jsName, groups, msg, sender, njAsh };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_push);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 5, argv);
						}
						break;
					case sePublishEx:
						if (!obj->m_push.IsEmpty()) {
							Local<String> jsName = ToStr(isolate, "PublishEx");
							auto sender = ToMessageSender(isolate, *se.QData);
							CComVariant vt;
							*se.QData >> vt;
							auto groups = From(isolate, vt);
							unsigned int len = se.QData->GetSize();
							auto bytes = node::Buffer::New(isolate, (char*)se.QData->GetBuffer(), len).ToLocalChecked();
							se.QData->SetSize(0);
							Local<Value> argv[] = { jsName, groups, bytes, sender, njAsh };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_push);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 5, argv);
						}
						break;
					case sePostUserMessage:
						if (!obj->m_push.IsEmpty()) {
							Local<String> jsName = ToStr(isolate, "SendMessage");
							auto sender = ToMessageSender(isolate, *se.QData);
							CComVariant vt;
							*se.QData >> vt;
							auto msg = From(isolate, vt);
							assert(!se.QData->GetSize());
							Local<Value> argv[] = { jsName, msg, sender, njAsh };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_push);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 4, argv);
						}
						break;
					case sePostUserMessageEx:
						if (!obj->m_push.IsEmpty()) {
							Local<String> jsName = ToStr(isolate, "SendMessageEx");
							auto sender = ToMessageSender(isolate, *se.QData);
							unsigned int len = se.QData->GetSize();
							auto bytes = node::Buffer::New(isolate, (char*)se.QData->GetBuffer(), len).ToLocalChecked();
							se.QData->SetSize(0);
							Local<Value> argv[] = { jsName, bytes, sender, njAsh };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_push);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 4, argv);
						}
						break;
					case seAllProcessed:
						if (!obj->m_ap.IsEmpty()) {
							Local<Value> argv[] = { njAsh, jsReqId };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_ap);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 2, argv);
						}
						break;
					case seBaseRequestProcessed:
						if (!obj->m_brp.IsEmpty()) {
							Local<Value> argv[] = { njAsh, jsReqId };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_brp);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 2, argv);
						}
						break;
					case seResultReturned:
						if (!obj->m_rr.IsEmpty()) {
							Local<Function> cb = Local<Function>::New(isolate, obj->m_rr);
							if (se.QData->GetSize()) {
								Local<Value> argv[3];
								argv[0] = njAsh;
								argv[1] = jsReqId;
								auto q = NJQueue::New(isolate, se.QData);
								argv[2] = q;
								cb->Call(isolate->GetCurrentContext(), Null(isolate), 3, argv);
								auto obj = ObjectWrap::Unwrap<NJQueue>(q);
								obj->Release();
							}
							else {
								Local<Value> argv[] = { njAsh , jsReqId };
								cb->Call(isolate->GetCurrentContext(), Null(isolate), 2, argv);
							}
						}
						break;
					case seServerException:
						if (!obj->m_se.IsEmpty()) {
							std::wstring errMsg;
							std::string errWhere;
							unsigned int errCode = 0;
							*se.QData >> errMsg >> errWhere >> errCode;
							Local<String> jsMsg = ToStr(isolate, errMsg.c_str());
							Local<String> jsWhere = ToStr(isolate, errWhere.c_str());
							Local<Value> jsCode = Number::New(isolate, errCode);
							Local<Value> argv[] = { njAsh, jsReqId, jsMsg, jsCode, jsWhere };
							Local<Function> cb = Local<Function>::New(isolate, obj->m_se);
							cb->Call(isolate->GetCurrentContext(), Null(isolate), 5, argv);
						}
						break;
					default:
						break;
					}
				}
				CScopeUQueue::Unlock(se.QData);
				obj->m_deqSocketEvent.pop_front();
			}
		}
		isolate->RunMicrotasks();
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
			unsigned int index = 0;
			Local<Array> v = Array::New(isolate);
			switch (obj->SvsId) {
			case SPA::Queue::sidQueue:
			{
				auto handlers = obj->Queue->GetAsyncHandlers();
				for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it, ++index) {
					v->Set(index, NJAsyncQueue::New(isolate, it->get(), true));
				}
			}
			break;
			case SPA::Mysql::sidMysql:
			case SPA::Odbc::sidOdbc:
			case SPA::Sqlite::sidSqlite:
			{
				auto handlers = obj->Db->GetAsyncHandlers();
				for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it, ++index) {
					v->Set(index, NJSqlite::New(isolate, it->get(), true));
				}
			}
			break;
			break;
			case SPA::SFile::sidFile:
			{
				auto handlers = obj->File->GetAsyncHandlers();
				for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it, ++index) {
					v->Set(index, NJFile::New(isolate, it->get(), true));
				}
			}
			break;
			default:
			{
				auto handlers = obj->Handler->GetAsyncHandlers();
				for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it, ++index) {
					v->Set(index, NJHandler::New(isolate, it->get(), true));
				}
			}
			break;
			}
			args.GetReturnValue().Set(v);
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
		args.GetReturnValue().Set(ToStr(isolate, obj->m_errMsg.c_str()));
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
				ThrowException(isolate, "A boolean value expected");
			}
		}
	}

	void NJSocketPool::getQueueName(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			std::string queueName = obj->Handler->GetQueueName();
			args.GetReturnValue().Set(ToStr(isolate, queueName.c_str()));
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
				ThrowException(isolate, "A boolean value expected");
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
			Local<Array> v = Array::New(isolate);
			auto sockets = obj->Handler->GetSockets();
			unsigned int index = 0;
			for (auto it = sockets.begin(), end = sockets.end(); it != end; ++it, ++index) {
				auto s = it->get();
				v->Set(index, NJSocket::New(isolate, s, true));
			}
			args.GetReturnValue().Set(v);
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
			unsigned int timeout = (~0);
			if (args[0]->IsNumber()) {
				timeout = args[0]->Uint32Value();
			}
			switch (obj->SvsId) {
			case SPA::Queue::sidQueue:
			{
				auto p = obj->Queue->Lock(timeout);
				args.GetReturnValue().Set(NJAsyncQueue::New(isolate, p.get(), true));
			}
			break;
			case SPA::Mysql::sidMysql:
			case SPA::Odbc::sidOdbc:
			case SPA::Sqlite::sidSqlite:
			{
				auto p = obj->Db->Lock(timeout);
				args.GetReturnValue().Set(NJSqlite::New(isolate, p.get(), true));
			}
			break;
			case SPA::SFile::sidFile:
			{
				auto p = obj->File->Lock(timeout);
				args.GetReturnValue().Set(NJFile::New(isolate, p.get(), true));
			}
			break;
			default:
			{
				auto p = obj->Handler->Lock(timeout);
				args.GetReturnValue().Set(NJHandler::New(isolate, p.get(), true));
			}
			break;
			}
		}
	}
	void NJSocketPool::Seek(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			switch (obj->SvsId) {
			case SPA::Queue::sidQueue:
			{
				auto p = obj->Queue->Seek();
				if (p)
					args.GetReturnValue().Set(NJAsyncQueue::New(isolate, p.get(), true));
			}
			break;
			case SPA::Mysql::sidMysql:
			case SPA::Odbc::sidOdbc:
			case SPA::Sqlite::sidSqlite:
			{
				auto p = obj->Db->Seek();
				if (p)
					args.GetReturnValue().Set(NJSqlite::New(isolate, p.get(), true));
			}
			break;
			case SPA::SFile::sidFile:
			{
				auto p = obj->File->Seek();
				if (p)
					args.GetReturnValue().Set(NJFile::New(isolate, p.get(), true));
			}
			break;
			default:
			{
				auto p = obj->Handler->Seek();
				if (p)
					args.GetReturnValue().Set(NJHandler::New(isolate, p.get(), true));
			}
			break;
			}
		}
	}
	void NJSocketPool::SeekByQueue(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsNullOrUndefined()) {
				switch (obj->SvsId) {
				case SPA::Queue::sidQueue:
				{
					auto p = obj->Queue->SeekByQueue();
					if (p)
						args.GetReturnValue().Set(NJAsyncQueue::New(isolate, p.get(), true));
				}
				break;
				case SPA::Odbc::sidOdbc:
				case SPA::Mysql::sidMysql:
				case SPA::Sqlite::sidSqlite:
				{
					auto p = obj->Db->SeekByQueue();
					if (p)
						args.GetReturnValue().Set(NJSqlite::New(isolate, p.get(), true));
				}
				break;
				case SPA::SFile::sidFile:
				{
					auto p = obj->File->SeekByQueue();
					if (p)
						args.GetReturnValue().Set(NJFile::New(isolate, p.get(), true));
				}
				break;
				default:
				{
					auto p = obj->Handler->SeekByQueue();
					if (p)
						args.GetReturnValue().Set(NJHandler::New(isolate, p.get(), true));
				}
				break;
				}
			}
			else if (p->IsString()) {
				std::string qname;
				String::Utf8Value str(p);
				qname = *str;
				switch (obj->SvsId) {
				case SPA::Queue::sidQueue:
				{
					auto p = obj->Queue->SeekByQueue(qname);
					if (p)
						args.GetReturnValue().Set(NJAsyncQueue::New(isolate, p.get(), true));
				}
				break;
				case SPA::Mysql::sidMysql:
				case SPA::Odbc::sidOdbc:
				case SPA::Sqlite::sidSqlite:
				{
					auto p = obj->Db->SeekByQueue(qname);
					if (p)
						args.GetReturnValue().Set(NJSqlite::New(isolate, p.get(), true));
				}
				break;
				case SPA::SFile::sidFile:
				{
					auto p = obj->File->SeekByQueue(qname);
					if (p)
						args.GetReturnValue().Set(NJFile::New(isolate, p.get(), true));
				}
				break;
				default:
				{
					auto p = obj->Handler->SeekByQueue(qname);
					if (p)
						args.GetReturnValue().Set(NJHandler::New(isolate, p.get(), true));
				}
				break;
				}
			}
			else {
				ThrowException(isolate, "A string, undefined or null expected");
				return;
			}
		}
	}
	void NJSocketPool::ShutdownPool(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			obj->Handler->ShutdownPool();
		}
	}

	bool NJSocketPool::To(Isolate* isolate, const Local<Object> &obj, SPA::ClientSide::CConnectionContext &cc) {
		Local<Array> props = obj->GetPropertyNames();
		if (props->Length() != 8) {
			ThrowException(isolate, "Invalid connection context");
			return false;
		}

		auto v = obj->Get(ToStr(isolate, "Host"));
		if (!v->IsString()) {
			ThrowException(isolate, "Invalid host string");
			return false;
		}
		String::Utf8Value host(v);
		cc.Host = *host;

		v = obj->Get(ToStr(isolate, "Port"));
		if (!v->IsUint32()) {
			ThrowException(isolate, "Invalid port number");
			return false;
		}
		cc.Port = v->Uint32Value();

		v = obj->Get(ToStr(isolate, "User"));
		if (!v->IsString()) {
			ThrowException(isolate, "Invalid user id string");
			return false;
		}
		String::Utf8Value uid(v);
		cc.UserId = Utilities::ToWide(*uid);

		v = obj->Get(ToStr(isolate, "Pwd"));
		if (!v->IsString()) {
			ThrowException(isolate, "Invalid password string");
			return false;
		}
		String::Utf8Value pwd(v);
		cc.Password = Utilities::ToWide(*pwd);

		v = obj->Get(ToStr(isolate, "EM"));
		if (!v->IsUint32()) {
			ThrowException(isolate, "Encryption method expected");
			return false;
		}
		unsigned int em = v->Uint32Value();
		if (em > 1) {
			ThrowException(isolate, "Invalid encryption method");
			return false;
		}
		cc.EncrytionMethod = (SPA::tagEncryptionMethod)em;

		v = obj->Get(ToStr(isolate, "Zip"));
		if (!v->IsBoolean()) {
			ThrowException(isolate, "Boolean value expected for Zip");
			return false;
		}
		cc.Zip = v->BooleanValue();

		v = obj->Get(ToStr(isolate, "V6"));
		if (!v->IsBoolean()) {
			ThrowException(isolate, "Boolean value expected for V6");
			return false;
		}
		cc.V6 = v->BooleanValue();

		v = obj->Get(ToStr(isolate, "AnyData"));
		if (!From(v, "", cc.AnyData)) {
			ThrowException(isolate, "Invalid data for AnyData");
			return false;
		}
		return true;
	}

	void NJSocketPool::StartSocketPool(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (!obj->IsValid(isolate))
			return;
		auto p1 = args[1];
		if (!p1->IsUint32()) {
			ThrowException(isolate, "An unsigned int number expected for client sockets");
			return;
		}
		unsigned int sessions = p1->Uint32Value();
		if (!sessions) {
			ThrowException(isolate, "The number of client sockets cannot be zero");
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
					ThrowException(isolate, "Invalid connection context found");
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
			ThrowException(isolate, "One or an array of connection contexts expected");
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
				ThrowException(isolate, "A callback expected for tracking pool event");
			}
		}
	}

	void NJSocketPool::setResultReturned(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsFunction()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_rr.Reset(isolate, Local<Function>::Cast(p));
			}
			else if (p->IsNullOrUndefined()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_rr.Empty();
			}
			else {
				ThrowException(isolate, "A callback expected for tracking request returned result");
			}
		}
	}

	void NJSocketPool::setAllProcessed(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsFunction()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_ap.Reset(isolate, Local<Function>::Cast(p));
			}
			else if (p->IsNullOrUndefined()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_ap.Empty();
			}
			else {
				ThrowException(isolate, "A callback expected for tracking event that all requests are processed");
			}
		}
	}

	void NJSocketPool::setPush(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsFunction()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_push.Reset(isolate, Local<Function>::Cast(p));
			}
			else if (p->IsNullOrUndefined()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_push.Empty();
			}
			else {
				ThrowException(isolate, "A callback expected for tracking online message from server");
			}
		}
	}

	void NJSocketPool::setServerException(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsFunction()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_se.Reset(isolate, Local<Function>::Cast(p));
			}
			else if (p->IsNullOrUndefined()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_se.Empty();
			}
			else {
				ThrowException(isolate, "A callback expected for tracking exception from server");
			}
		}
	}

	void NJSocketPool::setBaseRequestProcessed(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		NJSocketPool* obj = ObjectWrap::Unwrap<NJSocketPool>(args.Holder());
		if (obj->IsValid(isolate)) {
			auto p = args[0];
			if (p->IsFunction()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_brp.Reset(isolate, Local<Function>::Cast(p));
			}
			else if (p->IsNullOrUndefined()) {
				SPA::CAutoLock al(obj->m_cs);
				obj->m_brp.Empty();
			}
			else {
				ThrowException(isolate, "A callback expected for tracking the event of base request processed");
			}
		}
	}
}
