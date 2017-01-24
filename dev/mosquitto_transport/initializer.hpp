/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief mosquitto library initializer stuff.
 */

#pragma once

namespace mosquitto_transport {

/*!
 * \brief Class for initialization and deinitialization of mosquitto library.
 */
class lib_initializer_t
	{
	public :
		lib_initializer_t(const lib_initializer_t &) = delete;
		lib_initializer_t(lib_initializer_t &&) = delete;

		lib_initializer_t();
		~lib_initializer_t();
	};

} /* namespace mosquitto_transport */

