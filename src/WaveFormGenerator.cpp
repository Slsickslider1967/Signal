#include <iostream>
#include "../include/Functions/Window.h"
#include "../include/WaveForm.h"
#include <list>


// --Main Variables--
std::list<WaveForm> WaveForms;

// --Function Prototypes--
int min();
void MakeWindow();

int main()
{
    std::cout << "Hello World!" << std::endl;
    MakeWindow();
    return 0;
}

void MakeWindow()
{
    Window::CreateWindow(800, 600, "Signal Generator");
}