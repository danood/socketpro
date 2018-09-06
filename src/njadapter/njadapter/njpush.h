#pragma once


namespace NJA {

	class NJPush : public node::ObjectWrap {
	public:
		NJPush(SPA::IPushEx *p);
		NJPush(const NJPush &p) = delete;

		~NJPush();

	public:
		NJPush& operator=(const NJPush &p) = delete;
		static void Init(Local<Object> exports);
		static Local<Object> New(Isolate* isolate, SPA::IPushEx *p, bool setCb);

	private:
		static const SPA::INT64 SECRECT_NUM = 0x7f1bb0fce4a5;
		static void New(const FunctionCallbackInfo<Value>& args);

	private:
		static Persistent<Function> constructor;
		SPA::IPushEx *m_p;
	};
}
