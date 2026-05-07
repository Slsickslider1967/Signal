#pragma once

#include "HandlerShared.h"

class RackContextMenu
{
    public:
        static void Show();
        static void Close();

    private:
        void CreateContextMenu();
        bool RemoveRack();
        void VoltageRange(Rack &rack);
        void Input();
};