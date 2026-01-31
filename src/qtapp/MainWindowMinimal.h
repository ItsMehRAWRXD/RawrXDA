#pragma once

class MainWindowMinimal : public void
{

public:
    explicit MainWindowMinimal(void *parent = nullptr);
    ~MainWindowMinimal();

private:
    void newFile();
    void openFile();
};
