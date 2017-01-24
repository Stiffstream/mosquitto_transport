#include <iostream>

#include <mosquitto_transport/a_transport_manager.hpp>

#include <so_5/all.hpp>

using namespace std::chrono_literals;

void
do_test()
{
	mosquitto_transport::lib_initializer_t mosq_lib;

	so_5::launch( [&mosq_lib]( auto & env ) {
		env.introduce_coop( [&mosq_lib]( so_5::coop_t & coop ) {
			using namespace mosquitto_transport;

			coop.make_agent< a_transport_manager_t >(
					std::ref(mosq_lib),
					connection_params_t{
							"test-client",
							"localhost",
							1883u,
							5u },
					spdlog::stdout_logger_mt( "mosqt" ) );

			struct stop : so_5::signal_t {};
			auto stopper = coop.define_agent();
			stopper.on_start( [stopper]{
					so_5::send_delayed< stop >( stopper, 15s );
				} );
			stopper.event< stop >(
				stopper, [&coop] { coop.deregister_normally(); } );
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

