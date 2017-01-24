/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Main transport manager agent.
 */

#pragma once

#include <mosquitto_transport/initializer.hpp>
#include <mosquitto_transport/pub.hpp>
#include <mosquitto_transport/connection_params.hpp>

#include <mosquitto_transport/impl/subscriptions_map.hpp>

#include <mosquitto.h>

#include <so_5/all.hpp>

#include <spdlog/spdlog.h>

#include <boost/container/flat_set.hpp>

#include <memory>

namespace mosquitto_transport {

namespace details {

namespace bcnt = boost::container;

using mosquitto_unique_ptr_t =
	std::unique_ptr< mosquitto, void (*)(mosquitto*) >;

//
// subscription_status_t
//
enum class subscription_status_t
	{
		//! New topic. No subscription attempts yet.
		new_subscription,
		//! Topic successfully subscribed.
		subscribed,
		//! Subscription lost because of disconnection from broker.
		unsubscribed,
		//! Attempt of subscription failed.
		failed
	};

//
// subscription_info_t
//
class subscription_info_t
	{
	public :
		subscription_info_t();

		subscription_status_t
		status() const;

		void
		subscription_created(
			const std::string & topic_name );

		void
		subscription_lost(
			const std::string & topic_name );

		void
		subscription_failed(
			const std::string & topic_name,
			const std::string & description );

		bool
		has_postmans() const;

		void
		add_postman(
			const std::string & topic_name,
			const postman_shared_ptr_t & postman );

		void
		remove_postman( const postman_shared_ptr_t & postman );

		void
		deliver_message(
			const std::string & topic,
			const std::string & payload );

	private :
		bcnt::flat_set< postman_shared_ptr_t > m_postmans;
		subscription_status_t m_status;

		/*!
		 * \note Has value only if m_status == subscription_status_t::failed.
		 *
		 * \since
		 * v.0.4.0
		 */
		std::string m_failure_description;
	};

//
// delivery_map_t
//
/*!
 * Type of subscriptions_map to be used for incoming message delivery.
 */
using delivery_map_t = impl::subscriptions_map_t< subscription_info_t * >;

//
// pending_subscription_t
//
struct pending_subscription_t
	{
		std::string m_topic_name;
		std::chrono::steady_clock::time_point m_initiated_at;
	};

//
// subscription_result_t
//
struct subscription_result_t : public so_5::message_t
	{
		int m_mid;
		std::vector< int > m_granted_qos;

		subscription_result_t(
			int mid,
			std::vector< int > granted_qos )
			:	m_mid{ mid }
			,	m_granted_qos{ std::move(granted_qos) }
			{}
	};

//
// message_received_t
//
struct message_received_t : public so_5::message_t
	{
		const std::string m_topic;
		const std::string m_payload;

		message_received_t(
			const mosquitto_message & mosq_msg )
			:	m_topic( mosq_msg.topic )
			,	m_payload(
					reinterpret_cast< const char * >(mosq_msg.payload),
					static_cast< std::size_t >(mosq_msg.payloadlen) )
			{}
	};

} /* namespace details */

//
// a_transport_manager_t
//
/*!
 * \brief Main transport manager agent.
 */
class a_transport_manager_t : public so_5::agent_t
	{
	public :
		a_transport_manager_t(
			context_t ctx,
			lib_initializer_t & lib_initializer,
			connection_params_t connection_params,
			// Logger to be used by transport manager.
			std::shared_ptr< spdlog::logger > logger );

		virtual void
		so_define_agent() override;

		virtual void
		so_evt_start() override;

		virtual void
		so_evt_finish() override;

		instance_t
		instance() const;

		//! Sets the will for client.
		/*!
		 * \note This method must be called before agent will be registered.
		 *
		 * \throw ex_t in case of mosquitto_will_set failure.
		 */
		void
		mqtt_will_set(
			//! Topic name for will.
			const std::string & topic_name,
			//! Payload for will message.
			const std::string & payload,
			//! QoS for the will. QoS=0 by default.
			int qos = 0,
			//! Will this message be retained? No by default.
			bool retain = false );

		//! Set the subscription timeout.
		/*!
		 * A timeout of 60s is used by default.
		 *
		 * \note This method must be called before agent will be registered.
		 *
		 * \since
		 * v.0.4.0
		 */
		void
		set_subscription_timeout(
			std::chrono::steady_clock::duration timeout );

	private :
		struct connected_t : public so_5::signal_t {};
		struct disconnected_t : public so_5::signal_t {};
		struct pending_subscriptions_timer_t : public so_5::signal_t {};

		using subscription_info_map_t =
				std::map< std::string, details::subscription_info_t >;

		using mid_to_topic_map_t =
				std::map< int, details::pending_subscription_t >;

		const connection_params_t m_connection_params;

		// Mbox for broker_connected and broker_disconnected
		// broadcast notifications.
		const so_5::mbox_t m_self_mbox;

		// Logger to be used by transport manager.
		const std::shared_ptr< spdlog::logger > m_logger;

		details::mosquitto_unique_ptr_t m_mosq;

		state_t st_working{ this, "working" };
		state_t st_disconnected{
				initial_substate_of{ st_working }, "disconnected" };
		state_t st_connected{
				substate_of{ st_working }, "connected" };

		// Map of all registered subscriptions.
		// This map contains topic filters with and without wildcards. 
		subscription_info_map_t m_registered_subscriptions;

		// Map of topic filters to be used for incoming message delivery.
		details::delivery_map_t m_delivery_map;

		// Info about pending subscriptions.
		mid_to_topic_map_t m_pending_subscriptions;

		// Timer for checking pending subscriptions.
		so_5::timer_id_t m_pending_subscriptions_timer;

		// Time for subscription completion.
		std::chrono::steady_clock::duration m_subscription_timeout{
			std::chrono::seconds{60} };

		void
		setup_mosq_callbacks();

		static void
		on_connect_callback(
			mosquitto *,
			void * this_object,
			int connect_result );

		static void
		on_disconnect_callback(
			mosquitto *,
			void * this_object,
			int disconnect_result );

		static void
		on_subscribe_callback(
			mosquitto *,
			void * this_object,
			int mid,
			int qos_count,
			const int * qos_items );

		static void
		on_message_callback(
			mosquitto *,
			void * this_object,
			const mosquitto_message * msg );

		static void
		on_log_callback(
			mosquitto *,
			void * this_object,
			int log_level,
			const char * log_msg );

		void
		on_connected();

		void
		on_disconnected();

		void
		on_subscribe_topic( const subscribe_topic_t & cmd );

		void
		on_unsubscribe_topic( const unsubscribe_topic_t & cmd );

		void
		on_subscription_result(
			const details::subscription_result_t & cmd );

		void
		on_pending_subscriptions_timer(
			mhood_t< pending_subscriptions_timer_t > );

		void
		on_message_received(
			const details::message_received_t & cmd );

		void
		on_publish_message(
			const publish_message_t & cmd );

		void
		try_subscribe_topic(
			const std::string & topic_name );

		void
		do_subscription_actions(
			const std::string & topic_name );

		void
		process_subscription_result(
			const std::string & topic_name,
			details::subscription_info_t & info,
			int granted_qos );

		void
		drop_subscription_statuses();

		void
		restore_subscriptions_on_reconnect();
	};

} /* namespace mosquitto_transport */

