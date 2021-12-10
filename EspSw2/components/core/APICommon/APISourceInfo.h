// API Source Info
// Rob Dobson 2021

#pragma once

class APISourceInfo
{
public:
    APISourceInfo(uint32_t channelID)
    {
        this->channelID = channelID;
    }
    uint32_t channelID;
};
