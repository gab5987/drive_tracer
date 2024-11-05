#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <async_mqtt5.hpp>
#include <async_mqtt5/types.hpp>

#include <config.h>
#include <util.h>

class mqtt_inst
{
    enum class state : std::uint8_t
    {
        run,
        terminate,
    };

    const std::string _topic;

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

    void force_finalize();

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

mqtt_inst::error mqtt_inst::publish(std::string &message)
{
    return this->_txd.enqueue(message) ? error::ok : error::queue_full;
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

        std::string data;

        const bool deq = this->_txd.dequeue(data);
        if (deq)
        {
            using namespace async_mqtt5;

            this->_client.async_publish<qos_e::at_most_once>(
                this->_topic, std::move(data), retain_e::yes, publish_props{},
                [](error_code err) {});
        }
    }
}

mqtt_inst::mqtt_inst(
    boost::asio::io_context &ioc, const std::string &topic) noexcept
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
    this->_client.async_disconnect(boost::asio::detached);
    this->_state = state::terminate;

    this->_thread.join();
}

int main()
{
    boost::asio::io_context ioc;

    std::shared_ptr<mqtt_inst> mqtt =
        std::make_shared<mqtt_inst>(ioc, tracer_config::TOPIC);

    std::thread thr{[&]() {
        std::this_thread::sleep_for(
            std::chrono::duration(std::chrono::milliseconds(1000)));

        std::string msg = "Queued Message";
        mqtt->publish(msg);
        mqtt->force_finalize();
    }};

    ioc.run();

    return 0;
}
