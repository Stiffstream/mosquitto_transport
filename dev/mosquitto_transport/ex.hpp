/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Exceptions for mosquitto_transport library.
 */

#pragma once

#include <stdexcept>

namespace mosquitto_transport {

/*!
 * \brief Base class for all exceptions.
 */
class ex_t : public std::runtime_error
	{
	public :
		ex_t( std::string what )
			:	std::runtime_error{ std::move(what) }
			{}
	};

} /* namespace mosquitto_transport */

