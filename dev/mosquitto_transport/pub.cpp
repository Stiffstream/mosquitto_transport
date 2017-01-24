/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Public part of library.
 */

#include <mosquitto_transport/pub.hpp>

#include <fmt/format.h>

namespace mosquitto_transport {

//
// failed_subscription_ex_t
//
/*!
 * \brief An exception to be thrown if subscription failed.
 */
failed_subscription_ex_t::failed_subscription_ex_t(
	const std::string & topic_name,
	const std::string & description )
	:	ex_t{
			fmt::format(
				"subscription failed, topic_filter='{}', "
				"description='{}'", topic_name, description ) }
	{}

//
// postman_t
//
postman_t::~postman_t() {}

void
postman_t::subscription_failed(
	const std::string & topic_name,
	const std::string & description )
	{
		throw failed_subscription_ex_t{ topic_name, description };
	}

//
// topic_mbox_t
//
topic_mbox_t::topic_mbox_t(
	std::string topic_name,
	so_5::mbox_t manager,
	so_5::mbox_t actual_mbox,
	postman_shared_ptr_t postman )
	:	m_topic_name{ std::move(topic_name) }
	,	m_manager{ std::move(manager) }
	,	m_actual_mbox{ std::move(actual_mbox) }
	,	m_postman{ std::move(postman) }
	{
	}

unsigned int
topic_mbox_t::subscribers_count() const
	{
		return m_subscribers.load( std::memory_order_acquire );
	}

so_5::mbox_id_t
topic_mbox_t::id() const
	{
		return m_actual_mbox->id();
	}

void
topic_mbox_t::subscribe_event_handler(
	const std::type_index & type_index,
	const so_5::message_limit::control_block_t * limit,
	so_5::agent_t * subscriber )
	{
		m_actual_mbox->subscribe_event_handler( type_index, limit, subscriber );
		++m_subscribers;
	}

void
topic_mbox_t::unsubscribe_event_handlers(
	const std::type_index & type_index,
	so_5::agent_t * subscriber )
	{
		m_actual_mbox->unsubscribe_event_handlers( type_index, subscriber );
		if( 0 == --m_subscribers )
			so_5::send< unsubscribe_topic_t >( m_manager,
					m_topic_name, m_postman );
	}

std::string
topic_mbox_t::query_name() const
	{
		return m_actual_mbox->query_name();
	}

so_5::mbox_type_t
topic_mbox_t::type() const
	{
		return m_actual_mbox->type();
	}

void
topic_mbox_t::do_deliver_message(
	const std::type_index & msg_type,
	const so_5::message_ref_t & message,
	unsigned int overlimit_reaction_deep ) const
	{
		m_actual_mbox->do_deliver_message(
				msg_type,
				message,
				overlimit_reaction_deep );
	}

void
topic_mbox_t::do_deliver_service_request(
	const std::type_index & msg_type,
	const so_5::message_ref_t & message,
	unsigned int overlimit_reaction_deep ) const
	{
		m_actual_mbox->do_deliver_service_request(
				msg_type,
				message,
				overlimit_reaction_deep );
	}

void
topic_mbox_t::set_delivery_filter(
	const std::type_index & msg_type,
	const so_5::delivery_filter_t & filter,
	so_5::agent_t & subscriber )
	{
		m_actual_mbox->set_delivery_filter( msg_type, filter, subscriber );
	}

void
topic_mbox_t::drop_delivery_filter(
	const std::type_index & msg_type,
	so_5::agent_t & subscriber ) SO_5_NOEXCEPT
	{
		m_actual_mbox->drop_delivery_filter( msg_type, subscriber );
	}

} /* namespace mosquitto_transport */

