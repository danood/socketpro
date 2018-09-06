
#include "stdafx.h"
#include "njsqlite.h"

namespace NJA {

	Persistent<Function> NJSqlite::constructor;

	NJSqlite::NJSqlite(CSqlite *sqlite) : NJHandlerRoot(sqlite), m_sqlite(sqlite) {

	}

	NJSqlite::~NJSqlite() {
		Release();
	}

	bool NJSqlite::IsValid(Isolate* isolate) {
		if (!m_sqlite) {
			isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Async MySQL handler disposed")));
			return false;
		}
		return NJHandlerRoot::IsValid(isolate);
	}

	void NJSqlite::Init(Local<Object> exports) {
		Isolate* isolate = exports->GetIsolate();

		// Prepare constructor template
		Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
		tpl->SetClassName(String::NewFromUtf8(isolate, "CSqlite"));
		tpl->InstanceTemplate()->SetInternalFieldCount(3);

		NJHandlerRoot::Init(exports, tpl);

		constructor.Reset(isolate, tpl->GetFunction());
		exports->Set(String::NewFromUtf8(isolate, "CSqlite"), tpl->GetFunction());
	}

	Local<Object> NJSqlite::New(Isolate* isolate, CSqlite *ash, bool setCb) {
		SPA::UINT64 ptr = (SPA::UINT64)ash;
		Local<Value> argv[] = { Boolean::New(isolate, setCb), Number::New(isolate, (double)SECRECT_NUM), Number::New(isolate, (double)ptr) };
		Local<Context> context = isolate->GetCurrentContext();
		Local<Function> cons = Local<Function>::New(isolate, constructor);
		return cons->NewInstance(context, 3, argv).ToLocalChecked();
	}

	void NJSqlite::New(const FunctionCallbackInfo<Value>& args) {
		Isolate* isolate = args.GetIsolate();
		if (args.IsConstructCall()) {
			if (args[0]->IsBoolean() && args[1]->IsNumber() && args[1]->IntegerValue() == SECRECT_NUM && args[2]->IsNumber()) {
				bool setCb = args[0]->BooleanValue();
				SPA::INT64 ptr = args[2]->IntegerValue();
				NJSqlite *obj = new NJSqlite((CSqlite*)ptr);
				obj->Wrap(args.This());
				args.GetReturnValue().Set(args.This());
			}
			else {
				args.GetReturnValue().Set(Null(isolate));
			}
		}
		else {
			// Invoked as plain function `CSqlite()`, turn into construct call.
			Local<Context> context = isolate->GetCurrentContext();
			Local<Function> cons = Local<Function>::New(isolate, constructor);
			Local<Object> result = cons->NewInstance(context, 0, nullptr).ToLocalChecked();
			args.GetReturnValue().Set(result);
		}
	}

	void NJSqlite::Release() {
		{
			SPA::CAutoLock al(m_cs);
			if (m_sqlite) {
				m_sqlite = nullptr;
			}
		}
		NJHandlerRoot::Release();
	}
}