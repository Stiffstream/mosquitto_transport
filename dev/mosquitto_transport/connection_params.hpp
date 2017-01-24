/*
 * mosquitto_transport-0.4
 */

/*!
 * \file
 * \brief Connection parameters for MQTT client.
 */

#pragma once

#include <string>

namespace mosquitto_transport {

//
// connection_params_t
//
/*!
 * \brief Parameters for connection to MQTT broker.
 */
struct connection_params_t
	{
		std::string m_client_id;
		std::string m_host;
		unsigned int m_port = 1883;
		unsigned int m_keepalive = 30;

		//! Default constructor.
		connection_params_t()
			{}

		//! Constructor only for client ID and host values.
		/*!
		 * All other parameters receive default values.
		 */
		connection_params_t(
			std::string client_id,
			std::string host )
			:	m_client_id( std::move(client_id) )
			,	m_host( std::move(host) )
			{}

		//! Constructor for all parameters.
		connection_params_t(
			std::string client_id,
			std::string host,
			unsigned int port,
			unsigned int keepalive )
			:	m_client_id( std::move(client_id) )
			,	m_host( std::move(host) )
			,	m_port( port )
			,	m_keepalive( keepalive )
			{}
	};

} /* namespace mosquitto_transport */

