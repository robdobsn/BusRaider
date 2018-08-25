
#include "McBase.h"
#include "McManager.h"
#include "ee_printf.h"

class McTRS80 : public McBase
{
  public:

    McTRS80() : McBase()
    {
    }

    void handleDisplay()
    {
        ee_printf("Handle TRS80 display");
    }

};
