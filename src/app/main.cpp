#include "Application.h"

int main()
{
    Application app;
    if (app.Initialize())
        app.Run();
    app.Shutdown();
}