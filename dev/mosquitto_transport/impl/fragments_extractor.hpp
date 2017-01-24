/*
 * mosquitto_transport
 */

/*!
 * \file
 * \brief Helpers for spliting topic name to parts.
 * \since
 * v.0.3.0
 */

#pragma once

#include <mosquitto_transport/tools.hpp>

#include <boost/algorithm/string/split.hpp>

#include <functional>
#include <string>
#include <vector>

namespace mosquitto_transport {

namespace impl {

//
// splitted_topic_name_t
//
/*!
 * \brief A type of container with topic name splitted into parts.
 */
using splitted_topic_name_t = std::vector< std::string >;

//
// split_topic_name
//
/*!
 * \brief Helper function for deviding topic name into parts.
 *
 * \attention This implementation do not handle UTF-8!
 */
inline splitted_topic_name_t
split_topic_name( const std::string & topic_name )
	{
		ensure_with_explblock< ex_t >( topic_name.size() >= 1u,
			[]{ return "topic_name must be at least 1 symbol long"; } );

		splitted_topic_name_t result;
		boost::algorithm::split( result, topic_name,
				[]( std::string::value_type ch ) { return '/' == ch; } );

		return result;
	}

//
// fragments_extractor_t
//
/*!
 * \brief Helper class for getting parts of topic name one by one.
 *
 * Usage example:
 * \code
	const std::string & topic_name = ...;
	auto parsed_topic = impl::split_topic_name(topic_name);
	for(fragment_extractor_t fragments{ parsed_topic }; fragments.has_next();)
	{
		const std::string & f = fragments.next();
		...
	}
 * \endcode
 */
class fragments_extractor_t
	{
		//! Original parsed topic name.
		std::reference_wrapper< const splitted_topic_name_t > m_topic_name;
		//! The current position in topic name.
		splitted_topic_name_t::const_iterator m_it;

		fragments_extractor_t(
			const splitted_topic_name_t & topic_name,
			splitted_topic_name_t::const_iterator it )
			:	m_topic_name{ topic_name }
			,	m_it{ move(it) }
			{}

	public :
		fragments_extractor_t(
			//! Parsed topic name to be iterated.
			//! \attention This reference must remains valid for all
			//! lifetime of fragment_extractor_t instance.
			const splitted_topic_name_t & topic_name )
			:	m_topic_name{ topic_name }
			,	m_it{ begin(m_topic_name.get()) }
			{}

		operator bool() const { return m_it != m_topic_name.get().end(); }

		const splitted_topic_name_t::value_type &
		get() const { return *m_it; }

		const splitted_topic_name_t::value_type &
		operator*() const { return get(); }

		fragments_extractor_t
		next() const
			{
				auto next = m_it;
				return fragments_extractor_t{ m_topic_name.get(), ++next };
			}
	};

} /* namespace impl */

} /* namespace mosquitto_transport */

