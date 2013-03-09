//
//  M A R I A D B + +
//
//	Author   : Sylvain Rochette Langlois
//	License  : Boost Software License (http://www.boost.org/users/license.html)
//

#include <mysql.h>
#include <boost/lexical_cast.hpp>
#include <mariadb++/account.hpp>

using namespace mariadb;

namespace mariadb
{
	extern int g_connection_count;
}

//
// Constructor
//
account::account(const char* host_name, const char* user_name, const char* password, const char* schema, u32 port, const char* unix_socket) :
	m_auto_commit(true),
	m_port(port),
	m_host_name(host_name),
	m_user_name(user_name),
	m_password(password),
	m_schema(schema ? schema : "")
{
	//
	// Extract port from host name if any
	//
	auto pos = m_host_name.find(':');

	if (pos != std::string::npos)
	{
		m_port = boost::lexical_cast<u32>(m_host_name.substr(pos + 1));
		m_host_name = m_host_name.substr(0, pos);
	}

	if (unix_socket)
		m_unix_socket = unix_socket;

	++g_connection_count;
}

//
// Destructor
//
account::~account()
{
	if (!(--g_connection_count))
		mysql_server_end();
}

//
// Get account informations
//
const std::string& account::host_name() const
{
	return m_host_name;
}

const std::string& account::user_name() const
{
	return m_user_name;
}

const std::string& account::password() const
{
	return m_password;
}

const std::string& account::unix_socket() const
{
	return m_unix_socket;
}

u32 account::port() const
{
	return m_port;
}

const std::string& account::ssl_key() const
{
	return m_ssl_key;
}

const std::string& account::ssl_certificate() const
{
	return m_ssl_certificate;
}

const std::string& account::ssl_ca() const
{
	return m_ssl_ca;
}

const std::string& account::ssl_ca_path() const
{
	return m_ssl_ca_path;
}

const std::string& account::ssl_cipher() const
{
	return m_ssl_cipher;
}

//
// Get / Set default schema
//
const std::string& account::schema() const
{
	return m_schema;
}

void account::set_schema(const char* schema)
{
	if (schema)
		m_schema = schema;
	else
		m_schema.clear();
}

//
// Establishing secure connections using SSL
//
void account::set_ssl(const char* key, const char* certificate, const char* ca, const char* ca_path, const char* cipher)
{
	if (key)
	{
		m_ssl_key = key;
		m_ssl_certificate = certificate;
		m_ssl_ca = ca;
		m_ssl_ca_path = ca_path;
		m_ssl_cipher = cipher;
	}
	else
	{
		m_ssl_key.clear();
		m_ssl_certificate.clear();
		m_ssl_ca.clear();
		m_ssl_ca_path.clear();
		m_ssl_cipher.clear();
	}
}

//
// Set auto commit mode
//
bool account::auto_commit() const
{
	return m_auto_commit;
}

void account::set_auto_commit(bool auto_commit)
{
	m_auto_commit = auto_commit;
}

//
// Get / Set options
//
const std::map<std::string, std::string>& account::options() const
{
	return m_options;
}

const std::string& account::option(const char* name) const
{
	auto value = m_options.find(name);

	if (value != m_options.end())
		return value->second;

	static std::string null;
	return null;
}

void account::set_option(const char* name, const char* value)
{
	m_options[name] = value;
}

void account::clear_options()
{
	m_options.clear();
}

//
// Account creation helper
//
account_ref account::create(const char* host_name, const char* user_name, const char* password, const char* schema, u32 port, const char* unix_socket)
{
	return account_ref(new account(host_name, user_name, password, schema, port, unix_socket));
}
