
#include "stdafx.h"
#include "njobjects.h"
#include "njqueue.h"
#include "njhandler.h"

namespace NJA {
	void Destroy(const FunctionCallbackInfo<Value>& args) {

	}

	void InitAll(Local<Object> exports) {
		NODE_SET_METHOD(exports, "destroy", Destroy);
		NJQueue::Init(exports);
		NJSocketPool::Init(exports);
		NJHandler::Init(exports);
		SPA::CScopeUQueue sb;
	}
	NODE_MODULE(NODE_GYP_MODULE_NAME, InitAll)
}
