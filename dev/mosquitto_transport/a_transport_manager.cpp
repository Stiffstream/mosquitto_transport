/*
 * mosquitto_transport-1.0
 */

/*!
 * \file
 * \brief Main transport manager agent.
 */

#include <mosquitto_transport/a_transport_manager.hpp>
#include <mosquitto_transport/tools.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <algorithm>
#include <iterator>

namespace mosquitto_transport {

namespace details {

constexpr int qos_to_use = 0;

//
// subscription_info_t
//
subscription_info_t::subscription_info_t()
	:	m_status{ subscription_status_t::new_subscription }
	{}

subscription_status_t
subscription_info_t::status() const
	{ return m_status; }

void
subscription_info_t::subscription_created(
	const std::string & topic_name )
	{
		m_status = subscription_status_t::subscribed;
		m_failure_description.clear();

		for( const auto & p : m_postmans )
			p->subscription_available( topic_name );
	}

void
subscription_info_t::subscription_lost(
	const std::string & topic_name )
	{
		m_status = subscription_status_t::unsubscribed;
		m_failure_description.clear();

		for( const auto & p : m_postmans )
			p->subscription_unavailable( topic_name );
	}

void
subscription_info_t::subscription_failed(
	const std::string & topic_name,
	const std::string & description )
	{
		m_status = subscription_status_t::failed;
		m_failure_description = description;

		for( const auto & p : m_postmans )
			p->subscription_failed( topic_name, description );
	}

bool
subscription_info_t::has_postmans() const
	{
		return !m_postmans.empty();
	}

void
subscription_info_t::add_postman(
	const std::string & topic_name,
	const postman_shared_ptr_t & postman )
	{
		if( subscription_status_t::subscribed == m_status )
			postman->subscription_available( topic_name );
		else if( subscription_status_t::failed == m_status )
			postman->subscription_failed( topic_name, m_failure_description );

		// If there is no any exception after status setup the postman
		// can be stored in postmans set.
		m_postmans.insert( postman );
	}

void
subscription_info_t::remove_postman( const postman_shared_ptr_t & postman )
	{
		m_postmans.erase( postman );
	}

void
subscription_info_t::deliver_message(
	const std::string & topic,
	const std::string & payload )
	{
		for( const auto & p : m_postmans )
			p->post( topic, payload );
	}

//
// make_mosq_instance
//
mosquitto_unique_ptr_t
make_mosq_instance(
	const std::string & client_id,
	a_transport_manager_t * callback_param )
	{
		auto m = mosquitto_new( client_id.c_str(), true, callback_param );
		ensure_with_explblock< ex_t >( m,
			[]{ return fmt::format( "mosquitto_new failed, errno: {}", errno ); } );

		mosquitto_unique_ptr_t retval{ m, &mosquitto_destroy };

		return retval;
	}

} /* namespace details */

using namespace details;

//
// a_transport_manager_t
//
a_transport_manager_t::a_transport_manager_t(
	context_t ctx,
	lib_initializer_t & /*lib_initializer*/,
	connection_params_t connection_params,
	std::shared_ptr< spdlog::logger > logger )
	:	so_5::agent_t{ ctx }
	,	m_connection_params( std::move(connection_params) )
	,	m_self_mbox{ so_environment().create_mbox() }
	,	m_logger{ std::move(logger) }
	,	m_mosq{
			make_mosq_instance(
				m_connection_params.m_client_id, this ) }
	{
		setup_mosq_callbacks();
	}

void
a_transport_manager_t::so_define_agent()
	{
		st_working
			.event( m_self_mbox, &a_transport_manager_t::on_subscribe_topic )
			.event( m_self_mbox, &a_transport_manager_t::on_unsubscribe_topic )
			.event( m_self_mbox, &a_transport_manager_t::on_message_received,
					so_5::thread_safe );

		st_disconnected
			.on_enter( [this] {
					// Everyone should be informed that connection lost.
					so_5::send< broker_disconnected_t >( m_self_mbox );
				} )
			.event< connected_t >(
				m_self_mbox, &a_transport_manager_t::on_connected );

		st_connected
			.on_enter( [this] {
					// Everyone should be informed that connection established.
					so_5::send< broker_connected_t >( m_self_mbox );
					// All registered subscriptions must be restored.
					restore_subscriptions_on_reconnect();
				} )
			.on_exit( [this] {
					// All subscriptions are lost.
					drop_subscription_statuses();
					// No more pending subscriptions.
					m_pending_subscriptions.clear();
				} )
			.event< disconnected_t >(
				m_self_mbox, &a_transport_manager_t::on_disconnected )
			.event( m_self_mbox, &a_transport_manager_t::on_subscription_result )
			.event( m_self_mbox, &a_transport_manager_t::on_publish_message,
					so_5::thread_safe )
			.event( &a_transport_manager_t::on_pending_subscriptions_timer );
	}

void
a_transport_manager_t::so_evt_start()
	{
		// mosquitto event loop must be started.
		ensure_mosq_success(
				mosquitto_loop_start( m_mosq.get() ),
				[]{ return "mosquitto_loop_start failed"; } );

		this >>= st_disconnected;

		// Initiate connection to broker.
		ensure_mosq_success(
				mosquitto_connect_async(
						m_mosq.get(),
						m_connection_params.m_host.c_str(),
						static_cast< int >(m_connection_params.m_port),
						static_cast< int >(m_connection_params.m_keepalive) ),
				[&]{ return fmt::format(
						"mosquitto_connect_async({}, {}, {}) failed",
						m_connection_params.m_host,
						m_connection_params.m_port,
						m_connection_params.m_keepalive ); } );

		m_pending_subscriptions_timer =
			so_5::send_periodic< pending_subscriptions_timer_t >( *this,
					std::chrono::seconds{1},
					std::chrono::seconds{1} );
	}

void
a_transport_manager_t::so_evt_finish()
	{
		// mosquitto event-loop must be stopped here!
		if( st_connected == so_current_state() )
			// Because there is a connection it must be gracefully closed.
			ensure_mosq_success(
					mosquitto_disconnect( m_mosq.get() ),
					[]{ return "mosquitto_disconnect failed"; } );

		ensure_mosq_success(
				mosquitto_loop_stop( m_mosq.get(), true ),
				[]{ return "mosquitto_loop_stop failed"; } );
	}

instance_t
a_transport_manager_t::instance() const
	{
		return instance_t{ so_environment(), m_self_mbox };
	}

void
a_transport_manager_t::mqtt_will_set(
	const std::string & topic_name,
	const std::string & payload,
	int qos,
	bool retain )
	{
		ensure_mosq_success(
				mosquitto_will_set( m_mosq.get(),
						topic_name.c_str(),
//FIXME: there could be utility class mqtt_payload_size with checking
//the size of payload (this value cannot exces 268,435,455 bytes).
						static_cast< int >( payload.size() ),
						payload.data(),
						qos,
						retain ),
				[&] { return fmt::format(
						"mosquitto_will_set({}, {} bytes) failed",
						topic_name,
						payload.size() ); } );

	}

void
a_transport_manager_t::set_subscription_timeout(
	std::chrono::steady_clock::duration timeout )
	{
		m_subscription_timeout = timeout;
	}

void
a_transport_manager_t::setup_mosq_callbacks()
	{
		mosquitto_log_callback_set(
				m_mosq.get(),
				&a_transport_manager_t::on_log_callback );

		mosquitto_connect_callback_set(
				m_mosq.get(),
				&a_transport_manager_t::on_connect_callback );
		mosquitto_disconnect_callback_set(
				m_mosq.get(),
				&a_transport_manager_t::on_disconnect_callback );
		mosquitto_subscribe_callback_set(
				m_mosq.get(),
				&a_transport_manager_t::on_subscribe_callback );
		mosquitto_message_callback_set(
				m_mosq.get(),
				&a_transport_manager_t::on_message_callback );
	}

void
a_transport_manager_t::on_connect_callback(
	mosquitto *,
	void * this_object,
	int connect_result )
	{
		auto tm = reinterpret_cast< a_transport_manager_t * >(this_object);

		tm->m_logger->info( "on_connect, rc={}/{}",
				connect_result,
				mosquitto_connack_string( connect_result ) );

		if( 0 == connect_result )
			so_5::send< connected_t >( tm->m_self_mbox );
	}

void
a_transport_manager_t::on_disconnect_callback(
	mosquitto *,
	void * this_object,
	int disconnect_result )
	{
		auto tm = reinterpret_cast< a_transport_manager_t * >(this_object);

		tm->m_logger->info( "on_disconnect, rc={}", disconnect_result );

		if( 0 != disconnect_result )
			so_5::send< disconnected_t >( tm->m_self_mbox );
	}

void
a_transport_manager_t::on_subscribe_callback(
	mosquitto *,
	void * this_object,
	int mid,
	int qos_count,
	const int * qos_items )
	{
		auto tm = reinterpret_cast< a_transport_manager_t * >(this_object);
		if( qos_count )
			{
				tm->m_logger->trace( "on_subscribe, mid={}, qos_count={}",
						mid, qos_count );

				so_5::send< subscription_result_t >( tm->m_self_mbox,
						mid,
						std::vector< int >( qos_items, qos_items + qos_count ) );
			}
		else
			tm->m_logger->warn( "on_subscribe, qos_count is zero, mid={}", mid );
	}

void
a_transport_manager_t::on_message_callback(
	mosquitto *,
	void * this_object,
	const mosquitto_message * msg )
	{
		auto tm = reinterpret_cast< a_transport_manager_t * >(this_object);

		tm->m_logger->trace( "on_message, topic={}, payloadlen={}, qos={}"
				", retain={}",
				msg->topic, msg->payloadlen, msg->qos, msg->retain );

		so_5::send< message_received_t >( tm->m_self_mbox, *msg );
	}

void
a_transport_manager_t::on_log_callback(
	mosquitto *,
	void * this_object,
	int log_level,
	const char * log_msg )
	{
		static const char log_line_format[] = "[libmosquitto] {}";

		auto tm = reinterpret_cast< a_transport_manager_t * >(this_object);
		auto & logger = *(tm->m_logger);

		switch( log_level )
			{
			case MOSQ_LOG_ERR :
				logger.error( log_line_format, log_msg ); break;
			case MOSQ_LOG_WARNING :
			case MOSQ_LOG_NOTICE :
				logger.warn( log_line_format, log_msg ); break;
			case MOSQ_LOG_INFO :
				logger.info( log_line_format, log_msg ); break;
			case MOSQ_LOG_DEBUG :
				logger.debug( log_line_format, log_msg ); break;
			}
	}

void
a_transport_manager_t::on_connected()
	{
		this >>= st_connected;
	}

void
a_transport_manager_t::on_disconnected()
	{
		this >>= st_disconnected;
	}

void
a_transport_manager_t::on_subscribe_topic(
	const subscribe_topic_t & cmd )
	{
		m_logger->debug( "add topic postman, topic={}, postman={}",
				cmd.m_topic_name, cmd.m_postman );

		auto & info = m_registered_subscriptions[ cmd.m_topic_name ];
		info.add_postman( cmd.m_topic_name, cmd.m_postman );
		if( subscription_status_t::new_subscription == info.status() )
		{
			m_delivery_map.insert( cmd.m_topic_name, &info );
			try_subscribe_topic( cmd.m_topic_name );
		}
	}

void
a_transport_manager_t::on_unsubscribe_topic(
	const unsubscribe_topic_t & cmd )
	{
		m_logger->debug( "remove topic postman, topic={}, postman={}",
				cmd.m_topic_name, cmd.m_postman );

		auto ittopic = m_registered_subscriptions.find( cmd.m_topic_name );
		if( ittopic != m_registered_subscriptions.end() )
			{
				ittopic->second.remove_postman( cmd.m_postman );
				if( !ittopic->second.has_postmans() )
					{
						m_delivery_map.erase( cmd.m_topic_name, &(ittopic->second) );
						m_registered_subscriptions.erase( ittopic );

						m_logger->info( "topic unsubscription, topic={}",
								cmd.m_topic_name );

						auto r = mosquitto_unsubscribe( m_mosq.get(), 0,
								cmd.m_topic_name.c_str() );
						// If there is an error we can't do something reasonable.
						// Just log it and ignore.
						if( r != MOSQ_ERR_SUCCESS )
							m_logger->warn( "mosquitto_unsubscribe failed, "
									"topic={}, rc={}",
									cmd.m_topic_name, r );
					}
			}
		else
			m_logger->warn( "topic for unsubscription is not registered, "
					"topic={}", cmd.m_topic_name );
	}

void
a_transport_manager_t::on_subscription_result(
	const subscription_result_t & cmd )
	{
		auto itpending = m_pending_subscriptions.find( cmd.m_mid );
		if( itpending != m_pending_subscriptions.end() )
		{
			auto ittopic = m_registered_subscriptions.find(
					itpending->second.m_topic_name );
			if( ittopic != m_registered_subscriptions.end() )
			{
				m_logger->debug( "subscription_result: mid={}, topic={}, "
						"granted_qos={}",
						cmd.m_mid,
						itpending->second.m_topic_name,
						cmd.m_granted_qos.front() );

				process_subscription_result(
						itpending->second.m_topic_name,
						ittopic->second,
						cmd.m_granted_qos.front() );
			}
			else
				m_logger->warn( "unknown topic for subscription_result, "
						"mid={}, topic={}",
						cmd.m_mid,
						itpending->second.m_topic_name );

			m_pending_subscriptions.erase( itpending );
		}
		else
			m_logger->warn( "unknown mid in subscription_result_t, mid={}",
					cmd.m_mid );
	}

void
a_transport_manager_t::on_pending_subscriptions_timer(
	mhood_t< pending_subscriptions_timer_t > )
	{
		if( m_pending_subscriptions.empty() )
			return;

		const auto now = std::chrono::steady_clock::now();
		for( auto it = m_pending_subscriptions.begin();
				it != m_pending_subscriptions.end(); )
			{
				auto & pending = it->second;
				if( now - pending.m_initiated_at > m_subscription_timeout )
					{
						auto itdel = it++;
						m_logger->error( "subscription timed out, topic={}",
								pending.m_topic_name );

						auto ittopic = m_registered_subscriptions.find(
								pending.m_topic_name );
						ittopic->second.subscription_failed(
								pending.m_topic_name,
								"subscription timed out" );

						m_pending_subscriptions.erase( itdel );
					}
				else
					++it;
			}
	}

void
a_transport_manager_t::on_message_received(
	const message_received_t & cmd )
	{
		auto subscribers = m_delivery_map.match( cmd.m_topic );
		if( !subscribers.empty() )
		{
			for( auto * s : subscribers )
				s->deliver_message( cmd.m_topic, cmd.m_payload );
		}
		else
			m_logger->warn( "message for unregistered topic, topic={}, "
					"payloadlen={}",
					cmd.m_topic, cmd.m_payload.size() );
	}

void
a_transport_manager_t::on_publish_message(
	const publish_message_t & cmd )
	{
		m_logger->debug( "message publish, topic={}, "
				"payloadlen={}",
				cmd.m_topic_name, cmd.m_payload.size() );

		auto r = mosquitto_publish( m_mosq.get(), 0 /* mid */,
				cmd.m_topic_name.c_str(),
				static_cast< int >(cmd.m_payload.size()),
				cmd.m_payload.data(),
				qos_to_use,
				false /* retain */ );

		// If error just log it and ignore.
		if( MOSQ_ERR_SUCCESS != r )
				m_logger->warn( "message_publish failed, rc={}, topic={}, "
						"payloadlen={}",
						r, cmd.m_topic_name, cmd.m_payload.size() );
	}

void
a_transport_manager_t::try_subscribe_topic(
	const std::string & topic_name )
	{
		if( st_connected == so_current_state() )
			do_subscription_actions( topic_name );
	}

void
a_transport_manager_t::do_subscription_actions(
	const std::string & topic_name )
	{
		int mid{};

		m_logger->info( "topic subscription, topic={}", topic_name );

		auto r = mosquitto_subscribe( m_mosq.get(),
				&mid, topic_name.c_str(), qos_to_use );
		ensure_with_explblock< ex_t >(
				MOSQ_ERR_SUCCESS == r ||
				MOSQ_ERR_NO_CONN == r ||
				MOSQ_ERR_CONN_LOST == r,
				[&]{ return fmt::format( "mosquitto_subscribe({}, {}) "
						"failed, rc={}", topic_name, qos_to_use, r ); } );

		m_pending_subscriptions[ mid ] = pending_subscription_t{
				topic_name,
				std::chrono::steady_clock::now() };
	}

void
a_transport_manager_t::process_subscription_result(
	const std::string & topic_name,
	subscription_info_t & info,
	int granted_qos )
	{
		if( qos_to_use == granted_qos )
			info.subscription_created( topic_name );
		else
			{
				m_logger->error( "unexpected qos, topic_filter={}, granted_qos={}",
						topic_name, granted_qos );

				info.subscription_failed(
						topic_name,
						fmt::format( "unexpected qos: {}", granted_qos ) );
			}
	}

void
a_transport_manager_t::drop_subscription_statuses()
	{
		for( auto & info : m_registered_subscriptions )
			info.second.subscription_lost( info.first );
	}

void
a_transport_manager_t::restore_subscriptions_on_reconnect()
	{
		for( auto & info : m_registered_subscriptions )
			do_subscription_actions( info.first );
	}

} /* namespace mosquitto_transport */

