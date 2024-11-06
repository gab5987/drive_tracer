#include <chrono>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <async_mqtt5.hpp>
#include <async_mqtt5/types.hpp>

#include <config.h>
#include <util.h>

class mqtt_inst
{
    // Modified completion token that will prevent co_await from throwing
    // exceptions.
    static constexpr auto USE_NOTHROW_AWAITABLE =
        boost::asio::as_tuple(boost::asio::use_awaitable);

    enum class state : std::uint8_t
    {
        run,
        terminate,
    };

    const std::string _topic;

    async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> _client;

    util::queue<std::string, 5> _rxd;
    util::queue<std::string, 5> _txd;

    std::mutex _critical_section;

    std::thread _thread;

    state _state{state::run};

    boost::asio::awaitable<void> loop();

    boost::asio::awaitable<bool> subscribe();
    boost::asio::awaitable<void> subscribe_and_receive();

    public:
    enum class error
    {
        ok,
        no_data,
        queue_full,
    };

    void force_finalize();

    error receive(std::string &message);
    error publish(std::string &message);

    explicit mqtt_inst(
        boost::asio::io_context &ioc, const std::string &topic) noexcept;
    ~mqtt_inst();
};

void mqtt_inst::force_finalize()
{
    this->_client.async_disconnect(boost::asio::detached);
    this->_state = state::terminate;
}

boost::asio::awaitable<bool> mqtt_inst::subscribe()
{
    using namespace async_mqtt5;
    subscribe_options opts = {
        // All messages will arrive at QoS 2.
        qos_e::exactly_once,
        // Forward message from Clients with same ID.
        no_local_e::no,
        // Keep the original RETAIN flag.
        retain_as_published_e::retain,
        // Send retained messages when the subscription is established.
        retain_handling_e::send,
    };

    // Configure the request to subscribe to a Topic.
    const subscribe_topic sub_topic{this->_topic, opts};

    // Subscribe to a single Topic.
    auto &&[ec, sub_codes, sub_props] = co_await this->_client.async_subscribe(
        sub_topic, async_mqtt5::subscribe_props{}, USE_NOTHROW_AWAITABLE);

    if (ec)
    {
        std::cout << "Subscribe error occurred: " << ec.message() << std::endl;
    }
    else
    {
        std::cout << "Result of subscribe request: " << sub_codes[0].message()
                  << std::endl;
    }

    co_return !ec &&
        !sub_codes[0]; // True if the subscription was successfully established.
}

boost::asio::awaitable<void> mqtt_inst::subscribe_and_receive()
{
    // Before attempting to receive an Application Message from the Topic we
    // just subscribed to, it is advisable to verify that the subscription
    // succeeded. It is not recommended to call mqtt_client::async_receive if
    // you do not have any subscription established as the corresponding handler
    // will never be invoked.
    const bool fsub = co_await this->subscribe();
    if (!fsub)
    {
        co_return;
    }

    for (;;)
    {
        // Receive an Appplication Message from the subscribed Topic(s).
        auto &&[ec, topic, payload, publish_props] =
            co_await this->_client.async_receive(USE_NOTHROW_AWAITABLE);

        if (ec == async_mqtt5::client::error::session_expired)
        {
            // The Client has reconnected, and the prior session has expired.
            // As a result, any previous subscriptions have been lost and must
            // be reinstated.
            const bool fsub = co_await this->subscribe();
            if (fsub)
            {
                continue;
            }
            break;
        }

        if (ec)
        {
            break;
        }

        std::cout << "Received message from the Broker" << std::endl;
        std::cout << "\t topic: " << topic << std::endl;
        std::cout << "\t payload: " << payload << std::endl;
    }

    co_return;
}

mqtt_inst::error mqtt_inst::publish(std::string &message)
{
    return this->_txd.enqueue(message) ? error::ok : error::queue_full;
}

mqtt_inst::error mqtt_inst::receive(std::string &message)
{
    const bool result = this->_rxd.dequeue(message);
    return result ? error::ok : error::no_data;
}

boost::asio::awaitable<void> mqtt_inst::loop()
{
    co_return;
    for (;;)
    {
        // TODO: Implement thread dequeueing
        if (this->_state != state::run)
        {
            break;
        }

        std::string data;

        //     using namespace async_mqtt5;

        //     this->_client.async_publish<qos_e::at_most_once>(
        //         this->_topic, std::move(data), retain_e::yes,
        //         publish_props{},
        //         [](error_code err) {});
    }
}

mqtt_inst::mqtt_inst(
    boost::asio::io_context &ioc, const std::string &topic) noexcept
    : _topic(topic), _client(ioc)
{
    this->_client.brokers(tracer_config::BROKER_URL, tracer_config::BROKER_PORT)
        .async_run(boost::asio::detached);

    co_spawn(ioc, this->subscribe_and_receive(), boost::asio::detached);
}

mqtt_inst::~mqtt_inst()
{
    this->_client.async_disconnect(boost::asio::detached);
    // this->_state = state::terminate;

    // this->_thread.join();
}

int main()
{
    boost::asio::io_context ioc;

    std::shared_ptr<mqtt_inst> mqtt =
        std::make_shared<mqtt_inst>(ioc, tracer_config::TOPIC);

    // std::thread thr{[&]() {
    //     using namespace std::chrono;

    //     std::this_thread::sleep_for(duration(milliseconds(1000)));

    //     std::string msg = "Queued Message";
    //     mqtt->publish(msg);

    //     std::this_thread::sleep_for(duration(milliseconds(500)));
    //     mqtt->force_finalize();
    // }};

    ioc.run();

    return 0;
}
