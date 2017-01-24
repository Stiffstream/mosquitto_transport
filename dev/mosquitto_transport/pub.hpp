/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Public part of library.
 */

#pragma once

#include <mosquitto_transport/encoder_decoder.hpp>
#include <mosquitto_transport/ex.hpp>

#include <so_5/all.hpp>

#include <mosquitto.h>

#include <memory>
#include <string>
#include <atomic>

namespace mosquitto_transport {

//
// instance_t
//
/*!
 * \brief Transport manager handle.
 */
class instance_t
	{
	public :
		instance_t() {}
		instance_t(
			so_5::environment_t & env,
			so_5::mbox_t mbox )
			:	m_env{ &env }
			,	m_mbox{ std::move(mbox) }
			{}

		so_5::environment_t &
		environment() const { return *m_env; }

		const so_5::mbox_t &
		mbox() const { return m_mbox; }

		operator bool() const { return nullptr != m_env; }

	private :
		so_5::environment_t * m_env{};
		so_5::mbox_t m_mbox;
	};

//
// failed_subscription_react_t
//
/*!
 * \brief Type of reaction on subscription failure.
 * \since
 * v.0.4.0
 */
enum class failed_subscription_react_t
	{
		//! Default reaction: throw an exception.
		//! This exception will be thrown on the context of
		//! transport_manager agent. And this will lead to
		//! application abortion or shutting down of SObjectizer.
		throw_exception,
		//! Sending of subscription_failure_t notification.
		send_notification
	};

/*!
 * \since
 * v.0.4.0
 */
constexpr failed_subscription_react_t notify_on_failure =
	failed_subscription_react_t::send_notification;

//
// failed_subscription_ex_t
//
/*!
 * \brief An exception to be thrown if subscription failed.
 */
class failed_subscription_ex_t : public ex_t
	{
	public :
		failed_subscription_ex_t(
			const std::string & topic_name,
			const std::string & description );
	};

//
// postman_t
//
/*!
 * \brief Interface of postman object.
 */
struct postman_t
	{
		virtual ~postman_t();

		virtual void
		subscription_available( const std::string & topic_name ) = 0;

		virtual void
		subscription_unavailable( const std::string & topic_name ) = 0;

		virtual void
		post( std::string topic_name, std::string payload ) = 0;

		/*!
		 * \brief Reaction on subscription failure.
		 *
		 * An exception thrown by default.
		 *
		 * \since
		 * v.0.4.0
		 */
		virtual void
		subscription_failed(
			const std::string & topic_name,
			const std::string & description );
	};

/*!
 * \brief Alias of shared_ptr for postman.
 */
using postman_shared_ptr_t = std::shared_ptr< postman_t >;

//
// broker_connected_t
//
/*!
 * \brief A signal to be sent when a connection to broker established.
 */
struct broker_connected_t : public so_5::signal_t {};

//
// broker_disconnected_t
//
/*!
 * \brief A signal to be sent when a connection to broker lost.
 */
struct broker_disconnected_t : public so_5::signal_t {};

//
// subscription_available_t
//
/*!
 * A message about availability of subscription.
 * This message is sent when subscription is acknowledged by MQTT broker.
 */
class subscription_available_t : public so_5::message_t
	{
		const std::string m_topic_name;

	public :
		subscription_available_t( std::string topic_name )
			:	m_topic_name{ move(topic_name) }
			{}

		const std::string &
		topic_name() const { return m_topic_name; }
	};

//
// subscription_unavailable_t
//
/*!
 * A message about unavailability of subscription.
 * This message is sent when connection to MQTT broker is lost.
 */
class subscription_unavailable_t : public so_5::message_t
	{
		const std::string m_topic_name;

	public :
		subscription_unavailable_t( std::string topic_name )
			:	m_topic_name{ move(topic_name) }
			{}

		const std::string &
		topic_name() const { return m_topic_name; }
	};

//
// subscription_failed_t
//
/*!
 * A message about subscription failure.
 * 
 * \since
 * v.0.4.0
 */
class subscription_failed_t : public so_5::message_t
	{
		const std::string m_topic_name;
		const std::string m_description;

	public :
		subscription_failed_t(
			std::string topic_name,
			std::string description )
			:	m_topic_name{ move(topic_name) }
			,	m_description{ move(description) }
			{}

		const std::string &
		topic_name() const { return m_topic_name; }

		const std::string &
		description() const { return m_description; }
	};

//
// subscribe_topic_t
//
/*!
 * \brief Message for subscription to a topic.
 */
struct subscribe_topic_t : public so_5::message_t
	{
		const std::string m_topic_name;
		const postman_shared_ptr_t m_postman;

		subscribe_topic_t(
			std::string topic_name,
			postman_shared_ptr_t postman )
			:	m_topic_name{ std::move(topic_name) }
			,	m_postman{ std::move(postman) }
			{}
	};

//
// unsubscribe_topic_t
//
/*!
 * \brief Message for unsubscription from a topic.
 */
struct unsubscribe_topic_t : public so_5::message_t
	{
		const std::string m_topic_name;
		const postman_shared_ptr_t m_postman;

		unsubscribe_topic_t(
			std::string topic_name,
			postman_shared_ptr_t postman )
			:	m_topic_name{ std::move(topic_name) }
			,	m_postman{ std::move(postman) }
			{}
	};

//
// topic_mbox_t
//
/*!
 * \brief Implementation of mbox for messages from subscribed topics.
 */
class topic_mbox_t : public so_5::abstract_message_box_t
	{
	public :
		topic_mbox_t(
			//! Topic to be subscribed.
			std::string topic_name,
			//! Manager's mbox.
			//! This mbox will be used for subscribe_topic_t and
			//! unsubscribe_topic_t messages.
			so_5::mbox_t manager,
			//! Actual mbox.
			//! This mbox will be used for all mbox-related actions.
			so_5::mbox_t actual_mbox,
			//! Postman for message delivery.
			postman_shared_ptr_t postman );

		unsigned int
		subscribers_count() const;

		virtual so_5::mbox_id_t
		id() const override;

		virtual void
		subscribe_event_handler(
			const std::type_index & type_index,
			const so_5::message_limit::control_block_t * limit,
			so_5::agent_t * subscriber ) override;

		virtual void
		unsubscribe_event_handlers(
			const std::type_index & type_index,
			so_5::agent_t * subscriber ) override;

		virtual std::string
		query_name() const override;

		virtual so_5::mbox_type_t
		type() const override;

		virtual void
		do_deliver_message(
			const std::type_index & msg_type,
			const so_5::message_ref_t & message,
			unsigned int overlimit_reaction_deep ) const override;

		virtual void
		do_deliver_service_request(
			const std::type_index & msg_type,
			const so_5::message_ref_t & message,
			unsigned int overlimit_reaction_deep ) const override;

		virtual void
		set_delivery_filter(
			const std::type_index & msg_type,
			const so_5::delivery_filter_t & filter,
			so_5::agent_t & subscriber ) override;

		virtual void
		drop_delivery_filter(
			const std::type_index & msg_type,
			so_5::agent_t & subscriber ) SO_5_NOEXCEPT override;

	private :
		//! Topic to be subscribed.
		const std::string m_topic_name;
		//! Manager's mbox.
		const so_5::mbox_t m_manager;
		//! Actual mbox for all mbox-related actions.
		const so_5::mbox_t m_actual_mbox;
		//! Postman for actual message delivery.
		const postman_shared_ptr_t m_postman;

		std::atomic< unsigned int > m_subscribers;
	};

//
// incoming_message_t
//
template< typename DECODER_TAG >
class incoming_message_t : public so_5::message_t
	{
		const std::string m_topic_name;
		const std::string m_payload;

	public :
		incoming_message_t( std::string topic_name, std::string payload )
			:	m_topic_name{ std::move(topic_name) }
			,	m_payload{ std::move(payload) }
			{}

		const std::string &
		topic_name() const { return m_topic_name; }

		const std::string &
		payload() const { return m_payload; }

		template< typename MSG >
		MSG decode() const
			{
				return decoder_t< DECODER_TAG, MSG >::decode( this->payload() );
			}
	};

namespace details {

//
// actual_postman_t
//
template< typename DECODER_TAG >
class actual_postman_t : public postman_t
	{
		//! Destination for incoming messages.
		const so_5::mbox_t m_dest;

		//! Reaction on failed subscription.
		/*!
		 * \since
		 * v.0.4.0
		 */
		failed_subscription_react_t m_on_failure;

	public :
		actual_postman_t(
			so_5::mbox_t dest,
			failed_subscription_react_t on_failure )
			:	m_dest{ std::move(dest) }
			,	m_on_failure{ on_failure }
			{}

		virtual void
		subscription_available( const std::string & topic_name ) override
			{
				so_5::send< subscription_available_t >( m_dest, topic_name );
			}

		virtual void
		subscription_unavailable( const std::string & topic_name ) override
			{
				so_5::send< subscription_unavailable_t >( m_dest, topic_name );
			}

		virtual void
		post( std::string topic_name, std::string payload ) override
			{
				so_5::send< incoming_message_t< DECODER_TAG > >(
						m_dest, std::move(topic_name), std::move(payload) );
			}

		virtual void
		subscription_failed(
			const std::string & topic_name,
			const std::string & description ) override
			{
				if( failed_subscription_react_t::send_notification == m_on_failure )
					so_5::send< subscription_failed_t >(
							m_dest,
							topic_name, description );
				else
					// Exception will be thrown by default implementation.
					postman_t::subscription_failed( topic_name, description );
			}
	};

} /* namespace details */

//
// topic_subscriber_t
//
template< typename DECODER_TAG >
struct topic_subscriber_t
	{
		using msg_type = incoming_message_t< DECODER_TAG >;

		template< typename LAMBDA >
		static void
		subscribe(
			const instance_t & instance,
			const std::string & topic_name,
			LAMBDA subscription_actions,
			failed_subscription_react_t on_failure =
				failed_subscription_react_t::throw_exception );
	};

template< typename DECODER_TAG >
template< typename LAMBDA >
void
topic_subscriber_t< DECODER_TAG >::subscribe(
	const instance_t & instance,
	const std::string & topic_name,
	LAMBDA subscription_actions,
	failed_subscription_react_t on_failure )
	{
		using namespace details;

		auto actual_mbox = instance.environment().create_mbox();

		postman_shared_ptr_t postman{
				new actual_postman_t< DECODER_TAG >{ actual_mbox, on_failure } };

		auto tm = new topic_mbox_t{
				topic_name,
				instance.mbox(), 
				actual_mbox,
				postman };
		so_5::mbox_t tm_mbox{ tm };

		subscription_actions( tm_mbox );

		if( 0 != tm->subscribers_count() )
			// There are some subscriptions.
			// Manager should handle this subscription.
			so_5::send< subscribe_topic_t >(
					instance.mbox(), topic_name, postman );
	}

//
// publish_message_t
//
struct publish_message_t : public so_5::message_t
	{
		const std::string m_topic_name;
		const std::string m_payload;

		publish_message_t( std::string topic_name, std::string payload )
			:	m_topic_name{ std::move(topic_name) }
			,	m_payload{ std::move(payload) }
			{}
	};

//
// topic_publisher_t
//
template< typename ENCODER_TAG >
struct topic_publisher_t
	{
		template< typename MSG >
		static void
		publish(
			const instance_t & instance,
			std::string topic_name,
			const MSG & msg );
	};

template< typename ENCODER_TAG >
template< typename MSG >
void
topic_publisher_t< ENCODER_TAG >::publish(
	const instance_t & instance,
	std::string topic_name,
	const MSG & msg )
	{
		so_5::send< publish_message_t >(
				instance.mbox(),
				std::move(topic_name),
				encoder_t< ENCODER_TAG, MSG >::encode( msg ) );

	}

} /* namespace mosquitto_transport */

