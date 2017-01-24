#define CATCH_CONFIG_MAIN
#include <catch/catch.hpp>

#include <mosquitto_transport/impl/subscriptions_map.hpp>

#include <iostream>
#include <sstream>
#include <set>

using namespace std;
using namespace std::string_literals;

using namespace mosquitto_transport;
using namespace mosquitto_transport::impl;

class dummy_postman_t;
using postman_shptr_t = shared_ptr< dummy_postman_t >;

class dummy_postman_t
{
	string m_name;
public :
	dummy_postman_t( string name ) : m_name{ move(name) } {}

	const string & name() const { return m_name; }

	static postman_shptr_t
	make( string name ) { return make_shared< dummy_postman_t >(move(name)); }
};

vector< string > mk_actual( vector< postman_shptr_t > postmans )
{
	vector< string > r; r.reserve( postmans.size() );
	for( const auto & p : postmans )
		r.push_back( p->name() );
	sort( begin(r), end(r) );

	return r;
}

vector< string > mk_expected( vector< string > v )
{
	sort( begin(v), end(v) );
	return v;
}

TEST_CASE( "Simple insert/match", "simple_insert_match" )
{
	subscriptions_map_t< postman_shptr_t > map;
	map.insert( "a", dummy_postman_t::make( "[a]" ) );
	map.insert( "/", dummy_postman_t::make( "[/]" ) );
	map.insert( "a/", dummy_postman_t::make( "[a/]" ) );
	map.insert( "/a", dummy_postman_t::make( "[/a]" ) );

	REQUIRE( mk_expected({}) == mk_actual( map.match("b") ) );
	REQUIRE( mk_expected({}) == mk_actual( map.match("/b") ) );
	REQUIRE( mk_expected({}) == mk_actual( map.match("b/") ) );

	REQUIRE( mk_expected({}) == mk_actual( map.match("a/b") ) );
	REQUIRE( mk_expected({}) == mk_actual( map.match("a//b") ) );

	REQUIRE( mk_expected({"[/]"}) == mk_actual( map.match("/") ) );
	REQUIRE( mk_expected({"[a]"}) == mk_actual( map.match("a") ) );
	REQUIRE( mk_expected({"[a/]"}) == mk_actual( map.match("a/") ) );
	REQUIRE( mk_expected({"[/a]"}) == mk_actual( map.match("/a") ) );
}

TEST_CASE( "Simple insert/match/remove", "simple_insert_match_remove" )
{
	subscriptions_map_t< postman_shptr_t > map;
	map.insert( "a", dummy_postman_t::make( "[a]" ) );
	map.insert( "/", dummy_postman_t::make( "[/]" ) );

	auto p1 = dummy_postman_t::make( "[a/]" );
	map.insert( "a/", p1 );

	REQUIRE( mk_expected({"[/]"}) == mk_actual( map.match("/") ) );
	REQUIRE( mk_expected({"[a]"}) == mk_actual( map.match("a") ) );
	REQUIRE( mk_expected({"[a/]"}) == mk_actual( map.match("a/") ) );

	map.erase( "a/", p1 );

	REQUIRE( mk_expected({}) == mk_actual( map.match("a/") ) );

	auto p2 = dummy_postman_t::make( "<a/>" );
	map.insert( "a/", p2 );

	REQUIRE( mk_expected({"<a/>"}) == mk_actual( map.match("a/") ) );

	map.insert( "a/", p1 );

	REQUIRE( mk_expected({"<a/>", "[a/]"}) == mk_actual( map.match("a/") ) );
}

TEST_CASE( "Cases from mosquitto", "some_mosquitto_cases" )
{
	auto do_check =
		[](const string & filter, const string & name, bool must_match ) {
			subscriptions_map_t< postman_shptr_t > map;
			map.insert( filter, dummy_postman_t::make(filter) );
			if( must_match )
				REQUIRE( mk_expected({filter}) == mk_actual(map.match(name)) );
			else
				REQUIRE( mk_expected({}) == mk_actual(map.match(name)) );
		};

	do_check("foo/bar", "foo/bar", true);
	do_check("foo/+", "foo/bar", true);
	do_check("foo/+/baz", "foo/bar/baz", true);
	do_check("foo/+/#", "foo/bar/baz", true);
	do_check("#", "foo/bar/baz", true);

	do_check("foo/bar", "foo", false);
	do_check("foo/+", "foo/bar/baz", false);
	do_check("foo/+/baz", "foo/bar/bar", false);
	do_check("foo/+/#", "fo2/bar/baz", false);

	do_check("#", "/foo/bar", true);
	do_check("/#", "/foo/bar", true);
	do_check("/#", "foo/bar", false);

	do_check("foo//bar", "foo//bar", true);
	do_check("foo//+", "foo//bar", true);
	do_check("foo/+/+/baz", "foo///baz", true);
	do_check("foo/bar/+", "foo/bar/", true);

	do_check("foo/#", "foo", true);
	do_check("foo/#", "foo/", true);

	do_check("#", "foo", true);
	do_check("#", "foo/", true);
	do_check("#", "/foo/", true);

	do_check("/#", "foo", false);
	do_check("/#", "foo/", false);
	do_check("/#", "/foo", true);
	do_check("/#", "/", true);
	do_check("/#", "//", true);

	do_check("foo/+", "foo/", true);
	do_check("foo/+", "foo/a", true);
	do_check("foo/+", "foo", false);
	do_check("foo/+", "foo/a/", false);
	do_check("foo/+", "foo/a/b", false);
}


TEST_CASE( "Advanced insert/match/remove", "adv_insert_match_remove" )
{
	subscriptions_map_t< postman_shptr_t > map;
	auto make_and_insert = [&map]( const std::string & n ) {
		auto p = dummy_postman_t::make( n );
		map.insert( n, p );
		return p;
	};

	auto p1 = make_and_insert( "foo/+/+/baz" );
	auto p2 = make_and_insert( "foo/+/+/#" );
	auto p3 = make_and_insert( "foo/+/+/+" );
	auto p4 = make_and_insert( "foo/+/#" );
	auto p5 = make_and_insert( "foo/#" );
	auto p6 = make_and_insert( "#" );


	REQUIRE( mk_expected({"#", "foo/#"}) == mk_actual( map.match("foo") ) );
	REQUIRE( mk_expected({"#", "foo/#", "foo/+/#"})
			== mk_actual( map.match("foo/") ) );

	REQUIRE( mk_expected({
				p1->name(), p2->name(),
				p3->name(), p4->name(),
				p5->name(), p6->name()})
			== mk_actual( map.match("foo/1/2/baz") ) );

	map.erase( "foo/+/+/+", p3 );
	REQUIRE( mk_expected({
				p1->name(), p2->name(),
				/*p3->name(),*/ p4->name(),
				p5->name(), p6->name()})
			== mk_actual( map.match("foo/1/2/baz") ) );

	map.erase( "foo/+/+/baz", p1 );
	REQUIRE( mk_expected({
				/*p1->name(),*/ p2->name(),
				/*p3->name(),*/ p4->name(),
				p5->name(), p6->name()})
			== mk_actual( map.match("foo/1/2/baz") ) );

	map.erase( "foo/+/#", p4 );
	REQUIRE( mk_expected({
				/*p1->name(),*/ p2->name(),
				/*p3->name(),*/ /*p4->name(),*/
				p5->name(), p6->name()})
			== mk_actual( map.match("foo/1/2/baz") ) );

	map.erase( "#", p6 );
	REQUIRE( mk_expected({
				/*p1->name(),*/ p2->name(),
				/*p3->name(),*/ /*p4->name(),*/
				p5->name()/*, p6->name()*/ })
			== mk_actual( map.match("foo/1/2/baz") ) );

	REQUIRE( mk_expected({"foo/#"}) == mk_actual( map.match("foo") ) );
	REQUIRE( mk_expected({"foo/#"}) == mk_actual( map.match("foo/") ) );
}
