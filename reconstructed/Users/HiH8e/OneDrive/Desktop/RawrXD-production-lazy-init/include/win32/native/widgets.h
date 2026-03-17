#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include "native_layout.h"

enum class NativeSizePolicy { Fixed, Expanding };
enum class NativeOrientation { Horizontal, Vertical };

class NativeButton : public NativeWidget {
public:
    explicit NativeButton(NativeWidget* parent = nullptr);
    void setText(const std::string& text);
    void setOnClick(std::function<void()> callback);

private:
    std::function<void()> m_onClick;
};

class NativeTextEditor : public NativeWidget {
public:
    explicit NativeTextEditor(NativeWidget* parent = nullptr);
    void setText(const std::string& text);
    std::string getText() const;
    void setBackgroundColor(const std::string& color);
    void setTextColor(const std::string& color);
    void setFont(const std::string& family, int size);
    void goToLineColumn(int line, int column);
};

class NativeComboBox : public NativeWidget {
public:
    explicit NativeComboBox(NativeWidget* parent = nullptr);
    void addItem(const std::string& text);
    void clear();
    int getSelectedIndex() const;
    void setOnChange(std::function<void(int)> callback);

private:
    std::function<void(int)> m_onChange;
};

class NativeSlider : public NativeWidget {
public:
    NativeSlider(NativeOrientation orient = NativeOrientation::Horizontal, NativeWidget* parent = nullptr);
    void setRange(int min, int max);
    void setValue(int value);
    int getValue() const;
    void setOnValueChanged(std::function<void(int)> callback);

private:
    NativeOrientation m_orientation;
    std::function<void(int)> m_onValueChanged;
};

class NativeLabel : public NativeWidget {
public:
    explicit NativeLabel(NativeWidget* parent = nullptr);
    void setText(const std::string& text);
};

class NativeSpinBox : public NativeWidget {
public:
    explicit NativeSpinBox(NativeWidget* parent = nullptr) : NativeWidget(parent) {}
    void setRange(int min, int max) { m_min = min; m_max = max; }
    void setValue(int v) { m_value = v; if (m_onValueChanged) m_onValueChanged(v); }
    int value() const { return m_value; }
    void setOnValueChanged(std::function<void(int)> cb) { m_onValueChanged = std::move(cb); }

private:
    int m_min = 0;
    int m_max = 100;
    int m_value = 0;
    std::function<void(int)> m_onValueChanged;
};

