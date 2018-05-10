# What Is It?

_mosquitto_transport_ is an experiment of writing SObjectizer-based wrapper around [mosquitto](https://mosquitto.org/) library.

# Obtaining And Building

To use _mosquitto_transport_ it is necessary to have:

* C++14 compiler (gcc 5.4 or above, clang 3.8 or above);
* [Mxx_ru](https://sourceforge.net/projects/mxxru/) 1.6.13 or above;
* Boost-1.58 or above.

## MxxRu::externals Recipes

There is an example of recipes for MxxRu::externals:

~~~{.rb}
MxxRu::arch_externals :mosquitto_transport do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/mosquitto_transport-0.6/get/v.0.6.1.tar.bz2'

  e.map_dir 'dev/mosquitto_transport' => 'dev'
end

MxxRu::arch_externals :libmosquitto do |e|
  e.url 'https://github.com/eao197/mosquitto/archive/v1.4.8-p1.tar.gz'

  e.map_dir 'lib' => 'dev/libmosquitto'
  e.map_file 'config.h' => 'dev/libmosquitto/*'
end

MxxRu::arch_externals :libmosquitto_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/libmosquitto_mxxru_1.1/get/v.1.1.0.tar.bz2'

  e.map_file 'dev/libmosquitto/prj.rb' => 'dev/libmosquitto/*'
end

MxxRu::arch_externals :fmt do |e|
  e.url 'https://github.com/fmtlib/fmt/archive/4.1.0.zip'

  e.map_dir 'fmt' => 'dev/fmt'
end

MxxRu::arch_externals :fmtlib_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/fmtlib_mxxru-0.1/get/v.0.1.0.tar.bz2'

  e.map_dir 'dev/fmt_mxxru' => 'dev'
end

MxxRu::arch_externals :spdlog do |e|
  e.url 'https://github.com/gabime/spdlog/archive/v0.16.3.zip'

  e.map_dir 'include' => 'dev/spdlog'
end

MxxRu::arch_externals :spdlog_mxxru do |e|
  e.url 'https://bitbucket.org/sobjectizerteam/spdlog_mxxru-1.2/get/v.1.2.0.tar.bz2'

  e.map_dir 'dev/spdlog_mxxru' => 'dev'
end

MxxRu::arch_externals :so5 do |e|
  e.url 'https://sourceforge.net/projects/sobjectizer/files/sobjectizer/SObjectizer%20Core%20v.5.5/so-5.5.22.tar.xz'

  e.map_dir 'dev/so_5' => 'dev'
  e.map_dir 'dev/timertt' => 'dev'
end

MxxRu::arch_externals :catch do |e|
  e.url 'https://github.com/catchorg/Catch2/archive/v2.2.2.tar.gz'

  e.map_file 'single_include/catch.hpp' => 'dev/catch/*'
end
~~~

# License

_mosquitto_transport_ is distributed under [BSD-3-Clause](http://spdx.org/licenses/BSD-3-Clause.html) license.  See LICENSE file for more information.

# How To Use

## mosquitto Library Initialization And Deinitialization

To initialize and deinitialize mosquitto library correctly an instance of `lib_initializer_t` must be created. This instance must live for all time while mosquitto-transport is used in application. A reference to that instance must be passed to the constructor of every `a_transport_manager_t` agent.

For example:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
int main()
{
  mosqt::lib_initializer mosq_init;
  so_5::launch( [&]( so_5::environment_t & env ) {
    ...
  } );
}
~~~~~

## Creation Of Transport Manager

Every client connection to MQTT broker is under control of `a_transport_manager_t` agent. Because of that an agent of this type must be created.

**Note.** Agent `a_transport_manager_t` uses thread-safe event handler so the `adv_thread_pool` dispatcher can be used for that agent.

**Note.** It is better to place `a_transport_manager_t` instance to separate cooperation.

After creation of `a_transport_manager_t` a value of `instance_t` must be obtained from it. This value will be used for message publishing and retrieving.

A very simple example:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
mosqt::instance_t make_transport(so_5::environment_t & env, mosqt::lib_initializer_t & mosq_init)
{
  mosqt::instance_t instance;
  env.introduce_coop( [&](so_5::coop_t & coop) {
    auto tm = coop.make_agent< mosqt::a_transport_manager_t >(
      // A reference to mosquitto library initializer.
      std::ref{mosq_init},
      // Broker connection params.
      mosqt::connection_params_t{
        // ID for client on broker.
        "my-clien-name",
        // Broker host.
        "localhost",
        // Broker port.
        1883,
        // Keepalive timer in seconds.
        30 },
      // Logger instance to be used.
      spdlog::stdout_logger_mt("mosqt") );
    instance = tm->instance();
  } );
  return instance;
}
~~~~~

## Message Encoding and Decoding Principles

mosquitto_transport library supports automatic message encoding and decoding.
But it requires some help from application code.

To encode message into some format (binary, JSON, XML and so on) there should
be a partial or full specialization of
`mosquitto_transport::encoder_t<ENCODER_TAG,MSG>` template. For example it
could looks like:

~~~~~{.cpp}
struct json_encoding {}; // Will be used as tag type.

namespace mosquitto_transport {

// Partial specialization for all message types.
template< typename MSG >
struct encoder_t< json_encoding, MSG >
{
  // Actual encoding method.
  static std::string encode( const MSG & what )
  {
    // Assume every MSG has `to_json` method.
    return what.to_json();
  }
};

}
~~~~~

Similary for decoding a message from some format there should be a partial of
full specialization of `mosquitto_transport::decoder_t<DECODER_TAG,MSG>`
template. Something like that:

~~~~~{.cpp}
struct json_encoding {}; // Will be used as tag type.

namespace mosquitto_transport {

// Partial specialization for all message types.
template< typename MSG >
struct decoder_t< json_encoding, MSG >
{
  // Actual decoding method.
  static MSG decode( const std::string & payload )
  {
    // Assume every MSG has `from_json` method.
    MSG m;
    m.from_json(payload);
    return m;
  }
};

}
~~~~~

After that some useful typedefs could be defined:

~~~~~{.cpp}
using topic_subscriber = mosquitto_transport::topic_subscriber_t< json_encoding >;
using topic_publisher = mosquitto_transport::topic_publisher_t< json_encoding >;
~~~~~

## Message Publishing 

There is just one way for publishing messages in v.0.3.

It requires usage of `mosquitto_transport::publisher_t` template class and its
method `publish`:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;

mosqt::instance_t instance = ...; // Value returned by a_transport_manager_t agent.

// Publish instance of status_update_t message into "clients/statuses/updates" topic.
// Type json_encoding means that there is an appropriate specialization
// for mosquitto_transport::encoder_t template.
mosqt::topic_publisher_t< json_encoding >::publish(
  instance, // Transport manager to be used.
  "clients/statuses/updates", // Topic for message.
  status_update_t{...} ); // Message to be published.
~~~~~

A call to `topic_publisher_t<TAG>::publish` can be shortened if there is an
alias like that:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
using topic_publisher = mosqt::topic_publisher_t< json_encoding >;
...
topic_publisher::publish(
  instance, // Transport manager to be used.
  "clients/statuses/updates", // Topic for message.
  status_update_t{...} ); // Message to be published.
~~~~~

**Attention.** *All messages are published with QoS=0.*

## Message Subscription

To receive messages for a topic it is necessary to create a subscription from
some special mbox. This mbox is created automatically and managed to
mosquitto\_transport.

To create subscription it is necessary to use template class
`mosquitto_transport::topic_subscriber_t<DECODER_TAG>` and its template method
subscribe:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
class status_receiver_t : public so_5::agent_t
{
  mosqt::instance_t m_transport;
  ...
public :
  status_receiver_t(context_t ctx, mosqt::instance_t transport)
    : so_5::agent_t{ctx}
    , m_transport{std::move(transport)}
  {}
  virtual void so_define_agent() override
  {
    mosqt::topic_subscriber_t< json_encoding >::subscribe(
      m_transpor, // Transport manager to be used for subscription.
      "clients/statuses/updates", // Topic to be subscribed.
      // Lambda to do agent subscription actions.
      [this]( so_5::mbox_t mbox ) {
        so_default_state().event( mbox, &status_receiver_t::on_status_update );
        st_finishing.event( mbox, &status_receiver_t::on_status_update );
        ...
      } );
    ...
  }
  ...
private :
  // Message will be delivered as mosquitto_transport::incoming_message_t.
  void on_status_update(const mosqt::incoming_message_t< json_encoding > & cmd)
  {
    // cmd contains topic name and payload.
    // Actual message object should be extracted manually.
    const status_update_t upd = cmd.decode< status_update_t >();
  }
};
~~~~~

A call to `topic_subscriber_t<TAG>::subscribe` can be shortened if there is an
alias like that:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
using topic_subscriber = mosqt::topic_subscriber_t< json_encoding >;
...
topic_subscriber::subscribe(
  m_transpor, // Transport manager to be used for subscription.
  "clients/statuses/updates", // Topic to be subscribed.
  // Lambda to do agent subscription actions.
  [this]( so_5::mbox_t mbox ) {
    so_default_state().event( mbox, &status_receiver_t::on_status_update );
    st_finishing.event( mbox, &status_receiver_t::on_status_update );
    ...
  } );
~~~~~

**Note.** There is also short name `msg_type` which can be used for
simplification of incoming messages handling:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
using topic_subscriber = mosqt::topic_subscriber_t< json_encoding >;
...
void status_receiver_t::on_status_update(const topic_subscriber::msg_type & cmd)
{
  const auto & upd = cmd.decode< status_update_t >();
}
~~~~~

### Supscriptions With Wildcards In Topic Filters

Since v.0.3 there is a possibility to subscribe to several topics by using
wildcards `+` and `#` in topic filters. It could be done usual way:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
using topic_subscriber = mosqt::topic_subscriber_t< json_encoding >;
...
void status_listener_t::so_define_agent() override
{
	topic_subscriber::subscribe(
		m_transpor, // Transport manager to be used for subscription.
		"client/+/status/updates", // Topic filter to be subscribed.
		// Lambda to do agent subscription actions.
		[this]( const so_5::mbox_t & mbox ) {
			so_subscribe( mbox ).event( &status_listener_t::on_status_update );
			...
		} );
}
~~~~~

*Note.* When agent is subscribed to topic filter with wildcards it will
receive actual topic name in `incoming_message_t`. For example if agent is
subscribed to `client/+/status/updates` and there is a message from
topic `client/1/status/updates` then name `client/1/status/updates` will
be in `incoming_message_t::topic_name`.

### Subscription Availability And Unavailability Notifications

Since v.0.3 there are notifications about subscriptions availability and
unavailability.

Message `mosquitto_transport::subscription_available_t` is sent when
subscription to topic is completed successfully.

Message `mosquitto_transport::subscription_unavailable_t` is send when
connection to MQTT broker is lost.

Method `topic_name()` of these messages will return the original topic
filter which was passed to `subscribe` method. For example, if topic filter
`/sport/results/#` was used then this value will be returned by
`topic_name()` methods.

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
using topic_subscriber = mosqt::topic_subscriber_t< json_encoding >;
...
void status_listener_t::so_define_agent() override
{
	topic_subscriber::subscribe(
		m_transpor, // Transport manager to be used for subscription.
		"client/+/status/updates", // Topic filter to be subscribed.
		// Lambda to do agent subscription actions.
		[this]( const so_5::mbox_t & mbox ) {
			so_subscribe( mbox )
				.event( &status_listener_t::on_topic_subscribed )
				.event( &status_listener_t::on_topic_lost );

			so_subscribe( mbox ).event( &status_listener_t::on_status_update );
			...
		} );
}

void status_listener_t::on_topic_subscribed(
	const mosqt::subscription_available_t & cmd )
{
	// cmd.topic_name() will contain "client/+/status/updates" name.
	...
}

void status_listener_t::on_topic_lost(
	const mosqt::subscription_unavailable_t & cmd )
{
	// cmd.topic_name() will contain "client/+/status/updates" name.
	...
}
~~~~~

**Note.** Because MQTT allows delivery of messages from subscribed topics
before completion of subscription operation it is possible to receve some
messages from subscribed topics before arival of `subscription_available_t`
message.

*Note.* Message `subscription_unavailable_t` can be sent before
`subscription_available_t`. It is possible in situation when new subscription
is not finished but a connection to broker is lost.

### Subscription Failure

Subscription is an asynchronous process. The subscription result could be
received after some time. And this result could be negative.

If the case of subscription failure an exception is thrown on the context
of `transport_manager` agent. This exeption usually leads to abortion
of the whole application.

There is a way to specify another reaction for subscription failure.
Method `topic_subscriber_t::subscriber` can receive yet another parameter.
If this parameter has value `mosquitto_transport::notify_on_failure` then
a special message will be sent instead on throwing an exception:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
using topic_subscriber = mosqt::topic_subscriber_t< json_encoding >;
...
void status_listener_t::so_define_agent() override
{
	topic_subscriber::subscribe(
		m_transpor, // Transport manager to be used for subscription.
		"client/+/status/updates", // Topic filter to be subscribed.
		// Lambda to do agent subscription actions.
		[this]( const so_5::mbox_t & mbox ) {
			so_subscribe( mbox )
				.event( &status_listener_t::on_topic_subscribed )
				.event( &status_listener_t::on_topic_lost )
				.event( &status_listener_t::on_topic_failed );

			so_subscribe( mbox ).event( &status_listener_t::on_status_update );
			...
		},
		// Send subscription_failed_t message instead on throwing an exception.
		mosqt::notify_on_failure );
}

void status_listener_t::on_topic_failed(
	const mosqt::subscription_failed_t & cmd )
{
	// cmd.topic_name() will contain "client/+/status/updates" name.
	// cmd.description() will contain description of the failure.
	...
}
~~~~~

### Subscription Timeout

There is a time limit for subscription operation. If `SUBACK` response is not received during specified
timeout then the whole subscription operation will be considered as failed. And appropriate reaction will
be performed (see above).

Subscription timeout is 60 seconds by default. It can be changed by `set_subscription_timeout` method
of `transport_manager`. This method must be called before registration of `transport_manager` registration:

~~~~~{.cpp}
using namespace std::chrono_literals;

namespace mosqt = mosquitto_transport;
mosqt::instance_t make_transport(so_5::environment_t & env, mosqt::lib_initializer_t & mosq_init)
{
  mosqt::instance_t instance;
  env.introduce_coop( [&](so_5::coop_t & coop) {
    auto tm = coop.make_agent< mosqt::a_transport_manager_t >(
      std::ref{mosq_init},
      mosqt::connection_params_t{"my-clien-id", "localhost", 1883, 30 },
      spdlog::stdout_logger_mt("mosqt") );
    // Setting the timeout for subscription operation.
    tm->set_subscription_timeout( 15s ); // Wait no more than 15 seconds.
    instance = tm->instance();
  } );
  return instance;
}
~~~~~

## Broker Connection And Disconnection Notifications

There are `mosquitto_transport::broker_connected_t` and
`mosquitto_transport::broker_disconnected_t` signals. They can be used to
detection of connection status. Something like:

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
...
class client_t : public so_5::agent_t
{
  mosqt::instance_t m_transport;
  ...
public :
  client_t(context_t ctx, mosqt::instance_t transport)
    : so_5::agent_t{ctx}
    , m_transport{std::move(transport)}
  {}
  virtual void so_define_agent() override
  {
    so_subscribe( m_transport.mbox() )
      .event< mosqt::broker_connected_t >( &client_t::on_connected )
      .event< mosqt::broker_disconnected_t >( &client_t::on_disconnected );
    ...
  }
...
};
~~~~~

## Setting The Will

MQTT will can be set by `a_transport_manager_t::mqtt_will_set()` method. Please note that this method must be called before the registration of transport manager agent!

~~~~~{.cpp}
namespace mosqt = mosquitto_transport;
mosqt::instance_t make_transport(so_5::environment_t & env, mosqt::lib_initializer_t & mosq_init)
{
  mosqt::instance_t instance;
  env.introduce_coop( [&](so_5::coop_t & coop) {
    auto tm = coop.make_agent< mosqt::a_transport_manager_t >(
      std::ref{mosq_init},
      mosqt::connection_params_t{"my-clien-id", "localhost", 1883, 30 },
      spdlog::stdout_logger_mt("mosqt") );
    // Setting the will for the client.
    tm->mqtt_will_set(
      "clients/statuses/offline", // Topic for the will.
      "my-client-id" ); // Payload of the will message.
    instance = tm->instance();
  } );
  return instance;
}
~~~~~
