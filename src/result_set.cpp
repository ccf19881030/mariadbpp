//
//  M A R I A D B + +
//
//	Author   : Sylvain Rochette Langlois
//	License  : Boost Software License (http://www.boost.org/users/license.html)
//

#include <mysql/mysql.h>
#include <memory.h>
#include <mariadb++/connection.hpp>
#include <mariadb++/result_set.hpp>
#include <mariadb++/conversion_helper.hpp>
#include "bind.hpp"
#include "private.hpp"

using namespace mariadb;

result_set::result_set(connection* connection) :
	m_field_count(0),
	m_lengths(NULL),
	m_result_set(mysql_store_result(connection->m_mysql)),
	m_fields(NULL),
	m_my_binds(NULL),
	m_binds(NULL),
	m_stmt_data(NULL),
    m_row(nullptr),
    m_has_result(false)
{
	if (m_result_set)
	{
		m_field_count = mysql_num_fields(m_result_set);
		m_fields = mysql_fetch_fields(m_result_set);

		for (u32 i = 0; i < m_field_count; ++i)
			m_indexes[m_fields[i].name] = i;
	}
}

result_set::result_set(const statement_data_ref &stmt_data) :
	m_field_count(0),
	m_lengths(NULL),
	m_result_set(NULL),
	m_fields(NULL),
	m_my_binds(NULL),
	m_binds(NULL),
	m_stmt_data(stmt_data),
    m_row(nullptr),
    m_has_result(false)
{
	int max_length = 1;
	mysql_stmt_attr_set(stmt_data->m_statement, STMT_ATTR_UPDATE_MAX_LENGTH, &max_length);

	if (mysql_stmt_store_result(stmt_data->m_statement))
		STMT_ERROR(stmt_data->m_statement)
	else
	{
		m_field_count = mysql_stmt_field_count(stmt_data->m_statement);
		m_result_set = mysql_stmt_result_metadata(stmt_data->m_statement);

        if (m_field_count > 0) {
            m_fields = mysql_fetch_fields(m_result_set);
            m_binds = new bind[m_field_count];
            m_my_binds = new MYSQL_BIND[m_field_count];
            m_row = new char *[m_field_count];

            memset(m_my_binds, 0, sizeof(MYSQL_BIND) * m_field_count);

            for (u32 i = 0; i < m_field_count; ++i) {
                m_indexes[m_fields[i].name] = i;
                m_binds[i].set_output(m_fields[i], &m_my_binds[i]);
                m_row[i] = m_binds[i].buffer();
            }

            mysql_stmt_bind_result(stmt_data->m_statement, m_my_binds);
        }
	}
}

statement_data::~statement_data() {
	if (m_my_binds)
		delete [] m_my_binds;

	if (m_binds)
		delete [] m_binds;

	if (m_statement)
		mysql_stmt_close(m_statement);
}

result_set::~result_set()
{
	if (m_result_set)
		mysql_free_result(m_result_set);

	if (m_my_binds)
		delete [] m_my_binds;

	if (m_stmt_data)
	{
		if (m_row)
			delete [] m_row;

		mysql_stmt_free_result(m_stmt_data->m_statement);
	}

	if (m_binds)
		delete [] m_binds;
}

u64 result_set::column_count() const
{
    return m_field_count;
}

value::type result_set::column_type(u32 index)
{
    if (index >= m_field_count)
        throw std::out_of_range("Column index out of range");

    bool is_unsigned = (m_fields[index].flags & UNSIGNED_FLAG) == UNSIGNED_FLAG;

    switch (m_fields[index].type)
    {
        case MYSQL_TYPE_NULL:
            return value::null;

        case MYSQL_TYPE_BIT:
            return value::boolean;

        case MYSQL_TYPE_FLOAT:
            return value::float32;

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return value::decimal;

        case MYSQL_TYPE_DOUBLE:
            return value::double64;

        case MYSQL_TYPE_NEWDATE:
        case MYSQL_TYPE_DATE:
            return value::date;

        case MYSQL_TYPE_TIME:
            return value::time;

        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
            return value::date_time;

        case MYSQL_TYPE_TINY:
            return (is_unsigned ? value::unsigned8 : value::signed8);

        case MYSQL_TYPE_YEAR:
        case MYSQL_TYPE_SHORT:
            return (is_unsigned ? value::unsigned16 : value::signed16);

        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
            return (is_unsigned ? value::unsigned32 : value::signed32);

        case MYSQL_TYPE_LONGLONG:
            return (is_unsigned ? value::unsigned64 : value::signed64);

        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
            return value::blob;

        case MYSQL_TYPE_ENUM:
            return value::enumeration;

        default:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            return value::string;
    }
}

const std::string result_set::column_name(u32 index)
{
    if (index >= m_field_count)
        throw std::out_of_range("Column index out of range");

    return m_fields[index].name;
}

u32 result_set::column_index(const std::string &name) const
{
    const map_indexes_t::const_iterator i = m_indexes.find(name);

    if (i == m_indexes.end())
        return 0xffffffff;

    return i->second;
}

unsigned long result_set::column_size(u32 index) const {

    if (index >= m_field_count)
        throw std::out_of_range("Column index out of range");

    return m_stmt_data ? m_binds[index].length() : m_lengths[index];
}

bool result_set::set_row_index(u64 index)
{
    if (m_stmt_data)
        mysql_stmt_data_seek(m_stmt_data->m_statement, index);
    else
        mysql_data_seek(m_result_set, index);
    return next();
}

bool result_set::next()
{
    if (!m_result_set)
        return (m_has_result = false);

    if (m_stmt_data)
        return (m_has_result = !mysql_stmt_fetch(m_stmt_data->m_statement));

    m_row = mysql_fetch_row(m_result_set);
    m_lengths = mysql_fetch_lengths(m_result_set);

    // make sure no access to results is possible until a result is successfully fetched
    return (m_has_result = m_row != nullptr);
}

u64 result_set::row_index() const
{
    if (m_stmt_data)
        return (u64)mysql_stmt_row_tell(m_stmt_data->m_statement);

    return (u64)mysql_row_tell(m_result_set);
}

u64 result_set::row_count() const
{
    if (m_stmt_data)
        return (u64)mysql_stmt_num_rows(m_stmt_data->m_statement);

    return (u64)mysql_num_rows(m_result_set);
}

void result_set::check_result_exists() const {
    if(!m_has_result)
        throw std::out_of_range("No row was fetched");
}

MAKE_GETTER(blob, stream_ref)
    size_t len = column_size(index);

    if (len == 0)
        return stream_ref();

    auto *ss = new std::istringstream();
    ss->rdbuf()->pubsetbuf(m_row[index], len);
    return stream_ref(ss);
}

MAKE_GETTER(data, data_ref)
	size_t len = column_size(index);

	return len == 0 ? data_ref() : data_ref(new data<char>(m_row[index], len));
}

MAKE_GETTER(string, std::string)
	return std::string(m_row[index], column_size(index));
}

MAKE_GETTER(date, date_time)
	if (m_stmt_data)
		return mariadb::date_time(m_binds[index].m_time);

	return date_time(m_row[index]).date();
}

MAKE_GETTER(date_time, date_time)
	if (m_stmt_data)
		return mariadb::date_time(m_binds[index].m_time);

	return date_time(m_row[index]);
}

MAKE_GETTER(time, mariadb::time)
	if (m_stmt_data)
		return mariadb::time(m_binds[index].m_time);

	return mariadb::time(m_row[index]);
}

MAKE_GETTER(decimal, decimal)
	return decimal(m_row[index]);
}

MAKE_GETTER(boolean, bool)
	if (m_stmt_data)
		return (m_binds[index].m_unsigned64 != 0);

	return string_cast<bool>(m_row[index]);
}

MAKE_GETTER(unsigned8, u8)
	if (m_stmt_data)
		return checked_cast<u8>(0x00000000000000ff & m_binds[index].m_unsigned64);

	return string_cast<u8>(m_row[index]);
}

MAKE_GETTER(signed8, s8)
	if (m_stmt_data)
		return checked_cast<s8>(0x00000000000000ff & m_binds[index].m_signed64);

	return string_cast<s8>(m_row[index]);
}

MAKE_GETTER(unsigned16, u16)
	if (m_stmt_data)
		return checked_cast<u16>(0x000000000000ffff & m_binds[index].m_unsigned64);

	return string_cast<u16>(m_row[index]);
}

MAKE_GETTER(signed16, s16)
	if (m_stmt_data)
		return checked_cast<s16>(0x000000000000ffff & m_binds[index].m_signed64);

	return string_cast<s16>(m_row[index]);
}

MAKE_GETTER(unsigned32, u32)
	if (m_stmt_data)
		return checked_cast<u32>(0x00000000ffffffff & m_binds[index].m_unsigned64);

	return string_cast<u32>(m_row[index]);
}

MAKE_GETTER(signed32, s32)
	if (m_stmt_data)
		return m_binds[index].m_signed32[0];

	return string_cast<s32>(m_row[index]);
}

MAKE_GETTER(unsigned64, u64)
	if (m_stmt_data)
		return m_binds[index].m_unsigned64;

	return string_cast<u64>(m_row[index]);
}

MAKE_GETTER(signed64, s64)
	if (m_stmt_data)
		return m_binds[index].m_signed64;

	return string_cast<s64>(m_row[index]);
}

MAKE_GETTER(float, f32)
	if (m_stmt_data)
		return m_binds[index].m_float32[0];

	return string_cast<f32>(m_row[index]);
}

MAKE_GETTER(double, f64)
	if (m_stmt_data)
		return checked_cast<f64>(m_binds[index].m_double64);

	return string_cast<f64>(m_row[index]);
}

MAKE_GETTER(is_null, bool)
	if (m_stmt_data)
		return m_binds[index].is_null();

	return !m_row[index];
}

