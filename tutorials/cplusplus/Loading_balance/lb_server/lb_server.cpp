// hw_server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "piImpl.h"

int main(int argc, char* argv[]) {
	CMySocketProServer MySocketProServer;
	if (!MySocketProServer.Run(20901)) {
		int errCode = MySocketProServer.GetErrorCode();
		std::cout << "Error happens with code = " << errCode << std::endl;
	} else
		CSocketProServer::Router::SetRouting(sidPi, sidPiWorker);
	std::cout << "Press any key to stop the server ......" << std::endl;
	::getchar();
	return 0;
}
