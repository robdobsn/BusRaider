// Machine Interface
// Rob Dobson 2018

#pragma once

class MachineInterface
{
  private:
    String _cachedStatusJSON;

  public:

    MachineInterface()
    {
        _cachedStatusJSON = "{}";
    }

    const char* getStatus()
    {
        return _cachedStatusJSON.c_str();
    }

    void service()
    {

    }
    
    void handleRxFrame(const uint8_t *framebuffer, int framelength)
    {
        // Extract frame type
        if ((framelength < 0) || (framelength > 1500))
            return;

        // Ensure string is terminated
        ((char*)framebuffer)[framelength] = 0;

        // Get command
        String cmdName = RdJson::getString("cmdName", "", (const char*)framebuffer);
        if (cmdName.equalsIgnoreCase("statusUpdate"))
        {
            // Store the status frame
            _cachedStatusJSON = (const char*)framebuffer;            
        }
    }
};
