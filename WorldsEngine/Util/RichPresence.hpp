#pragma once

namespace discord
{
    class Core;
}

namespace worlds
{
    extern discord::Core *discordCore;
    void initRichPresence(EngineInterfaces interfaces);
    void tickRichPresence();
    void shutdownRichPresence();
}
