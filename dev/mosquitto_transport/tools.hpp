/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Various helper tools.
 */

#pragma once

#include <mosquitto_transport/ex.hpp>

#include <mosquitto.h>

#include <fmt/format.h>

namespace mosquitto_transport {

/*!
 * \brief Helper for ensuring validity of some condition.
 */
template< typename EXCEPTION, typename EXPLANATION_LAMBDA >
void
ensure_with_explblock( bool condition, EXPLANATION_LAMBDA explanation )
	{
		if( !condition )
			throw EXCEPTION{ explanation() };
	}

/*!
 * \brief Check the result of mosquitto function call and
 * throws and exception if this result is not MOSQ_ERR_SUCCESS.
 */
template< typename EXPLANATION_LAMBDA >
void
ensure_mosq_success(
	// The result of call of some mosquitto function.
	int ret_code,
	EXPLANATION_LAMBDA explanation )
	{
		ensure_with_explblock< ex_t >( MOSQ_ERR_SUCCESS == ret_code, [&]{
				return fmt::format( "{}, ret_code={} ({}), errno={}",
						explanation(),
						ret_code,
						mosquitto_strerror(ret_code),
						errno );
			} );
	}

} /* namespace mosquitto_transport */

