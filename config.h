#ifndef TRACER_CONFIG_H
#define TRACER_CONFIG_H

namespace tracer_config
{
    static constexpr char BROKER_URL[] = "mqtt.eclipseprojects.io";
    static constexpr int  BROKER_PORT  = 1883;

    // Randomically generated using this website:
    // https://www.random.org/strings/?num=10&len=10&digits=on&upperalpha=on
    // &loweralpha=on&unique=on&format=html&rnd=new
    static constexpr char TOPIC[] = "o4Yx1y7emP/QZCOTkFg0n";
}; // namespace tracer_config

#endif
