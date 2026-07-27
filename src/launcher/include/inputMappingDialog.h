#ifndef INPUT_MAPPING_DIALOG_H
#define INPUT_MAPPING_DIALOG_H

#include "common.h"

#include <QDialog>

class QByteArray;
class QMoveEvent;
class QSettings;
class QWidget;

class Configuration;
namespace Ui {
class InputMappingDialog;
} // namespace Ui

// Lets the user rebind which keyboard key / physical controller button triggers each PS5 pad
// button, entirely from the Qt launcher -- no CLI flags required to use the feature (the
// emulator-side --keyboard-map/--controller-map flags this writes into Configuration still
// exist and can be set manually, but are no longer the only way in).
//
// Serializes to Configuration::keyboard_button_map / controller_button_map using the exact same
// "host_code:pad_button,..." text format as Libs::Controller::ParseInputBindingList/
// SerializeInputBindingList in the emulator (src/libs/controller.h), so the two ends never need
// to agree on anything beyond that shared text format.
class InputMappingDialog: public QDialog {
	Q_OBJECT
	KYTY_QT_CLASS_NO_COPY(InputMappingDialog);

public:
	explicit InputMappingDialog(Configuration& info, QWidget* parent = nullptr);
	~InputMappingDialog() override;

	static void WriteSettings(QSettings& s);
	static void ReadSettings(QSettings& s);

private:
	Ui::InputMappingDialog* m_ui = nullptr;
	Configuration&          m_info;

	void Init(const Configuration& info);
	void InitKeyboardTab(const QString& serialized_map);
	void InitControllerTab(const QString& serialized_map);

	void moveEvent(QMoveEvent* event) override;

	static QByteArray g_last_geometry;

	/*slots:*/
	void save();
	void reset_defaults();
};

#endif // INPUT_MAPPING_DIALOG_H
