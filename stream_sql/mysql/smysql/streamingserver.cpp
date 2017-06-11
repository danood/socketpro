

#include "streamingserver.h"
#include "include/mysql/client_plugin.h"

CSetGlobals::CSetGlobals() {
	//defaults
	m_nParam = 0;
	DisableV6 = false;
	Port = 20902;
	TLSv = false;

	//set interface_version
	unsigned int version = MYSQL_VERSION_ID;
	async_sql_plugin.interface_version = (version << 8);
}

CStreamingServer::CStreamingServer(int nParam)
	: SPA::ServerSide::CSocketProServer(nParam), m_pMySql(nullptr) {
}

void CStreamingServer::Clean() {
	if (m_pMySql) {
		delete m_pMySql;
		m_pMySql = nullptr;
	}
}

CStreamingServer::~CStreamingServer() {
	Clean();
}

bool CStreamingServer::OnSettingServer(unsigned int listeningPort, unsigned int maxBacklog, bool v6) {
	m_pMySql = new SPA::ServerSide::CMysqlService;
    //amIntegrated and amMixed not supported yet
    CSocketProServer::Config::SetAuthenticationMethod(SPA::ServerSide::amOwn);

    //add MySQL streaming service into SocketPro server
    return AddService();
}

bool CStreamingServer::AddService() {
    bool ok = m_pMySql->AddMe(SPA::Mysql::sidMysql, SPA::taNone);
    if (!ok)
		return false;
	ok = m_pMySql->AddSlowRequest(SPA::UDB::idOpen);
	if (!ok)
		return false;
	ok = m_pMySql->AddSlowRequest(SPA::UDB::idBeginTrans);
	if (!ok)
		return false;
	ok = m_pMySql->AddSlowRequest(SPA::UDB::idEndTrans);
	if (!ok)
		return false;
	ok = m_pMySql->AddSlowRequest(SPA::UDB::idExecute);
	if (!ok)
		return false;
	ok = m_pMySql->AddSlowRequest(SPA::UDB::idPrepare);
	if (!ok)
		return false;
	ok = m_pMySql->AddSlowRequest(SPA::UDB::idExecuteParameters);
	if (!ok)
		return false;
    return true;
}