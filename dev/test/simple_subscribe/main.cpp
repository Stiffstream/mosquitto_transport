#include <iostream>

#include <mosquitto_transport/a_transport_manager.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

struct dummy_postman_t : public mosquitto_transport::postman_t
	{
		virtual void
		subscription_available( const std::string & topic_name ) override
			{
				std::cout << "[" << topic_name << "]: available" << std::endl;
			}

		virtual void
		subscription_unavailable( const std::string & topic_name ) override
			{
				std::cout << "[" << topic_name << "]: unavailable" << std::endl;
			}

		virtual void
		post( std::string topic, std::string payload ) override
			{
				std::cout << "[" << topic << "]: " << payload << std::endl;
			}
	};

void
do_test()
{
	mosquitto_transport::lib_initializer_t mosq_lib;

	so_5::launch( [&mosq_lib]( auto & env ) {
		env.introduce_coop( [&mosq_lib]( so_5::coop_t & coop ) {
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
			tm->mqtt_will_set( "clients/statuses/offline", "test-client" );

			auto instance = tm->instance();

			struct stop : so_5::signal_t {};

			auto client = coop.define_agent();
			client.on_start( [client]{
					so_5::send_delayed< stop >( client, 10s );
				} );
			client.event< broker_connected_t >(
				instance.mbox(), [instance] {
					std::cout << "connection established" << std::endl;
					so_5::send< subscribe_topic_t >( instance.mbox(),
						"clients/test-client/cmds",
						std::make_shared< dummy_postman_t >() );
				} );
			client.event< broker_disconnected_t >(
				instance.mbox(), [] {
					std::cout << "connection lost" << std::endl;
				} );
			client.event< stop >(
				client, [&coop] { coop.deregister_normally(); } );
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

