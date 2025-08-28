// my_app.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "app.hpp"
#include "app.cpp"
//#include "capture_device.cpp"

// define our application
App app;

int main()
{
    try {
        if (app.init())
            return app.run();
    }
    catch (std::exception const& e) {
        std::cerr << "App failed : " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}


