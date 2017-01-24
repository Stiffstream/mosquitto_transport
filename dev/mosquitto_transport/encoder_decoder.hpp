/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Stuff related to message representation encoding and decoding
 */

#pragma once

#include <string>

namespace mosquitto_transport {

//
// decoder_t
//
template< typename TAG, typename RESULT_TYPE >
struct decoder_t
	{
		static RESULT_TYPE decode( const std::string & );
	};

//
// encoder_t
//
template< typename TAG, typename SOURCE_TYPE >
struct encoder_t
	{
		static std::string encode( const SOURCE_TYPE & );
	};

} /* namespace mosquitto_transport */

