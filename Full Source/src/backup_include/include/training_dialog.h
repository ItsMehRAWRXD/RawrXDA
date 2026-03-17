#pragma once

// C++20 / Win32. Training config dialog; no Qt. JSON config + callback.

#include <string>
#include <functional>

class ModelTrainer;

class TrainingDialog
{
public:
    using TrainingStartRequestedFn = std::function<void(const std::string& configJson)>;
    using TrainingCancelledFn      = std::function<void()>;

    explicit TrainingDialog(ModelTrainer* trainer);
    void setOnTrainingStartRequested(TrainingStartRequestedFn f) { m_onStartRequested = std::move(f); }
    void setOnTrainingCancelled(TrainingCancelledFn f)            { m_onCancelled = std::move(f); }
    void initialize();

    std::string getTrainingConfig() const;

    void showModal(void* parent = nullptr);
    void* getDialogHandle() const { return m_dialogHandle; }

private:
    void setupUI();
    void onStartTraining();
    void onCancel();
    bool validateConfiguration() const;
    std::string detectDatasetFormat(const std::string& path) const;

    void* m_dialogHandle = nullptr;
    ModelTrainer* m_trainer = nullptr;
    TrainingStartRequestedFn m_onStartRequested;
    TrainingCancelledFn      m_onCancelled;
};
