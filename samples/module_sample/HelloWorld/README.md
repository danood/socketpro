# SocketPro server plug-in development with C/C++

1.	The sample is created to demonstrate how to create a dynamic linking library which may contain multiple services by use of C/C++. Although the sample is created with Visual C++ on window platforms, you can do so on all Linux platforms too.

2.	To create such a plug-in, the following methods, which is defined at the file {socketpro}/include/spa_module.h, must be realized as shown in this sample.
	- bool WINAPI InitServerLibrary (int param); //The method will be called from SocketPro server core right after the library is loaded
	- void WINAPI UninitServerLibrary(); //The method will be called from SocketPro server core right before the library is going to be unloaded
	- unsigned short WINAPI GetNumOfServices(); //SocketPro will use the method to query how many services the library has defined
	- unsigned int WINAPI GetAServiceID(unsigned short index); //The method will be called from SocketPro server core to query each service id on zero-based index
	- CSvsContext WINAPI GetOneSvsContext(unsigned int serviceId); //The method will be called from SocketPro server core to get service context for a given service id
	- unsigned short WINAPI GetNumOfSlowRequests(unsigned int serviceId); //The method will be called from SocketPro server core to query the number of slow requests for one service id
	- unsigned short WINAPI GetOneSlowRequestID(unsigned int serviceId, unsigned short index); //The method will be called from SocketPro server core to get a slow request id from given service id and zero-based index
	
3.	After creating such a library, you can load it at runtime by calling the method SocketProAdapter.ServerSide.CSocketProServer.DllManager.AddALibrary in C#. You can do so for all other development environments similarly.

