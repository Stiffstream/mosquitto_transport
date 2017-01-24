/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief mosquitto library initializer stuff.
 */

#include <mosquitto_transport/initializer.hpp>
#include <mosquitto_transport/tools.hpp>

namespace mosquitto_transport {

lib_initializer_t::lib_initializer_t()
	{
		ensure_mosq_success( mosquitto_lib_init(),
			[]{ return "mosquitto_lib_init() failure"; } );
	}
lib_initializer_t::~lib_initializer_t()
	{
		mosquitto_lib_cleanup();
	}

} /* namespace mosquitto_transport */


