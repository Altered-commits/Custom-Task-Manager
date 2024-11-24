#include <iostream>
#include "CTMBackend/ctm_app.h"

int main(void)
{
    CTMApp app;

    CTMAppErrorCodes code = app.Initialize();

    if(code != CTMAppErrorCodes::INIT_SUCCESS) {
        std::cerr << "Failed to intialize custom task manager. Error code: " << (int)code << '\n';
        return 1;
    }

    app.Run();

    return 0;
}