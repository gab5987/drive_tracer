#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>

#include <config.h>
#include <util.h>

class mqtt_inst
{
    enum class state : std::uint8_t
    {
        run,
        terminate,
    };

    const std::string_view _topic;

    async_mqtt5::mqtt_client<boost::asio::ip::tcp::socket> _client;

    util::queue<std::string, 5> _txd;

    std::thread _thread;

    state _state{state::run};

    void loop();

    public:
    enum class error
    {
        ok,
        queue_full,
    };

    error publish(std::string &message);

    explicit mqtt_inst(
        boost::asio::io_context &ioc, const std::string_view &topic) noexcept;
    ~mqtt_inst();
};

mqtt_inst::error mqtt_inst::publish(std::string &message)
{
    // TODO: This should not be handled here, rather beign in the loop.
    this->_client.async_publish<async_mqtt5::qos_e::at_most_once>(
        tracer_config::TOPIC, message, async_mqtt5::retain_e::yes,
        async_mqtt5::publish_props{}, [this](async_mqtt5::error_code ec) {
            std::cout << ec.message() << std::endl;
            this->_client.async_disconnect(boost::asio::detached);
        });

    this->_state = state::terminate;

    // TODO: Check errors
    return error::ok;
}

void mqtt_inst::loop()
{
    for (;;)
    {
        // TODO: Implement thread dequeueing
        if (this->_state != state::run)
        {
            break;
        }
    }
}

mqtt_inst::mqtt_inst(
    boost::asio::io_context &ioc, const std::string_view &topic) noexcept
    : _topic(topic), _client(ioc)
{
    this->_client.brokers(tracer_config::BROKER_URL, tracer_config::BROKER_PORT)
        .async_run(boost::asio::detached);

    std::function<void(mqtt_inst *)> handle = [](mqtt_inst *inst) -> void {
        inst->loop();
    };

    this->_thread = std::thread{handle, this};
    this->_thread.detach();
}

mqtt_inst::~mqtt_inst()
{
    this->_thread.join();
}

int main()
{
    boost::asio::io_context ioc;

    std::shared_ptr<mqtt_inst> mqtt =
        std::make_shared<mqtt_inst>(ioc, tracer_config::TOPIC);

    std::string msg = "Hello World";
    mqtt->publish(msg);

    ioc.run();
}
