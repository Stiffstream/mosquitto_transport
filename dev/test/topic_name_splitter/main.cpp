#define CATCH_CONFIG_MAIN
#include <catch/catch.hpp>

#include <mosquitto_transport/impl/fragments_extractor.hpp>

#include <iostream>
#include <sstream>

using namespace std;
using namespace std::string_literals;

using namespace mosquitto_transport;
using namespace mosquitto_transport::impl;

splitted_topic_name_t mk_actual( const std::string & topic )
{
	return split_topic_name( topic );
}

splitted_topic_name_t mk_expected( splitted_topic_name_t name )
{
	return name;
}

TEST_CASE( "Check topic_name splitting", "splitting_test" )
{
	REQUIRE( mk_actual( "a" ) == mk_expected({ "a" }));

	REQUIRE( mk_actual( "a"s ) == mk_expected({ "a" }));

	REQUIRE( mk_actual( "/a"s ) == mk_expected({ "", "a" }));

	REQUIRE( mk_actual( "/"s ) == mk_expected({ "", "" }));

	REQUIRE( mk_actual( "a/"s ) == mk_expected({ "a", "" }));

	REQUIRE( mk_actual( "sport/+"s ) == mk_expected({ "sport", "+" }));

	REQUIRE( mk_actual( "sport/+/"s ) == mk_expected({ "sport", "+", "" }));

	REQUIRE( mk_actual( "sport/+/+"s ) == mk_expected({ "sport", "+", "+" }));

	REQUIRE( mk_actual( "///"s ) == mk_expected({ "", "", "", "" }));

	REQUIRE( mk_actual( "///a"s ) == mk_expected({ "", "", "", "a" }));

	REQUIRE( mk_actual( "///a/"s ) == mk_expected({ "", "", "", "a", "" }));
}

