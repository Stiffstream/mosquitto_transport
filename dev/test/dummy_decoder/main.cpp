#include <iostream>

#include <mosquitto_transport/a_transport_manager.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

struct dummy_decoder {};

namespace mosquitto_transport
{

template< typename MSG >
struct decoder_t< dummy_decoder, MSG >
	{
		static MSG decode( const std::string & payload )
			{
				return MSG{ "[dummy=" + payload + "=dummy]" };
			}
	};

template< typename MSG >
struct encoder_t< dummy_decoder, MSG >
	{
		static std::string encode( const MSG & what )
			{
				return what.to_string();
			}
	};

};

namespace mosqt = mosquitto_transport;

using topic_subscriber = mosqt::topic_subscriber_t< dummy_decoder >;
using topic_publisher = mosqt::topic_publisher_t< dummy_decoder >;

struct hello_message
{
	const std::string m_greeting;

	hello_message( std::string s ) : m_greeting{ std::move(s) } {}

	std::string to_string() const { return m_greeting; }
};

class a_client_t : public so_5::agent_t
{
	struct dereg_itself : public so_5::signal_t {};

public :
	a_client_t(
		context_t ctx,
		mosqt::instance_t transport,
		unsigned int working_time )
		:	so_5::agent_t{ ctx }
		,	m_transport{ std::move(transport) }
		,	m_working_time{ working_time }
	{}

	virtual void
	so_define_agent() override
	{
		topic_subscriber::subscribe(
			m_transport,
			"clients/test-client/cmds",
			[this]( const so_5::mbox_t & mbox ) {
				so_subscribe( mbox ).event( &a_client_t::on_hello_message );
				so_subscribe( mbox ).event( &a_client_t::on_topic_available );
				so_subscribe( mbox ).event( &a_client_t::on_topic_lost );
			} );

		so_subscribe( m_transport.mbox() )
			.event( &a_client_t::on_broker_connected )
			.event( &a_client_t::on_broker_disconnected );

		so_subscribe_self()
			.event( &a_client_t::on_dereg_itself );
	}

	virtual void
	so_evt_start() override
	{
		so_5::send_delayed< dereg_itself >( *this,
				std::chrono::seconds{ m_working_time } );
	}

private :
	const mosqt::instance_t m_transport;

	const unsigned int m_working_time;

	void
	on_hello_message( const topic_subscriber::msg_type & cmd )
	{
		const auto & m = cmd.decode< hello_message >();
		std::cout << cmd.topic_name() << " => " << m.m_greeting << std::endl;
	}

	void
	on_topic_available( const mosqt::subscription_available_t & cmd )
	{
		std::cout << cmd.topic_name() << ": subscribed!" << std::endl;

		topic_publisher::publish(
				m_transport,
				"clients/test-client/cmds",
				hello_message{ "hello-" + std::to_string( m_working_time ) } );
	}

	void
	on_topic_lost( const mosqt::subscription_unavailable_t & cmd )
	{
		std::cout << cmd.topic_name() << ": lost!" << std::endl;
	}

	void
	on_broker_connected( mhood_t< mosqt::broker_connected_t > )
	{
		std::cout << "there is a connection to broker" << std::endl;
	}

	void
	on_broker_disconnected( mhood_t< mosqt::broker_disconnected_t > )
	{
		std::cout << "there is no connection to broker" << std::endl;
	}

	void
	on_dereg_itself( mhood_t< dereg_itself > )
	{
		so_deregister_agent_coop_normally();
	}
};

class a_cmd_listener_t : public so_5::agent_t
{
public :
	a_cmd_listener_t(
		context_t ctx,
		mosqt::instance_t transport )
		:	so_5::agent_t{ ctx }
		,	m_transport{ std::move(transport) }
	{}

	virtual void
	so_define_agent() override
	{
		topic_subscriber::subscribe(
			m_transport,
			"clients/+/cmds",
			[this]( const so_5::mbox_t & mbox ) {
				so_subscribe( mbox ).event( &a_cmd_listener_t::on_hello_message );
				so_subscribe( mbox ).event( &a_cmd_listener_t::on_topic_available );
				so_subscribe( mbox ).event( &a_cmd_listener_t::on_topic_lost );
				so_subscribe( mbox ).event( &a_cmd_listener_t::on_subscribe_failed );
			},
			mosqt::notify_on_failure );
	}

private :
	const mosqt::instance_t m_transport;

	void
	on_hello_message( const topic_subscriber::msg_type & cmd )
	{
		const auto & m = cmd.decode< hello_message >();
		std::cout << cmd.topic_name() << " => " << m.m_greeting << std::endl;
	}

	void
	on_topic_available( const mosqt::subscription_available_t & cmd )
	{
		std::cout << cmd.topic_name() << ": subscribed!" << std::endl;
	}

	void
	on_topic_lost( const mosqt::subscription_unavailable_t & cmd )
	{
		std::cout << cmd.topic_name() << ": lost!" << std::endl;
	}

	void
	on_subscribe_failed( const mosqt::subscription_failed_t & cmd )
	{
		std::cout << cmd.topic_name() << ": failure='" << cmd.description()
			<< std::endl;
	}
};

void
do_test()
{
	mosquitto_transport::lib_initializer_t mosq_lib;

	so_5::launch( [&mosq_lib]( auto & env ) {
		mosqt::instance_t transport;

		env.introduce_coop( [&]( so_5::coop_t & coop ) {
			using namespace mosquitto_transport;

			auto logger = spdlog::stdout_logger_mt( "mosqt" );
			logger->set_level( spdlog::level::trace );

			auto tm = coop.make_agent< a_transport_manager_t >(
					std::ref(mosq_lib),
					connection_params_t{
							"test-client",
							"localhost",
							1883u,
							5u },
					logger );
			transport = tm->instance();
		} );

		env.introduce_coop( [&]( so_5::coop_t & coop ) {
			coop.make_agent< a_client_t >( transport, 10 );
		} );

		env.introduce_coop( [&transport]( so_5::coop_t & coop ) {
			coop.make_agent< a_client_t >( transport, 15 );
		} );

		env.introduce_coop( [&]( so_5::coop_t & coop ) {
			coop.make_agent< a_cmd_listener_t >( transport );
		} );

	} );
}

int main()
{
	try
	{
		do_test();
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Oops! " << ex.what() << std::endl;
	}
}

