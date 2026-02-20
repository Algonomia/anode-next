#pragma once

namespace nodes {

/**
 * Register all PostgreSQL database nodes
 */
void registerPostgresNodes();

/**
 * postgres_config - Configure PostgreSQL connection
 *
 * Properties:
 *   _host: Host (default: localhost)
 *   _port: Port (default: 5432)
 *   _database: Database name
 *   _user: Username
 *   _password: Password
 *
 * Outputs:
 *   connection: Connection string
 */
void registerPostgresConfigNode();

/**
 * postgres_query - Execute raw SQL query
 *
 * Inputs:
 *   query: SQL query string
 *
 * Outputs:
 *   csv: Query result as DataFrame
 */
void registerPostgresQueryNode();

/**
 * postgres_func - Call a PostgreSQL function with parameters
 *
 * Inputs:
 *   csv: Optional CSV for field resolution
 *   function: Function name
 *
 * Properties:
 *   _int_0, _int_1, ...: Integer parameters
 *   _string_0, _string_1, ...: String parameters
 *   _double_0, _double_1, ...: Double parameters
 *
 * Outputs:
 *   csv: Function result as DataFrame
 */
void registerPostgresFuncNode();

} // namespace nodes
