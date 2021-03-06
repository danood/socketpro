// hw_server.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "HWImpl.h"

class CMySocketProServer : public CSocketProServer {
protected:
	virtual bool OnSettingServer(unsigned int listeningPort, unsigned int maxBacklog, bool v6) {
		//amIntegrated and amMixed not supported yet
		CSocketProServer::Config::SetAuthenticationMethod(amOwn);

		//add service(s) into SocketPro server
		AddService();

		//create three chat groups
		PushManager::AddAChatGroup(1, L"R&D Department");
		PushManager::AddAChatGroup(2, L"Sales Department");
		PushManager::AddAChatGroup(3, L"Management Department");
		PushManager::AddAChatGroup(7, L"HR Department");

		return true; //true -- ok; false -- no listening server
	}

private:
	CSocketProService<HelloWorldPeer> m_HelloWorld;
	//One SocketPro server supports any number of services. You can list them here!

private:
	void AddService() {
		//No COM -- taNone; STA COM -- taApartment; and Free COM -- taFree
		bool ok = m_HelloWorld.AddMe(sidHelloWorld, taNone);
		ok = m_HelloWorld.AddSlowRequest(idSleepHelloWorld);
	}
};

int main(int argc, char* argv[]) {
	CMySocketProServer MySocketProServer;

	//test certificate and private key files are located at ../SocketProRoot/bin
#ifdef WIN32_64 //windows platforms
	MySocketProServer.UseSSL("intermediate.pfx", "", "mypassword");
	//MySocketProServer.UseSSL("root"/*"my"*/, "UDAParts Intermediate CA", ""); //or load cert and private key from windows system cert store
#else //non-windows platforms
	MySocketProServer.UseSSL("intermediate.cert.pem", "intermediate.key.pem", "mypassword");
#endif
	
	if (!MySocketProServer.Run(20901)) {
		int errCode = MySocketProServer.GetErrorCode();
		std::cout << "Error happens with code = " << errCode << std::endl;
	}
	std::cout << "Press any key to stop the server ......" << std::endl;
	::getchar();
	return 0;
}
