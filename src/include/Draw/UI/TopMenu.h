#pragma once 

class TopMenu
{
    public:
        static void Render();

    private:
        void CreateTopMenu();

        void RackMenu();
        void ModulesMenu();
        void RecordMenu();
        void FileMenu();
        void HelpMenu();
};